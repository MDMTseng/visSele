# inspd_log baseline fixtures

Regression input for `tools/webctl/mock_inspd_log.mjs` and the WebUI log-panel
WS layer.  Each fixture is a raw `insp.log` (producer-side stdout sink format)
that the mock server replays line-by-line as OTel-shaped JSON over
`ws://...:4091/log`.

## Files

### `caliper_verify_x60.log.gz`

5.5 MB / ~48 800 lines, gzipped to ~232 KB.

Captured on macOS arm64 dev box by looping the headless inspection entry
point 60 times against the existing `caliper_verify` golden:

```
for i in $(seq 1 60); do
  INSP_LOG_DAEMON=1 INSP_LOG_RING_NAME=insp_log_fixture \
  INSP_LOG_LEVEL=debug INSP_LOG_PERSIST_LEVEL=debug \
  visSele --insp caliper_verify.png caliper_verify.hydef out_$i.json
done
```

Module mix (line counts):

| module        | lines  |
|---------------|--------|
| match.sig360  | 41 005 |
| core          |  2 441 |
| match.contour |  1 953 |
| match.group   |  1 952 |
| match.engine  |    488 |
| match.core    |    488 |

Level mix: 47 352 INFO, 975 ERROR.  (DEBUG/TRACE not yet promoted into the
INFO+ disk tier; will appear once Phase D demotes match.* INFO chatter to
DEBUG.)

Good for: protocol-shape regression, module-tree population, filter-glob
testing, backpressure/dropped-notice exercising at high replay rate.

Not in scope: cam.* camera-driver chatter (suppressed in headless `--insp`
mode); per-image timing measurements (no real frame cadence).

## Replay

```
node tools/webctl/mock_inspd_log.mjs \
  --file baseline/inspd_log/caliper_verify_x60.log.gz \
  --port 4091
```

The mock server reads gzip transparently if the path ends in `.gz`; pipe
through `gunzip -c` otherwise.
