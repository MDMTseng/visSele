# uInspESP32 hardware verification run

Run at 2026-08-26 11:06:52

| # | Check | Result | Detail |
|---|---|---|---|
| 0.1 | PING -> PONG | PASS | {'type': 'pong', 'id': 1002, 'ack': True} |
| 0.2 | get_setup returns machine config | PASS | {"ver": "0.0.0 Alpha", "name": "uInspESP32", "stage_pulse_offset": {"L1A_on": 9515, "L1A_off": 9320, "CAM1_on": 9515, "CAM1_off": 9369, "L2A_on": 9317, "L2A_off": 9371, "CAM2_on": 9317, "CAM2_off": 9371, "SWITCH": 29900, "SEL1_on": 30000, "SEL1_off": 30800, "SEL2_on": 30010, "SEL2_off": 30810, "SEL3_on": 30020, "SEL3_off": 30820}, "plate": {"freq": 0, "accel": 2000, "pulses_per_rev": 70400, "diame |
| 0.2 |   field present: machine_id | PASS | 'uI-F80D5E12CFA4' |
| 0.2 |   field present: cfg_from_nvs | PASS | True |
| 0.2 |   field present: pulse_min_width | PASS | 120 |
| 0.2 |   field present: pulse_max_width | PASS | 1000 |
| 0.3 | cfg_from_nvs reported | PASS | cfg_from_nvs=True (False on a board that has never been persisted) |
| 0.4 | set_setup persist machine_id=T110652 | PASS | {'type': 'set_setup', 'persisted': True, 'id': 1004, 'ack': True} |
| 0.4 | machine_id readable before power cycle | **FAIL** | machine_id=uI-F80D5E12CFA4 |
