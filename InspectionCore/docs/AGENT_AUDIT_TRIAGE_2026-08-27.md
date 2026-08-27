# Agent audit triage — 2026-08-27

Working through `AGENT_AUDIT_2026-08-26_RECOVERED.md`, one finding at a time.

**The rule:** a finding is re-derived from the code before anything moves. Its
line numbers are already stale (this file has been edited since), and of eight
checked in earlier sessions none was correct as written — several named
something real while pointing at the wrong file, threshold or mechanism.

Each row says what was actually found, not what was claimed.

| # | finding (abridged) | verdict | done |
|---|---|---|---|
| C1 | `DoImageTransfer=false` turns streaming ON at full rate | **CONFIRMED, with a correction** | fixed |
| C2 | uncounted, unbounded frame drop at the acquisition gate | **CONFIRMED** | fixed |
| C3 | retry `push()` unchecked -> pool-slot leak with `_enqueued=true` | **CONFIRMED, both sites** | fixed |
| C4 | `perifSendQueue` drop accounting wrong in both directions | **CONFIRMED** | fixed |
| C5 | three pipeline threads have no exception handler | **CONFIRMED** | fixed |
| C9 | unsynchronised read of `lastDatViewCache` | **CONFIRMED, left alone** | see below |
| C10 | `calib_bacpac` shared by two threads | **CONFIRMED, left alone** | see below |
| C12 | `saveInspQFullSkipCount` is a plain `int` across threads | **CONFIRMED** | fixed |

## C1 — `DoImageTransfer=false` turns image streaming ON

```c
if (withinMinInterval == false) { *skipImageTransfer = true; }   // the FPS gate
if (DoImageTransfer   == false) { *skipImageTransfer = false; }  // <-- inverted
```

`*skipImageTransfer == false` is what **sends** (the guard further down). So
asking the core to stop transferring images turned them on — and because the
line sits AFTER the FPS gate, it cleared the rate limit at the same time. An
operator shedding load would have got an image for every frame with the cap
disabled.

**Where the finding was wrong:** it described this as something an operator
does. Nothing in the WebUI sends `enable:false` or `DoImageTransfer:false` —
it is unreachable from the product today, which is why it survived. The
defect is real; the scenario was not. It becomes reachable the moment somebody
adds a "stop streaming" control, and they would get the inversion and a dead
rate limit together.

`DATA_VIEW_INSP_DATA_MUST_WITH_IMG` (default **false**) derives the report skip
from the same flag, so with the sense corrected and that flag ON, disabling
image transfer also stops reports. That is what "data must go with an image"
means, but it is worth knowing before turning it on.

## C2 — a frame drop nobody can see

The acquisition gate refuses frames when inspection is behind:

```c
if (inspQueue.size() > imageQueueSkipSize) { LOGE(...); return NAK; }
```

No counter, and an unthrottled `LOGE` at camera rate — while **every** sibling
drop site in the file has both (`poolEmptyDropCount`, `inspQueueDropCount`,
`datViewDropCount`, each with a `% 50` throttle). Parts pass unjudged and the
only evidence is a stderr flood nobody reads.

The finding also claimed the two counters below stay at zero during this, and
that is right: the gate returns before the queue is touched, so a full-queue
eviction never happens.

Now counted as `acqSkipDropCount`, deliberately NOT folded into
`inspQueueDropCount`: one is a frame never accepted, the other a queue
evicting its oldest, and they need different answers.

Live in **CI only**. FI sets `imageQueueSkipSize` to `inspQueue.capacity()` so
it never trips, and the `-1` default is inert because `size_t > int` promotes
`-1` to `SIZE_MAX` — the finding got that detail right too.

## C3 — a leak that ends with the camera "running" and nothing being inspected

```c
if (inspQueue.push(p) == false) {
  if (inspQueue.pop(discard) && discard) { gc(discard); ++inspQueueDropCount; }
  inspQueue.push(p);        // return DISCARDED
}
_enqueued = true;           // set regardless
```

`_enqueued` is what the cleanup at the end of the function reads to decide
whether the queue owns the slot (`if (!_enqueued) retResrc(...)`). So when the
retry failed, the frame was neither queued nor returned **and the flag said it
was somebody else's problem**. One pool slot lost per occurrence; about
`resourcePoolSize` of them and acquisition is dead while the camera still
reports healthy — the exact failure the "never block acquisition" comment above
it was written to avoid.

