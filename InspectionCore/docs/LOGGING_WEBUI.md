# InspectionCore Log System — WebUI Integration Guide

Status: **drainer process (`inspd_log`) shipping; WebSocket bridge is Phase F.2, not yet implemented.** This doc is the contract WebUI can build against in parallel.

---

## 1. The big picture

```
 ┌──────────────────┐    shm ring     ┌──────────────────┐    ws://     ┌────────────┐
 │  visSele (Core)  │ ──────────────► │  inspd_log       │ ───────────► │  WebUI     │
 │   LOG* macros    │                 │  (drainer)       │              │  Log panel │
 │   crash handlers │                 │  disk + ws + dump│              └────────────┘
 └──────────────────┘                 └──────────────────┘
```

- **Producer (visSele)** writes lines into a 16 MB shared-memory ring buffer. INFO/WARN/ERROR/FATAL also go to a coarse on-disk rotating file. DEBUG/TRACE stay in RAM only (SSD-friendly).
- **Drainer (`inspd_log`)** is a sibling process spawned by main when `INSP_LOG_DAEMON=1`. It owns the disk file, the ephemeral in-RAM tail buffer, and (Phase F.2) the WebSocket port that WebUI connects to.
- **Crash**: a fatal signal in the producer is caught by `log_crash_*.cpp`, which writes a marker + raw stack frames into the ring header. The drainer detects the marker, drains the ring, symbolicates frames, and writes `crash_<utc>.dump` to disk. (Phase F.2 will additionally push it over the WebSocket.)

Producer and drainer are decoupled processes — the producer crashing does **not** kill the drainer; the drainer crashing does **not** kill the producer.

---

## 2. Enabling it

The drainer is opt-in. Set env vars before launching `visSele`:

| Env var | Default | Meaning |
|---|---|---|
| `INSP_LOG_DAEMON` | unset (off) | `1` = spawn `inspd_log` alongside main |
| `INSP_LOG_RING_NAME` | `insp_log_ring` | shm object name (Win: file-mapping name) |
| `INSP_LOG_RING_MB` | `16` | ring size in MB |
| `INSP_LOG_DIR` | `./` | directory for `insp.log*` and `crash_*.dump` |
| `INSP_LOG_FILE` | `insp.log` | base filename |
| `INSP_LOG_ROTATE_MB` | `64` | rotate when current file exceeds this |
| `INSP_LOG_ROTATE_KEEP` | `5` | keep N rotated files |
| `INSP_LOG_PERSIST_LEVEL` | `INFO` | min level routed to disk (DEBUG/TRACE always ephemeral) |
| `INSP_LOG_WS_PORT` | `4091` *(planned)* | drainer's WebSocket port (Phase F.2) |
| `INSP_LOG_LEVEL` | `INFO` | producer-side global threshold |
| `INSP_LOG_MODULES` | unset | per-module override, e.g. `match.*=DEBUG,cam.bmp=TRACE` |
| `INSP_LOG_FULL_COREDUMP` | unset | `1` = also let OS write a coredump on crash |

Runtime tunables (producer-side, via C API) — WebUI can drive these once the WS bridge is in:

```c
void log_set_global_level(int lv);                     // 0=TRACE … 5=FATAL, 6=OFF
void log_set_module_level(const char *mod, int lv);    // e.g. "cam.bmp"
int  log_get_effective_level(const char *file_or_mod); // for UI reflection
```

---

## 3. WebSocket protocol (Phase F.2 — implement against this)

> ⚠️ Not yet live. Drainer currently writes to disk only. The schema below is the agreed-on contract — when F.2 lands, this is what the port will speak.

### 3.1 Endpoint

- URL: `ws://<core-host>:4091/log`
- Subprotocol: `inspd_log.v1`
- No auth in v1; bound to localhost by default. Production will require token in query string (`?token=...`).
- One connection = one subscriber. Drainer accepts many in parallel.

### 3.2 Framing

All frames are JSON text. Binary frames are reserved (future: gzipped batch).

### 3.3 Server → Client messages

> **Schema philosophy.** Field names follow the **OpenTelemetry Logs Data Model** so a standard otel-collector / Loki / Grafana can scrape `inspd_log` with zero adapter code. We don't run the OTel SDK — we just speak its shape.

**3.3.1 `hello` — sent on connect**

```json
{
  "type": "hello",
  "drainerVersion": 1,
  "ringVersion": 2,
  "ringSlots": 65534,
  "ringMb": 16,
  "startedUnixNano": "1748751130000000000",
  "logDir": "/var/log/insp/",
  "resource": {
    "service.name": "visSele",
    "service.instance.id": "12345",
    "process.pid": 12345,
    "host.name": "factory-cell-3"
  }
}
```

