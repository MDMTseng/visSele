# uInspESP32 hardware verification run

Run at 2026-08-01 01:52:01

| # | Check | Result | Detail |
|---|---|---|---|
| G.0 | apply real-machine plan (freq/accel/offsets) | PASS | plateFreq=15000 accel=20000 SWITCH=27000 |
| G.1 | reached speed; ramp consistent with plateAccel | PASS | ramp 1.25s (accel predicts 0.75s + measurement settle); steady ~55346 steps/s |
| G.2 | plate speed within 3% of 2s/rev | PASS | measured 29958 ticks/s = 2.003s/rev (err 0.1%) (rim 376 mm/s) |
| G.3 | 30s stream at 30/s survives at real speed | PASS | fired=774 objects=774 state=101 (INSPECTION_MODE_READY) ERROR_HIST=[] |
| G.4 | tid strictly increasing by 1 across the stream | PASS | tid 399..1172 |
| G.5 | verdict pattern lands exactly on the exit counters | PASS | sorted {'SEL1': 388, 'SEL2': 193, 'SEL3': 0, 'NA': 193} expected {'SEL1': 388, 'SEL2': 193, 'NA': 193} |
| G.6 | in-flight depth ~= rate x transit (RBuf holds a real plate-load) | PASS | max registered=24, rate x transit = 30.0, RBuf cap 100 |
| G.7 | gate->report latency tracked for every part, max inside the answer window | PASS | n=774 (answered 774), avg=50ms max=131ms window=850ms |
| G.8 | after stop every actuator rests at logical OFF | PASS | all OFF |
| G.9 | configured minDetectTimeSep_us fits the real 3.0mm part gap | PASS | board=4000us, gap at speed = 7958us (3.0mm at rim); a larger separation MERGES adjacent parts. Pitch limit ~94 parts/s |
| G.10 | real gate path: width filter rejects a blip, accepts a part, no error | PASS | minWidth=900 ticks (11.3mm-eq): blip -> 0 announcements; 80ms part -> 1 object(s); state=101 ERROR_HIST=[] |
