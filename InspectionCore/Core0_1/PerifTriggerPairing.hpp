// ---------------------------------------------------------------------------
// PERIF_CORE_PAIRING -- does the CORE work out which object each frame belongs
// to, or does the device?
//
// 1 (default): the core keeps its own trigger FIFO and clock model, pairs each
//   frame here, and names a tid in every report. The device cross-checks that
//   against its own timestamp match and reports agree/disagree.
//
// 0: the core pairs nothing. Reports carry cam_ts and tid -1, and the device
//   alone decides which object a frame belongs to.
//
// The migration is toward 0. Everything the core does here exists only because
// the device used to announce the trigger timestamp and then forget it, leaving
// the host to reconstruct the mapping from clocks it could only observe
// indirectly. The device now keeps that timestamp, so this is redundancy, and
// on 2026-08-05 it was measured to be WORSE than redundant: with the core in
// positional mode the pairing runs one part out of step -- miss_delta 12221us
// against a 12000us object spacing -- and it reproduces below the camera's
// frame-rate ceiling, so it is not a frame-loss effect. A skewed pairing puts
// one part's verdict on the next part.
//
// Kept switchable rather than deleted because turning it off is not free:
//
//   - agree/disagree, the evidence the report_match_ts promotion rests on,
//     needs both mechanisms running to exist at all.
//   - the device's clock CALIBRATION currently finds its object by tid
//     (CamClockSync::observe is reached via byTid), so with no tid the
//     bootstrap has nothing to match against. Calibration would have to select
//     "the one outstanding sync object" instead -- which it can, since the
//     one-at-a-time guard makes that unambiguous by construction, but it is a
//     firmware change and must land first.
//   - when the device's timestamp match finds nothing, tarP falls back to the
//     tid. With this at 0 there is no fallback and the frame simply is not
//     placed, which is the correct "stop rather than guess" behaviour but is a
//     behaviour change.
//
// So: do not set this to 0 until the firmware calibrates without a tid, and
// re-run the burst and real-part validation afterwards.
// ---------------------------------------------------------------------------
#ifndef PERIF_CORE_PAIRING
#define PERIF_CORE_PAIRING 1
#endif

// Which physical object does this camera frame belong to?
//
// Everything needed to answer that question lives in this one file, on purpose.
// The pairing is the single point where a wrong answer becomes a mis-sorted
// part, so it should be readable end to end, switchable at runtime, and
// removable without unpicking it from the inspection loop. wiringPanel.cpp
// touches it through six calls and knows nothing about how it works.
//
// ---------------------------------------------------------------------------
// The problem
// ---------------------------------------------------------------------------
// The uInspESP32 fires the camera over a wire and announces the object id over
// a 115200-baud serial link. Two streams, same order, wildly different latency:
// announcements have been measured arriving up to 115ms AFTER the frame they
// describe.
//
// The original pairing was positional -- oldest unclaimed trigger owns the
// oldest frame. That is correct only while the two streams stay 1:1 forever,
// and they do not. One trigger that yields no frame (the camera silently
// refuses it while frame_id stays contiguous, so a frame-gap check cannot see
// it) offsets the FIFO permanently: every later frame is then reported against
// the object one position behind it. Measured on a real run -- 2596
// announcements, 2591 frames, a standing offset of 5, object 690's image
// reported as tid 685. Not late. Wrong. It only hid because every verdict was
// NA at the time; with real verdicts it mis-sorts every part.
//
// ---------------------------------------------------------------------------
// The fix
// ---------------------------------------------------------------------------
// Both clocks are already on the wire and neither was used: the device stamps
// each announcement with esp_timer_get_time() (dev_us) and the camera stamps
// each frame with its own clock (timeStamp_us). They share no epoch, but their
// DIFFERENCE is near-constant -- crystal drift moves it by tens of microseconds
// per second, nothing else does.
//
// So estimate that offset, then match on evidence: the trigger this frame
// belongs to is the one whose dev_us lands closest to (frame_ts - offset). A
// lost frame now costs exactly the one part it belongs to; the trigger it
// orphaned is retired as NA by the staleness sweep and nothing after it shifts.
//
// Bootstrapping is the awkward part -- with no offset there is nothing to match
// on. So the first frames pair positionally and are used only to MEASURE the
// offset, and timestamp matching engages once enough samples agree. If they
// never agree, it stays positional and says so, which is strictly the old
// behaviour rather than a confident wrong answer.
//
// ---------------------------------------------------------------------------
// Excluding it
// ---------------------------------------------------------------------------
// setMode(POSITIONAL) restores the old algorithm exactly -- same queue, same
// pop-the-front, same everything -- with no other code path changing. That is
// the intended A/B: run the same parts both ways and compare. Deleting the
// include and the six call sites removes it entirely.

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <deque>
#include <mutex>
#include <vector>
#include <algorithm>
#include <math.h>
#include <chrono>