`resource` mirrors OTel's Resource concept — fixed per-process metadata. Int64s are sent as strings to survive JSON number precision.

**3.3.2 `log` — one log line (live tail)**

```json
{
  "type": "log",
  "timeUnixNano": "1748751131019144000",
  "tsMsSinceStart": 1019144,
  "severityNumber": 9,
  "severityText": "INFO",
  "body": "driver_name:BMP_carousel id:BMP_carousel_0",
  "attributes": {
    "module": "cam.bmp",
    "code.filepath": "CameraLayer_BMP_carousel.cpp",
    "code.lineno": 41,
    "code.function": "CameraLayer_BMP_carousel"
  },
  "traceId": null,
  "spanId": null
}
```

Field mapping notes:
- `severityNumber` follows the OTel scale: TRACE=1, DEBUG=5, INFO=9, WARN=13, ERROR=17, FATAL=21. (Gaps are intentional — OTel reserves them for `INFO2`, `ERROR3`, etc.)
- `severityText` is the canonical uppercase short form.
- `body` is the rendered message only (no timestamp/level prefix).
- `attributes.code.*` follows OTel semantic conventions for source location.
- `traceId` / `spanId` are reserved (null for now); when we ever add request-scoped tracing, these get populated and existing UIs keep working.
- `tsMsSinceStart` is kept as a convenience for the "boot-relative" view we already show on disk. WebUI's primary axis should be `timeUnixNano`.
- High-volume scenarios may batch into `{"type":"logBatch","items":[…]}`. WebUI should accept both forms.

**3.3.3 `dropped` — synthetic backpressure notice**

When the WS subscriber falls behind and the drainer has to discard buffered lines for that connection:

```json
{ "type": "dropped", "count": 42, "sinceUnixNano": "1748751131000000000" }
```

UI should render this as a gap marker ("42 lines lost") rather than silently skip.

**3.3.4 `backlogChunk` — response to `subscribe` with `sinceUnixNano` or `tailN`**

```json
{ "type": "backlogChunk", "items": [ /* log objects */ ], "more": true }
```

Repeated until `"more": false`. The live stream resumes after backlog is delivered.

**3.3.5 `crash` — crash detected**

Sent once when drainer observes the crash marker. After this, the drainer typically exits (its parent is dead); UI should treat it as terminal.

```json
{
  "type": "crash",
  "signal": "SIGABRT",
  "signalRaw": 6,
  "timeUnixNano": "1748751130000000000",
  "dumpPath": "/var/log/insp/crash_20260601T033210Z.dump",
  "frames": [
    { "idx": 0, "addr": "0x1047bcc50", "symbol": "?", "file": null, "line": null },
    { "idx": 1, "addr": "0x19c9ea744", "symbol": "_sigtramp", "file": null, "line": null }
  ],
  "ringTail": [ /* up to N log objects -- the last lines before death */ ],
  "ephemeralTail": [ /* DEBUG/TRACE lines never written to disk */ ]
}
```

`frames[].symbol` may be `"?"` when Mach-O addresses don't resolve (known limitation; needs `-rdynamic` or `atos` shell-out in drainer — tracked separately). On Windows + Linux with debug syms loaded, full function/file/line is provided.

**3.3.6 `ack` — response to a client command**

```json
{ "type": "ack", "id": "<echo of cmd id>", "ok": true }
```

or:

```json
{ "type": "ack", "id": "...", "ok": false, "error": "unknown module" }
```

### 3.4 Client → Server messages

**3.4.1 `subscribe`**

```json
{
  "type": "subscribe",
  "id": "c1",
  "minSeverityNumber": 5,            // 1=TRACE 5=DEBUG 9=INFO 13=WARN 17=ERROR 21=FATAL
  "modules": ["cam.*", "match.*"],   // glob; omit/empty = all
  "includeEphemeral": true,          // also stream DEBUG/TRACE
  "backlog": { "tailN": 500 }        // OR {"sinceUnixNano": "..."}; omit = none
}
```

Sent right after `hello`. Drainer replies with backlog (if requested) then live stream.

Re-sending `subscribe` updates filters in place.

**3.4.2 `setLevel`** — change producer-side threshold

```json
{ "type": "setLevel", "id": "c2", "scope": "global", "severityNumber": 5 }
{ "type": "setLevel", "id": "c3", "scope": "module", "module": "cam.bmp", "severityNumber": 1 }
```

Drainer forwards via a control IPC ring (TBD; producer reads it next tick).

**3.4.3 `getModules`** — list registered LOG_MODULE names

```json
{ "type": "getModules", "id": "c4" }
```

Reply:

