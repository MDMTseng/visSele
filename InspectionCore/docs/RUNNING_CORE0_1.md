# Running the legacy Core0_1 inspection daemon (for the WebUI team)

This is the 1st-gen inspection core (the one the current WebUI talks to via
`ws://localhost:<port>`). The default port is **4090**. The 2nd-gen `CoreHub` is
a separate process and not covered here.

The core rework is happening in parallel; that work runs on port **4190** and
must not interfere with your 4090. See "Coexistence with the rework" below.

## TL;DR — start it on 4090

```bash
cd /Users/mdm/workspace/visSele/InspectionCore/Core0_1
DYLD_LIBRARY_PATH=../build/mac-arm64 \
  ../build/mac-arm64/visSele > /tmp/visSele_4090.log 2>&1 &
```

Then verify it's up:

```bash
lsof -nP -iTCP:4090 -sTCP:LISTEN     # should show a visSele PID
tail -n 20 /tmp/visSele_4090.log     # should end with "Try to open websocket... port:4090"
```

Connect the WebUI at `ws://localhost:4090` as usual.

## Why the `cd` and the `DYLD_LIBRARY_PATH` matter

`Core0_1/wiringPanel.cpp` loads `data/default_camera_param.json` at startup using
a **relative** path. If you don't `cd` into `Core0_1/` first, the calibration map
is left uninitialised and every line/circle fit silently returns NaN
(documented in `docs/CORE0_1_CAVEATS.md` §B1). The dynamic-library path is
needed because the build produces `libaravis-0.8.0.dylib` next to the binary in
`build/mac-arm64/` — without it, the binary can't find its camera SDK.

## Rebuilding

If you pulled new code and need to rebuild:

```bash
cd /Users/mdm/workspace/visSele/InspectionCore/build/mac-arm64
cmake --build . --target visSele -j8
```

A new binary on disk does **not** affect a running process. You have to kill
and relaunch to pick up changes (caveat §B2).

## Killing the running daemon

```bash
pid=$(lsof -nP -iTCP:4090 -sTCP:LISTEN -t)
[ -n "$pid" ] && kill "$pid"          # SIGTERM first
sleep 1
ps -p "$pid" >/dev/null && kill -9 "$pid"   # SIGKILL only if needed
```

## Coexistence with the rework (port 4190)

The rework runs the same binary on a different port via the new
backward-compatible `port=` CLI argument. **Default is still 4090** — your
launch command above does not need to change.

```bash
# the rework sandbox (NOT for the WebUI):
DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele port=4190 &
```

If you ever see your WebUI talking to 4190 by accident, that's misconfigured —
the WebUI should hard-code or default to 4090.

## Quick sanity check (no WebUI needed)

The headless `--insp` mode runs one inspection on a saved image+def and writes
the report JSON to disk. It uses no port, so it's safe to run alongside the
4090 daemon.

```bash
cd /Users/mdm/workspace/visSele/InspectionCore/Core0_1
DYLD_LIBRARY_PATH=../build/mac-arm64 ../build/mac-arm64/visSele --insp \
  "/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.png" \
  "/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.hydef" \
  /tmp/insp_out.json
echo "exit=$?"      # 0 = OK, 3 = bad image, 4 = bad def
```

A clean exit 0 with a populated `/tmp/insp_out.json` means the build + calib
chain is healthy.

## If something goes wrong

- **"Try to open websocket... port:4090" then it retries every 5s** — port 4090
  is already in use. Find the offender: `lsof -nP -iTCP:4090 -sTCP:LISTEN`.
- **All measurements come back NaN** — almost always the `cd` issue (caveat B1).
- **WebUI disconnects with code 1006 randomly** — known issue (caveat in the
  `project_run_local` notes); unrelated to startup.
- **Binary doesn't pick up your latest fix** — stale process (caveat B2).
  Kill + relaunch.

## Where stuff lives

- Binary: `InspectionCore/build/mac-arm64/visSele`
- Working dir for run: `InspectionCore/Core0_1/`
- Calib data the binary reads relative to cwd: `Core0_1/data/`
- Caveats / quirks doc: `InspectionCore/docs/CORE0_1_CAVEATS.md`