// One announced camera trigger, waiting for the frame it caused.
struct PerifTrigger
{
  int64_t  tid        = -1;   // object id the verdict must be reported against
  uint64_t dev_us     = 0;    // device clock at trigger (esp_timer_get_time)
  int      tidx       = 1;    // which camera branch announced it
  uint32_t gate_pulse = 0;    // plate position at registration
  int      qs         = -1;   // device pipeline depth when it was announced
  uint64_t arrival_ms = 0;    // host clock when WE saw it -- drives staleness
  // A clock-sync pulse, not a part. The device fires the camera directly
  // (trig_cam_pulse) with no pipeline object behind it and announces it with
  // gate_pulse == 0. It is worth pairing -- that is the whole point, it carries
  // both timestamps -- but reporting a verdict against it would name an object
  // the device does not have, which faults it with INSP_RESULT_MATCHES_NO_OBJECT.
  bool sync_only = false;
};

class PerifTriggerPairing
{
public:
  enum Mode { POSITIONAL = 0, TIMESTAMP = 1 };

  enum PairResult {
    PAIRED,        // tid_out is the object this frame belongs to
    PAIRED_SYNC,   // matched a clock-sync pulse: model updated, nothing to report
    EMPTY,         // nothing announced yet -- caller may wait, it may still arrive
    NO_CANDIDATE   // announcements exist but none is plausibly this frame
  };

  // Tolerance on |dev_us - (frame_ts - offset)|. Real jitter between the
  // trigger edge and the sensor's own timestamp is well under a millisecond;
  // 5ms leaves room for clock granularity while staying far below the part
  // spacing the gate limiter allows (25 parts/s -> 40ms), so the nearest
  // trigger is never ambiguous.
  static constexpr int64_t DEFAULT_TOL_US = 5000;

  // Offset samples that must agree before timestamp matching takes over. Small
  // enough to engage within the first second of a run, large enough that a
  // single mispaired bootstrap frame cannot set the offset on its own.
  static constexpr int BOOTSTRAP_N = 8;

  // Consecutive unmatched frames that mean "the clock model is stale", not
  // "these parts were lost". Low enough to recover within a fraction of a
  // second at production rates; high enough that a genuine burst of lost frames
  // does not throw away a good offset.
  static constexpr int RESYNC_AFTER = 5;

  // How long a measured offset stays trustworthy with nothing to refresh it.
  //
  // Only a successful match feeds the drift EWMA, so an idle line freezes the
  // estimate while the two crystals keep separating -- ~22ms of drift was
  // observed across a single run, against a 5ms tolerance. 30s at that rate is
  // roughly half the tolerance, so the first frame after a pause still lands
  // inside the window if the offset is fresh, and is treated as unmeasured if
  // it is not.
  //
  // Without this the recovery is failure-driven: the offset stays "valid" until
  // five frames in a row fail to match, and with the early-dump those five are
  // discarded before inspection and never reported. Restarting after a break
  // therefore burned five parts and, at unanswered_stop_after=2, faulted the
  // machine before it had judged anything. Expiring on TIME instead costs
  // nothing: bootstrap pairs positionally and still reports every frame.
  static constexpr int64_t OFFSET_TTL_MS = 30000;

