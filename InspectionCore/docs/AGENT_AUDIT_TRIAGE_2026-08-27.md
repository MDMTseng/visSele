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
