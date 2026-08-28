# The GPU "leak" is a sawtooth, and the ceiling holds

2026-08-28, from a full-resolution soak on the Windows bench.

## What was claimed, and why it was wrong

A soak at scale 1 (imgW 640, the view zoomed until the stream negotiates full
resolution) showed the GPU process climbing steadily:

    gpuMB   261 -> 490 over 8 minutes,  ~29 MB/min
    heapMB  flat at 15.4
    DOM     flat at 277 nodes
    forced collection: 822 -> 648 MB, 174 freed at once

Everything about that reads as a leak, and it was written up as one. It is not.
**Those eight minutes were the rising edge of a sawtooth**, sampled before the
first eviction. Normalising it per inspection report (12-16 KB) produced a
convincingly precise number that described the *fill rate of a cache*, not a
loss.

## What an hour actually looks like

    peaks    412@8m  414@23m  415@38m  418@40m  413@53m  418@56m
    troughs  279@12m 269@27m  282@43m  255@57m
    gpuMB    255 - 418 across the whole hour
    elRSS    578 - 776 MB
    state    101 for all 58 samples, no faults

Four complete cycles, period 13-15 min. The ceiling moves 412 -> 418, which is
noise; the floor drifts slightly DOWN. It plateaus on its own at ~415 before any
forced collection -- the t=6.2..10.4 samples sit flat at 412/412/411/403 with
the soak's forced GC not arriving until 10.4 -- and one of the releases (t=26.0)
has no [gc] line against it at all. Chromium is evicting to its own budget.

## Consequences for the things built on the wrong reading

* **No fix is needed.** "Solve the GPU leak" has no work in it. The bounded
  behaviour is the designed behaviour of an accelerated canvas.
* **The ctx.filter A/B is not usable as stated.** Greying NA shapes with
  ctx.filter measured 12.0 vs 9.6 MB/min at a matched inspection rate, reported
  as "20% of the leak". Both numbers are slopes of rising edges; against a
  sawtooth they say something about how fast the cache fills, not about a leak
  share. The switch (window.__NA_FILTER_OFF__ / SOAK_NA_FILTER=0) is worth
  keeping as an instrument; the conclusion is withdrawn.
* **"6 hours would need 10 GB"** was extrapolation from a slope with a ceiling
  above it. It never applied.

## What is still open

Whether the ceiling holds for six hours rather than one. An hour and four cycles
is good evidence and not proof; a long run is in progress to answer it.

Separately unexplained: an earlier run at full resolution lost its window at
17.7 min, near the first peak. Two other runs that night ended by external kill,
and the bench was being adjusted at the time, so there is no reason yet to link
the window loss to the memory cycle -- but it happened once, near a peak, and
that is worth remembering if it happens again.

## For whoever measures this next

Sample longer than one cycle before believing a slope. Fifteen minutes of a
sawtooth looks exactly like a leak, and per-unit normalisation makes it look
like a well-understood one.

## The strongest form of the argument: change the rate, not the clock

Everything above is one plate speed. If the ceiling were a leak that merely
looked bounded over an hour, running the machine HARDER would expose it. So the
same soak was run at a second speed. `SOAK_FREQ` maps linearly onto plate
speed -- 8000 -> 13.6 rpm, 10000 -> 17.0 rpm, both on one line through the
origin at 588 Hz per rpm -- which makes the two runs directly comparable.

    13.6 rpm   peaks 415 417 420 415 408      period ~62 min   over 5h13
    17.0 rpm   peaks 475 477 431 401 393      period ~16 min   over ~1h40

Raising the rate 25% cut the period by ~4x and lifted the ceiling by ~60 MB.
Both stayed bounded. That is the signature of a cache evicted against a budget
as WORK is done, not of a loss accumulating against wall time: a leak's period
does not shorten when you feed it faster, and its ceiling does not exist.

Six sawtooth cycles across two speeds, no ceiling drift in either.

## What the numbers are NOT evidence of

One 10000 run showed renderer RSS climbing 165 -> 227 MB with average latency
20 -> 38 ms over 45 minutes, which reads as a leak until the throughput column
is put beside it:

    t       seen/s   rendMB   lat_avg   imgKBps
    16.6    18.1     171      21.3      210
    33.3    22.7     235      31.5      266
    45.8    22.2     227      37.6      268

`seen` stepped up ~40% on its own at t~33 and the memory followed it up and
then FLAT (235 -> 228 -> 227 -> 227) once the load stopped rising. Latency was
still creeping but decelerating (3.6 -> 0.4 ms per sample). Load-tracking, not
loss. Why the feed rate stepped up mid-run with no setting changed is a
separate open question, and it means cross-run comparisons of absolute memory
need the throughput column quoted alongside them.

## Reproducing

    cd UI/Launcher
    SOAK_APP_ROOT=<repo>/export_v2/app SOAK_FREQ=10000 SOAK_ZOOM_IN=200 \
      node tools/soak.mjs 360

SOAK_APP_ROOT must point at `export_v2/app`, not `export_v2` -- the latter
holds only 1.1.104 and the soak then silently measures the old build. Zoom is
capped internally at whatever reaches imgScale 1 (10 notches), so 200 is "all
the way in" rather than an overshoot.