  explicit PerifTriggerPairing(size_t cap = 256) : _cap(cap) {}

  void setMode(Mode m)
  {
    std::lock_guard<std::mutex> lk(_mx);
    if (_mode == m) return;
    _mode = m;
    // A mode change invalidates nothing about the queue, but the offset was
    // learned under the old regime; make it re-earn confidence.
    _offset_valid = false;
    _boot.clear();
  }
  Mode mode() const { std::lock_guard<std::mutex> lk(_mx); return _mode; }
  void setToleranceUs(int64_t us) { std::lock_guard<std::mutex> lk(_mx); if (us > 0) _tol_us = us; }

  // Everything queued refers to objects the device no longer knows about
  // (it wiped its ring on fault / idle / clear_error). Keeping them would
  // report verdicts against ids that match nothing.
  void reset()
  {
    std::lock_guard<std::mutex> lk(_mx);
    _q.clear();
    // Deliberately keep the clock offset: the two crystals did not change
    // because the device faulted, and relearning it costs another bootstrap
    // window during which pairing is positional again.
  }

  void announce(const PerifTrigger &t)
  {
    std::lock_guard<std::mutex> lk(_mx);
    _rx++;
    if (_q.size() >= _cap)
    {
      // Backed up this far means results are not being produced at all. Drop
      // the oldest: stale ids would pair every later frame with the wrong part,
      // which is worse than losing parts already past the selector.
      _q.pop_front();
      _drops++;
    }
    _q.push_back(t);
  }

  // Claim the trigger that owns this frame. cam_ts_us is the camera's own
  // timestamp for the exposure; pass 0 if the layer does not provide one, and
  // the pairing degrades to positional for that frame.
  PairResult pairFrame(uint64_t cam_ts_us, int64_t *tid_out)
  {
    std::lock_guard<std::mutex> lk(_mx);
    _expireStaleOffset();
    if (_q.empty()) return EMPTY;

    if (_mode == POSITIONAL || cam_ts_us == 0)
    {
      bool sync = _q.front().sync_only;
      *tid_out = _q.front().tid;
      _q.pop_front();
      _matched++;
      _last_match_ms = _nowMs();
      return sync ? PAIRED_SYNC : PAIRED;
    }

    if (!_offset_valid)
    {
      // Bootstrap: pair positionally, but harvest the offset while we do. If
      // the FIFO is already skewed these samples disagree and we simply never
      // gain confidence -- which is the honest outcome, not a wrong offset.
      PerifTrigger t = _q.front();
      _q.pop_front();
      *tid_out = t.tid;
      const bool boot_sync = t.sync_only;
      _matched++;
      _last_match_ms = _nowMs();
      _boot.push_back((int64_t)cam_ts_us - (int64_t)t.dev_us);
      if ((int)_boot.size() >= BOOTSTRAP_N)
      {
        std::vector<int64_t> s = _boot;
        std::sort(s.begin(), s.end());
        int64_t med = s[s.size() / 2];
        int agree = 0;
        for (int64_t v : s) if (llabs(v - med) <= _tol_us) agree++;
        if (agree * 2 > (int)s.size())       // a real majority, not a plurality
        {
          _offset_us = (double)med;
          _offset_valid = true;
          _engaged_at_rx = _rx;
        }
        else
        {
          // Keep the most recent half and try again rather than starting cold:
          // early disagreement is usually the pre-existing FIFO skew flushing
          // out, and the newest samples are the trustworthy ones.
          _boot.erase(_boot.begin(), _boot.begin() + _boot.size() / 2);
        }
      }
      return boot_sync ? PAIRED_SYNC : PAIRED;
    }

    // Evidence. Nearest trigger to where this frame says it should be.
    int64_t want = (int64_t)cam_ts_us - (int64_t)llround(_offset_us);
    size_t best_i = 0;
    int64_t best_d = INT64_MAX;
    for (size_t i = 0; i < _q.size(); i++)
    {
      int64_t d = llabs((int64_t)_q[i].dev_us - want);
      if (d < best_d) { best_d = d; best_i = i; }
    }

    if (best_d > _tol_us)
    {
      // Nothing plausible. Do NOT fall back to popping the front -- that is
      // exactly the guess this class exists to stop making.
      //
      // Deliberately NOT counted here: the caller retries this every 2ms while
      // it waits out a late announcement, so counting per call would report ~75
      // failures for one frame that then paired fine. The caller calls
      // noteUnpaired() once, when it actually gives up.
      _last_miss_us = best_d;
      return NO_CANDIDATE;
    }

    *tid_out = _q[best_i].tid;
    const bool ts_sync = _q[best_i].sync_only;
    // Track drift. Only matched frames update the offset, so a mismatch can
    // never drag the estimate along with it.
    double resid = (double)((int64_t)cam_ts_us - (int64_t)_q[best_i].dev_us) - _offset_us;
    _offset_us += resid * 0.05;      // slow EWMA: crystals drift, they do not jump
    _last_resid_us = resid;
    if (fabs(resid) > _max_resid_us) _max_resid_us = fabs(resid);
    // Anything older than the match is an orphan -- its frame never arrived.
    // Leave it queued; the staleness sweep retires it as NA, which is what
    // makes a lost frame cost one part instead of every part after it.
    //
    // Counted once per out-of-order match, not once per position skipped: an
    // orphan sitting at the head is passed over by EVERY later match until the
    // sweep retires it, so summing positions reported 1994 "skips" for 308 real
    // orphans. The honest count of orphans is staleCount().
    if (best_i > 0) _out_of_order++;
    _q.erase(_q.begin() + best_i);
    _matched++;
    _ts_matched++;
    _consec_miss = 0;
    _last_match_ms = _nowMs();
    return ts_sync ? PAIRED_SYNC : PAIRED;
  }

