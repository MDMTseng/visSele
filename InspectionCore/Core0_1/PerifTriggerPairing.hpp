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

// One announced camera trigger, waiting for the frame it caused.
struct PerifTrigger
{
  int64_t  tid        = -1;   // object id the verdict must be reported against
  uint64_t dev_us     = 0;    // device clock at trigger (esp_timer_get_time)
  int      tidx       = 1;    // which camera branch announced it
  uint32_t gate_pulse = 0;    // plate position at registration
  int      qs         = -1;   // device pipeline depth when it was announced
  uint64_t arrival_ms = 0;    // host clock when WE saw it -- drives staleness
};

class PerifTriggerPairing
{
public:
  enum Mode { POSITIONAL = 0, TIMESTAMP = 1 };

  enum PairResult {
    PAIRED,        // tid_out is the object this frame belongs to
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
    if (_q.empty()) return EMPTY;

    if (_mode == POSITIONAL || cam_ts_us == 0)
    {
      *tid_out = _q.front().tid;
      _q.pop_front();
      return PAIRED;
    }

    if (!_offset_valid)
    {
      // Bootstrap: pair positionally, but harvest the offset while we do. If
      // the FIFO is already skewed these samples disagree and we simply never
      // gain confidence -- which is the honest outcome, not a wrong offset.
      PerifTrigger t = _q.front();
      _q.pop_front();
      *tid_out = t.tid;
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
      return PAIRED;
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
      // exactly the guess this class exists to stop making. The caller reports
      // no verdict for this frame; the part recirculates.
      _no_candidate++;
      _last_miss_us = best_d;
      return NO_CANDIDATE;
    }

    *tid_out = _q[best_i].tid;
    // Track drift. Only matched frames update the offset, so a mismatch can
    // never drag the estimate along with it.
    double resid = (double)((int64_t)cam_ts_us - (int64_t)_q[best_i].dev_us) - _offset_us;
    _offset_us += resid * 0.05;      // slow EWMA: crystals drift, they do not jump
    _last_resid_us = resid;
    if (fabs(resid) > _max_resid_us) _max_resid_us = fabs(resid);
    // Anything older than the match is an orphan -- its frame never arrived.
    // Leave it queued; the staleness sweep retires it as NA, which is what
    // makes a lost frame cost one part instead of every part after it.
    _skipped += best_i;
    _q.erase(_q.begin() + best_i);
    _matched++;
    return PAIRED;
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

  // Triggers whose frame is never coming. The caller reports these NA so the
  // part recirculates. Without this the queue only ever drains when a frame
  // arrives, so the tail is stranded the moment the plate empties.
  size_t sweepStale(uint64_t now_ms, uint32_t stale_ms, std::vector<PerifTrigger> *out)
  {
    std::lock_guard<std::mutex> lk(_mx);
    size_t n = 0;
    while (!_q.empty())
    {
      const PerifTrigger &f = _q.front();
      if (f.arrival_ms == 0 || now_ms <= f.arrival_ms + stale_ms) break;
      if (out) out->push_back(f);
      _q.pop_front();
      _stale++;
      n++;
    }
    return n;
  }

  // --- diagnostics ---------------------------------------------------------
  size_t   pending()      const { std::lock_guard<std::mutex> lk(_mx); return _q.size(); }
  bool     offsetValid()  const { std::lock_guard<std::mutex> lk(_mx); return _offset_valid; }
  double   offsetUs()     const { std::lock_guard<std::mutex> lk(_mx); return _offset_us; }
  double   lastResidUs()  const { std::lock_guard<std::mutex> lk(_mx); return _last_resid_us; }
  double   maxResidUs()   const { std::lock_guard<std::mutex> lk(_mx); return _max_resid_us; }
  int64_t  lastMissUs()   const { std::lock_guard<std::mutex> lk(_mx); return _last_miss_us; }
  long long rxCount()     const { std::lock_guard<std::mutex> lk(_mx); return _rx; }
  long long matched()     const { std::lock_guard<std::mutex> lk(_mx); return _matched; }
  long long skipped()     const { std::lock_guard<std::mutex> lk(_mx); return _skipped; }
  long long staleCount()  const { std::lock_guard<std::mutex> lk(_mx); return _stale; }
  long long noCandidate() const { std::lock_guard<std::mutex> lk(_mx); return _no_candidate; }
  long long dropCount()   const { std::lock_guard<std::mutex> lk(_mx); return _drops; }

  // One line, because these numbers only mean anything together: a healthy run
  // is skipped~0 and maxResid small; skipped climbing means frames are being
  // lost (and the pairing is absorbing it correctly); maxResid climbing toward
  // the tolerance means the offset is drifting faster than the EWMA tracks.
  void formatStatus(char *buf, size_t n) const
  {
    std::lock_guard<std::mutex> lk(_mx);
    snprintf(buf, n,
      "pairing:%s%s off:%.1fms resid last:%.0fus max:%.0fus | "
      "rx:%lld matched:%lld skipped:%lld stale:%lld nocand:%lld drops:%lld pend:%zu",
      _mode == TIMESTAMP ? "timestamp" : "positional",
      (_mode == TIMESTAMP && !_offset_valid) ? "(bootstrapping)" : "",
      _offset_us / 1000.0, _last_resid_us, _max_resid_us,
      _rx, _matched, _skipped, _stale, _no_candidate, _drops, _q.size());
  }

private:
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
  long long _rx = 0, _matched = 0, _skipped = 0, _stale = 0,
            _no_candidate = 0, _drops = 0;
};
