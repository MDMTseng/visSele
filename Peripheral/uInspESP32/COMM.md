# Communication Overview

This project communicates with the host controller over a JSON-based serial protocol carried on the ESP32 USB/UART link. The logic is split into a transport layer (`src/comm/Data_Layer_Protocol.cpp/.hpp`) and the application message handler that lives in `src/app/LegacyFirmware.cpp`.

## Transport Layer (`Data_JsonRaw_Layer`)

- Accepts both newline-free JSON streams and an optional framed binary format (opcode 1 = UTF-8 text, opcode 2 = raw data).
- Maintains a rolling buffer and feeds bytes into `json_seg_parser` to locate fully balanced JSON objects/arrays.
- Provides helpers such as `send_json_string`, `send_raw_data`, `send_binary`, and `send_RESET` that prepend the header and compute CRC when the framed raw mode is used.
- Exposes hooks `recv_jsonRaw_data`, `recv_ERROR`, and `recv_RESET` that higher layers implement to process decoded payloads or act on transport faults.
- Tracks protocol health via `enterProtocolError()` / `clearProtocolError()`:
  * Any parsing failure (unexpected character, buffer overflow, CRC mismatch) latches an error flag and calls the application `recv_ERROR`.
  * While latched, incoming bytes are scanned for the literal sequence `"type":"RESET"` to allow recovery without dropping the rest of the stream.

## JSON Segment Parser (`json_seg_parser`)

- Lightweight state machine that recognises object/array boundaries without building a DOM.
- Pushes/pops a stack of `JSonState` markers to keep track of nested structures; depth capped at 48 levels to catch malformed payloads.
- Handles whitespace, commas, and string escapes (quotes preceded by `\`) so quoted data is not mistaken for control characters.
- Returns `RESULT` codes such as `OBJECT_START`, `OBJECT_COMPLETE`, `ARRAY_START`, `ARRAY_COMPLETE`, and `ERROR` which the transport layer uses to trigger higher-level callbacks.

## Application Layer (`MData_JR` in `LegacyFirmware.cpp`)

- Implements the transport callbacks:
  * `recv_jsonRaw_data` parses the ArduinoJson document, routes `"type"` commands, and provides responses/acks.
  * `recv_ERROR` logs the failure, latches a machine-level error (`GEN_ERROR_CODE::SERIAL_PROTOCOL_ERROR`), and pushes a `system_info` message onto the outbound queue.
  * `recv_RESET` clears the transport latch and requests the state machine to exit the inspection error state.
- While a serial error is latched, any command other than `RESET` receives `{"ack":false,"err":"serial_error_locked"}` to force the host to perform a reset.
- Outbound messages are queued in `TaskQ2CommInfoQ` (system/camera events) and `AUX2CommInfoQ` (auxiliary task responses) before the main loop serialises them with `ArduinoJson`.

## Host-Facing Messages

- **Requests (host → device)** include control commands such as `enter_insp_mode`, `report`, `set_setup`, stepper enable/disable, selector controls, and diagnostics (`get_running_stat`, `get_version`).
- **Responses (device → host)** echo the `id`, set `ack`, and provide command-specific payloads. Errors use `ack:false` with descriptive fields (`err`, `log`).
- **Asynchronous notifications**:
  * `cam_trig_tagged` / `bT`: camera trigger metadata and timestamps.
  * `system_info`: state changes and aggregated error history.
  * Debug frames produced by `dbg_printf` / `msg_printf`.

## Error Handling & Recovery Workflow

1. Transport detects an anomaly and calls `enterProtocolError`.
2. Application is notified via `recv_ERROR` and moves the state machine to `INSPECTION_MODE_ERROR`.
3. All further commands are rejected until a well-formed `RESET` frame arrives.
4. When the reset literal is detected, `handleResetRecovery` invokes `recv_RESET`, clears the latch, and advances the state machine with `INSPECTION_ERROR_REDEEM`.

## Extending the Protocol

- Add new commands inside `MData_JR::recv_jsonRaw_data` using the existing `strcmp(type, "...")` pattern; guard every branch with appropriate ack/response handling.
- For new asynchronous events, extend `TaskQ2CommInfo` with the required fields and consume them inside the `firmwareLoop` sender loop.
- When introducing new data frames, prefer the framed raw mode (`send_raw_data`) if payloads exceed 125 bytes or contain binary content.

Refer to `README.md` for the high-level system behaviour and to `include/core/FirmwareTypes.hpp` for shared enums and error codes.