  // Retire the n oldest triggers unconditionally. Only meaningful in
  // POSITIONAL mode, where a detected transmission gap of n frames means the n
  // triggers at the head produced no image and would otherwise shift every
  // later pairing. TIMESTAMP mode needs no such compensation -- an orphaned
  // trigger is simply never the nearest match and falls out via sweepStale --
  // and calling this there would throw away triggers that would have matched.
  size_t retireOldest(size_t n, std::vector<PerifTrigger> *out)
  {
    std::lock_guard<std::mutex> lk(_mx);
    size_t k = 0;
    while (k < n && !_q.empty())
    {
      if (out) out->push_back(_q.front());
      _q.pop_front();
      _stale++;
      k++;
    }
    return k;
  }

  // Highest object id we have actually reported. The device sweeps on our
  // behalf: reporting tid R marks every OLDER object still unjudged as SKIP,
  // which is a non-error state. So anything below R is already accounted for
  // and saying it again would only add an out-of-order report.
  void noteReported(int64_t tid)
  {
    std::lock_guard<std::mutex> lk(_mx);
    if (tid > _max_reported_tid) _max_reported_tid = tid;
  }

  // Triggers whose frame is never coming.
  //
  // Only the ones the device CANNOT have swept are handed back to be reported
  // NA. An orphan older than something we already reported was marked SKIP on
  // the device the moment that report landed -- it reaches the selector in a
  // non-error state without us saying anything, and an extra NA for it would be
  // an out-of-order report whose only effect is to sweep ITS predecessors.
  //
  // What that leaves is exactly the case the device cannot cover: orphans NEWER
  // than anything we have reported. Nothing will follow them, so nothing will
  // sweep them, and they reach the selector still unjudged -> err=2. That is
  // the tail of a run -- clear the plate and this is every part still in
  // flight. The timer exists for those and only those, which makes it the
  // counterpart of the device's SWITCH deadline rather than a second, competing
  // source of reports.
  size_t sweepStale(uint64_t now_ms, uint32_t stale_ms, std::vector<PerifTrigger> *out)
  {
    std::lock_guard<std::mutex> lk(_mx);
    size_t n = 0;
    while (!_q.empty())
    {
      const PerifTrigger &f = _q.front();
      if (f.arrival_ms == 0 || now_ms <= f.arrival_ms + stale_ms) break;
      if (f.sync_only) { /* no object behind it -- nothing to answer for */ }
      else if (f.tid > _max_reported_tid)
      {
        if (out) out->push_back(f);
        n++;
      }
      else _covered_by_skip++;
      _q.pop_front();
      _stale++;
    }
    return n;
  }