```json
{
  "type": "modules",
  "id": "c4",
  "modules": [
    { "name": "core",          "effectiveSeverityNumber": 9 },
    { "name": "cam.bmp",       "effectiveSeverityNumber": 9 },
    { "name": "match.linemod", "effectiveSeverityNumber": 9 }
  ]
}
```

Use this to populate a tree-control on the log panel.

**3.4.4 `dumpNow`** — force the drainer to write a snapshot dump

```json
{ "type": "dumpNow", "id": "c5" }
```

Reply via `ack` with path. Useful for "save the current state for support".

**3.4.5 `ping` / `pong`** — keepalive (UI should ping every ~20 s; drainer drops the connection after 60 s of silence).

### 3.5 Wire-level error handling

- Drainer closes the connection with code `1011` if it itself is shutting down (parent dead). UI should reconnect with exponential backoff (max ~30 s).
- After a `crash` message, expect the connection to drop. UI should keep the crash payload visible across reconnects.
- The producer can restart without the drainer noticing (drainer is keyed on heartbeat). On a *cold* restart of both, WebUI sees a fresh `hello` with a new `started_utc`.

---

## 4. Disk artifacts

WebUI may want to expose these via download links or paste into bug reports.

| File | Contents | Notes |
|---|---|---|
| `<INSP_LOG_DIR>/insp.log` | INFO+ lines, plaintext, one per line | Currently rolling at 64 MB × 5. |
| `<INSP_LOG_DIR>/insp.log.1`…`.5` | rotated history | Older = higher number. |
| `<INSP_LOG_DIR>/crash_<utc>.dump` | header + stack + full ring + ephemeral DEBUG/TRACE | One per crash event. |

Line format on disk (identical to the producer-side stdout sink):

```
[  1019.144][I][cam           ][CameraLayerManager.c:245  connectCamera] driver_name:BMP_carousel ...
 │           │ │                │                       │
 │           │ │                │                       └─ function name
 │           │ │                └──────────────────────── file:line
 │           │ └───────────────────────────────────────── module (LOG_MODULE)
 │           └─────────────────────────────────────────── level char (T D I W E F)
 └─────────────────────────────────────────────────────── seconds since process start (3 decimals)
```

WebUI doesn't need to parse this — the WebSocket already delivers a
pre-parsed JSON object per line. Parsing is only needed if the UI lets
users paste a raw log file.

### 4.1 Crash dump format

Plaintext, sectioned:

```
=== InspectionCore crash dump ===
timestamp: 20260601T033210Z
signal:    SIGABRT (raw=6)
pid:       12345

--- Stack trace (N frames) ---
#0  0x...  <symbol>  (<file:line>)
...

--- Ring (entire retained history, incl. verbose) ---
[ ... raw log lines, oldest first ... ]
(K lines)

--- Ephemeral DEBUG/TRACE buffer (M lines) ---
[ ... lines that never reached disk under normal operation ... ]

=== end of dump ===
```

Stable: ordering, section headers, `(K lines)` / `(M lines)` counts. WebUI parsers can split on the `---` lines.

---

## 5. UI suggestions (non-binding)

- **Live tail panel**: virtualized list, color by level char, column for module.
- **Module tree**: from `get_modules`, with per-module slider (OFF / FATAL / ERROR / WARN / INFO / DEBUG / TRACE). Sending `set_level` per slider change is fine — it's cheap.
- **Filter bar**: client-side text filter on top of the WS stream. Server-side filter via `subscribe` is for bandwidth, not search.
- **Crash banner**: when a `crash` arrives, pin a banner with signal name, dump path, "Download dump" link, and a "Show last 200 lines + stack" expander prefilled from `ring_tail` + `frames`.
- **Reconnect**: backoff 1s → 2s → 4s → 8s → cap at 30 s. Show "disconnected — retrying" state.

---

## 6. What's not built yet

The producer side (LOG* macros, ring, crash handlers, disk file) is done and shipping. **The WebSocket bridge in the drainer is Phase F.2 and not yet implemented.** Until it lands:

- WebUI cannot live-tail. Disk file polling is the interim workaround.
- `set_level` from UI is not wired. Use env vars / restart for now.
- The `crash` push notification doesn't exist; UI can poll the log dir for `crash_*.dump` as a temporary measure.

If WebUI wants to start integrating early: build against the JSON schema above, mock the server with a tiny Node script that replays a captured `insp.log` line-by-line as `log` messages. When F.2 ships, point the URL at `ws://localhost:4091/log` and it should just work.

---

## 7. Contact / questions

Open a `Log Phase F.2` ticket or ping in the core channel. Schema changes to the WebSocket contract will bump the subprotocol to `inspd_log.v2`; v1 will keep working for the lifetime of the current factory deployment.