The finding named a second site with the same shape, `datViewQueue`, and it was
right. That one is worse in one way: it sits inside the pass-down path, so the
caller has already let go and nothing downstream can recover the slot. The
comment directly above it even says *"the dropped pipe has to be returned to the
pool by hand, or the leak is worse than the stall"*.

Both now check the retry and `gc` the frame when it fails.

## C4 — a FALSE "your parts are mis-sorted" alarm

```c
perifSendQueue.pop(discard);   // return ignored
perifSendQueue.push(msg);      // return ignored
int n = ++perifSendDropCount;  // unconditional
```

Wrong in both directions, exactly as the finding said:

- the send thread drains the queue between the failed push and this pop → the
  pop finds nothing, **nothing was lost, and the counter moves anyway**;
- the retry push fails → **two** verdicts lost, one counted.

The first is the serious one. This counter is exported as
`perif_pairing.link.queue_dropped`, which is what an operator reads to decide
whether to re-run a batch, and on a POSITIONAL-pairing machine the log says *"the
verdict train is now OFF BY ONE"* — a claim that every later part got its
neighbour's verdict. Raising that when nothing was dropped is worse than not
counting at all.

Now counts what was actually lost. The `inspQueue` site already gated its
counter on the pop succeeding; this is the same rule.

**And a defect I introduced while fixing it**, caught before commit: with `lost`
able to be 2, the counter steps 2, 4, 6... and the `n % 50 == 1` throttle never
lands — the log would have gone silent for exactly the failure it reports. It
now fires on the first loss and on each crossing of a multiple of 50.

## C5 — three threads whose first exception kills the core

`SlowFrameSaveThread` and `PerifSendThread` both catch. `InspSnapSaveThread`,
`ImgPipeDatViewThread` and `ImgPipeProcessThread` had **no handler at all**, so
anything thrown inside them — `cv::Exception` out of `imwrite` (the reason
`safe_imwrite_cache` exists), `bad_alloc`, `TS_Termination_Exception` out of a
blocking pop — escaped the thread function and became `std::terminate`. The
whole core, on one bad frame.

**Caught per iteration, and it CONTINUES.** Exiting the loop instead would be
worse than terminating on the inspection path: a dead core raises the launcher
shell and somebody knows, whereas a quietly-exited pipeline thread leaves parts
flowing past unjudged with the link still green. One bad iteration is logged and
skipped; the thread stays alive. `TS_Termination_Exception` still breaks, which
is the normal shutdown.

*(First attempt replaced each loop's closing brace instead of inserting before
it, and the file stopped compiling. Reverted and redone — everything up to that
point was already committed, which is the only reason that was cheap.)*

## C12 — a drop counter that can under-report

`saveInspQFullSkipCount` was a plain `int`, written from the action thread and
read and reset from the WS thread, while **every** sibling —
`datViewDropCount`, `inspQueueDropCount`, `poolEmptyDropCount` — was already
`std::atomic<int>`. Lost increments under-report dropped snapshots, which is the
one thing a drop counter must not do. Now atomic, with the log reading the value
it actually incremented rather than re-reading the shared one.

## C9 and C10 — confirmed, and deliberately NOT touched

Both are real and both are races I am not going to fix from a reading.

**C9** — `lastDatViewCache` is read before its lock is taken, at three sites,
and on the early-return path the `resendCache` bit is never set. The finding
calls it "benign on x86 in practice" and that is right, but it is also the one
field the whole cache-swap discipline rests on.

**C10** — `calib_bacpac` is a single global; the inspection thread writes
`bacpac->cam` per frame while the datView thread reads `bacpac->sampler`. The
per-frame camera check was added to stop a frame outliving its camera, and it
fixes the *pipe's* copy before publishing into shared state — so the guarantee
holds only while there is one reader.

Neither has a symptom anybody has reported, both need the ownership model
thought through rather than a lock dropped in, and a wrong fix here turns a
theoretical race into a deadlock on a running machine. **Recorded, not
patched.** They want a session with the threading model in front of you, not a
slot in a triage pass.