  // --- B: is this frame's announcement lost, or merely still in flight? ------
  //
  // The difference decides whether waiting (and inspecting) is worth anything.
  // If the queue already holds a trigger that fired AFTER this frame was
  // exposed, then announcements have overtaken it -- its own announcement is
  // not late, it is gone. Nothing will ever claim this frame, so inspecting it
  // is pure waste: a full-resolution inspect plus encode, thrown away.
  //
  // Only answerable with a valid offset; without one, every frame has to be
  // treated as maybe-pairable.
  bool announcementLost(uint64_t cam_ts_us) const
  {
    std::lock_guard<std::mutex> lk(_mx);
    // A stale offset makes every frame look lost, so never dump on one.
    if (_offsetExpired()) return false;
    if (_mode != TIMESTAMP || !_offset_valid || cam_ts_us == 0 || _q.empty()) return false;
    int64_t want = (int64_t)cam_ts_us - (int64_t)llround(_offset_us);
    for (const PerifTrigger &t : _q)
    {
      // A trigger this frame could still be. Not lost.
      if (llabs((int64_t)t.dev_us - want) <= _tol_us) return false;
      // A trigger that fired later than this frame: its announcement got here,
      // so an earlier one is not still in transit behind it.
      if ((int64_t)t.dev_us > want + _tol_us) return true;
    }
    return false;
  }

  // --- diagnostics ---------------------------------------------------------
  size_t   pending()      const { std::lock_guard<std::mutex> lk(_mx); return _q.size(); }
  bool     offsetValid()  const { std::lock_guard<std::mutex> lk(_mx); return _offset_valid; }
  double   offsetUs()     const { std::lock_guard<std::mutex> lk(_mx); return _offset_us; }
  double   lastResidUs()  const { std::lock_guard<std::mutex> lk(_mx); return _last_resid_us; }
  double   maxResidUs()   const { std::lock_guard<std::mutex> lk(_mx); return _max_resid_us; }
  int64_t  lastMissUs()   const { std::lock_guard<std::mutex> lk(_mx); return _last_miss_us; }
  long long rxCount()     const { std::lock_guard<std::mutex> lk(_mx); return _rx; }
  // Every frame given an object, by any route. tsMatched() is the subset that
  // was matched on evidence rather than on queue order -- the two were the same
  // number until matched() started counting the positional and bootstrap paths
  // too, which it had not, so a run pairing 455 frames reported matched:2.
  long long matched()     const { std::lock_guard<std::mutex> lk(_mx); return _matched; }
  long long tsMatched()   const { std::lock_guard<std::mutex> lk(_mx); return _ts_matched; }
  // How long since anything last paired. -1 = nothing has ever paired, so
  // there is no estimate to keep alive yet.
  int64_t lastMatchAgeMs() const
  {
    std::lock_guard<std::mutex> lk(_mx);
    return _last_match_ms == 0 ? -1 : (_nowMs() - _last_match_ms);
  }
  long long outOfOrder()  const { std::lock_guard<std::mutex> lk(_mx); return _out_of_order; }
  // Frames the caller gave up on, after any wait it chose to do.
  void noteUnpaired() { std::lock_guard<std::mutex> lk(_mx); _no_candidate++; _missStreak(); }
  long long staleCount()  const { std::lock_guard<std::mutex> lk(_mx); return _stale; }
  long long noCandidate() const { std::lock_guard<std::mutex> lk(_mx); return _no_candidate; }
  long long dropCount()   const { std::lock_guard<std::mutex> lk(_mx); return _drops; }
  // Times the clock model was thrown away and relearned.
  long long resyncs()     const { std::lock_guard<std::mutex> lk(_mx); return _resyncs; }
  // Orphans we stayed quiet about because the device had already swept them.
  long long coveredBySkip() const { std::lock_guard<std::mutex> lk(_mx); return _covered_by_skip; }
  // Frames dropped before inspection because their announcement was lost.
  long long dumped()      const { std::lock_guard<std::mutex> lk(_mx); return _dumped; }
  void noteDumped() { std::lock_guard<std::mutex> lk(_mx); _dumped++; _missStreak(); }

