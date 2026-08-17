# uInspESP32 hardware verification run

Run at 2026-08-08 20:50:57

| # | Check | Result | Detail |
|---|---|---|---|
| B.0 | widen the selector window for a bare-board round trip | PASS | SWITCH 29900 -> 9914 (600 steps past the camera trigger) |
| B.1 | set plate_freq=1000 | PASS | {'type': 'set_setup', 'id': 1010, 'ack': True} |
| B.2 | enter_insp_mode | PASS | {'id': 1011, 'ack': True} |
| B.3 | timer ISR ticking at speed | PASS | steady ~2980 steps/s |
| B.4 | state is INSPECTION_MODE_READY | PASS | state=101 (INSPECTION_MODE_READY) |
| B.5 | one object per pulse, announced twice (CAM1+CAM2) | PASS | fired=10 objects=10 announcements=20 |
| B.6 | tid strictly increasing by 1 | PASS | tid 1..10 |
| B.7 | no error state after a full reported run | PASS | state=101 (INSPECTION_MODE_READY) ERROR_HIST=[] |
| B.8 | SEL1 counter advanced by the reported parts | PASS | SEL1: 0 -> 10 (reported 10) |
| B.9 | unknown tid faults the machine | PASS | state=112 (INSPECTION_MODE_ERROR) ERROR_HIST=[1] (expect INSP_RESULT_MATCHES_NO_OBJECT=1) |
| B.10 | clear_error recovers | PASS | state=101 (INSPECTION_MODE_READY) |
| B.11 | board still answers after the ISR error path | PASS | no reply = hang/reboot = regression |
| B.12 | unjudged part faults cleanly | PASS | state=112 (INSPECTION_MODE_ERROR) ERROR_HIST=[1, 2] (expect OBJECT_HAS_NO_INSP_RESULT=2 after 10 unjudged parts) |
| B.13 | returned to IDLE, window + plate_freq restored | PASS | state=100 SWITCH=29900 plate_freq=0 |
