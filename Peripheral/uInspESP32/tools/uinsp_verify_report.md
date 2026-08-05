# uInspESP32 hardware verification run

Run at 2026-08-03 12:49:17

| # | Check | Result | Detail |
|---|---|---|---|
| G.0 | apply real-machine plan (freq/accel/offsets) | PASS | plate_freq=15000 accel=20000 SWITCH=27000 |
| G.1 | reached speed; ramp consistent with plate_accel | PASS | ramp 1.29s (accel predicts 0.75s + measurement settle); steady ~56113 steps/s |
| G.2 | plate speed within 3% of 2s/rev | PASS | measured 30146 ticks/s = 1.990s/rev (err 0.5%) (rim 379 mm/s) |
| G.3 | 30s stream at 30/s survives at real speed | PASS | ran 32s fired=759 objects=759 segments=1 reconnects=0 state=101 (INSPECTION_MODE_READY) ERROR_HIST=[] |
| G.4 | tid strictly increasing by 1 (final segment) | PASS | tid 1..759 |
| G.5 | verdict pattern lands exactly on the exit counters | PASS | sorted {'SEL1': 253, 'SEL2': 253, 'SEL3': 0, 'NA': 253} expected {'SEL1': 253, 'SEL2': 253, 'NA': 253} |
| G.6 | in-flight depth ~= rate x transit (RBuf holds a real plate-load) | PASS | max registered=24, rate x transit = 30.0, RBuf cap 100 |
| G.7 | gate->report latency tracked for every part, max inside the answer window | PASS | n=759 (answered 759), avg=58ms max=178ms window=850ms |
| G.8 | after stop every actuator rests at logical OFF | PASS | all OFF |
| G.9 | configured min_detect_sep_us fits the real 3.0mm part gap | PASS | board=3978us, gap at speed = 7958us (3.0mm at rim); a larger separation MERGES adjacent parts. Pitch limit ~94 parts/s |
| G.10 | real gate path: width filter rejects a blip, accepts a part, no error | PASS | minWidth=900 ticks (11.3mm-eq): blip -> 0 announcements; 80ms part -> 1 object(s); state=101 ERROR_HIST=[] |