  // One line, because these numbers only mean anything together: a healthy run
  // is skipped~0 and maxResid small; skipped climbing means frames are being
  // lost (and the pairing is absorbing it correctly); maxResid climbing toward
  // the tolerance means the offset is drifting faster than the EWMA tracks.
  void formatStatus(char *buf, size_t n) const
  {
    std::lock_guard<std::mutex> lk(_mx);
    snprintf(buf, n,
      "pairing:%s%s off:%.1fms resid last:%.0fus max:%.0fus | "
      "rx:%lld matched:%lld(ts:%lld) ooo:%lld stale:%lld(skip:%lld) nocand:%lld "
      "dumped:%lld drops:%lld resync:%lld pend:%zu",
      _mode == TIMESTAMP ? "timestamp" : "positional",
      (_mode == TIMESTAMP && !_offset_valid) ? "(bootstrapping)" : "",
      _offset_us / 1000.0, _last_resid_us, _max_resid_us,
      _rx, _matched, _ts_matched, _out_of_order, _stale, _covered_by_skip, _no_candidate,
      _dumped, _drops, _resyncs, _q.size());
  }

private:
  // Relearn the clock offset when matching stops working.
  //
  // The offset drifts -- 22ms observed across a single run, against a 5ms
  // tolerance -- and only a SUCCESSFUL match feeds the EWMA that tracks it. So
  // any gap with no matches (the plate stopped, a shift change, a fault) leaves
  // the estimate stale, and once it is stale by more than the tolerance nothing
  // matches, which means nothing updates it, which means nothing ever matches
  // again. A wedge that never recovers.
  //
  // Measured the hard way: after a 5-minute idle the next run dumped all 13
  // frames as "announcement lost", reported nothing, and faulted on the second
  // part. The offset was not wrong by a little -- it was wrong by more than the
  // window, and had no path back.
  //
  // So a run of consecutive misses is treated as evidence about the MODEL, not
  // about the parts: drop back to bootstrapping, pair positionally for a few
  // frames, and re-measure. Positional is wrong in the ways this class exists
  // to fix, but it is wrong for a handful of parts rather than forever.
  static int64_t _nowMs()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  bool _offsetExpired() const
  {
    return _offset_valid && _last_match_ms != 0 &&
           (_nowMs() - _last_match_ms) > OFFSET_TTL_MS;
  }
  void _expireStaleOffset()
  {
    if (!_offsetExpired()) return;
    _offset_valid = false;
    _boot.clear();
    _consec_miss = 0;
    _resyncs++;
  }

  void _missStreak()
  {
    if (!_offset_valid) return;
    if (++_consec_miss < RESYNC_AFTER) return;
    _offset_valid = false;
    _boot.clear();
    _consec_miss = 0;
    _resyncs++;
  }

  mutable std::mutex   _mx;
  std::deque<PerifTrigger> _q;
  size_t               _cap;
  Mode                 _mode = POSITIONAL;
  int64_t              _tol_us = DEFAULT_TOL_US;

  bool                 _offset_valid = false;
  double               _offset_us = 0;
  std::vector<int64_t> _boot;
  long long            _engaged_at_rx = 0;

  double    _last_resid_us = 0, _max_resid_us = 0;
  int64_t   _last_miss_us = 0;
  long long _rx = 0, _matched = 0, _ts_matched = 0, _out_of_order = 0, _stale = 0,
            _no_candidate = 0, _drops = 0, _covered_by_skip = 0, _dumped = 0;
  int64_t   _max_reported_tid = -1;
  int       _consec_miss = 0;
  int64_t   _last_match_ms = 0;
  long long _resyncs = 0;
};
