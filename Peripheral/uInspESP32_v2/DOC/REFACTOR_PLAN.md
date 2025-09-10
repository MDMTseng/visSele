# uInspESP32 Refactor Plan

Goal: Improve structure, readability, testability, and maintainability without breaking existing device behavior or the serial JSON protocol.

## High-Level Principles
- Single Responsibility: Each module owns one clear concern (state machine, scheduling, IO abstraction, protocol, command handlers, data transport, hardware timing, diagnostics).
- Layered Design: ISR/real-time layer (timers, GPIO toggles) isolated from logic & parsing (no dynamic allocation / heavy JSON in ISR paths).
- Progressive Extraction: Move code out of the 2300+ line `SyncTask.cpp` into focused components in stages to avoid large risky commits.
- Backward Compatibility: Preserve existing macros, message formats, and timing until explicitly scheduled for change.
- Testability: Pure logic (parsing, scheduling decisions, state transitions) moved into files that can later be unit-tested with host builds.

## Target Directory Layout
```
include/                Public headers (API surface between modules)
  BoardConfig.hpp       Pins & mechanical constants (added)
  SystemTypes.hpp       Enums & structs (added)
  Pipeline.hpp          (planned) Pipeline data structures & API
  Scheduler.hpp         (planned) Action scheduling interface
  GateSensor.hpp        (planned) Object detection interface
  CommandHandler.hpp    (planned) JSON command dispatch interface
  MessageBus.hpp        (planned) Inter-task/event messaging
  StateMachine.hpp      (planned) State transition logic
  Protocol/DataLayer.hpp(bridge around existing Data_Layer_*)
  Diagnostics.hpp       (planned) Error logging & counters

src/
  main.ino or main.cpp  (thin setup/loop wiring)
  state/                State machine implementation
  pipeline/             Gate sensing + pipeline registration & cleanup
  scheduler/            Timing-based action scheduling (ACT_* queues)
  comm/                 Command parsing, message building, outbound queue
  protocol/             (Existing Data_Layer moved here) framing & crc
  hal/                  Timer ISR, stepper pulse driver, GPIO wrappers
  util/                 RingBuf, resource pools, helpers (existing)
  diagnostics/          Error history, reporting utilities
  tests/ (future)       Host side logic tests (excluded from firmware build)
```

## Current Monolith Responsibilities (SyncTask.cpp)
| Concern | Observed Elements |
|---------|-------------------|
| State Machine | `SYS_STATE`, transfer macros, `SYS_STATE_Transfer`, lifecycle |
| Scheduling | ACT_* queues, macros `ACT_PUSH_TASK`, `ACT_TRY_RUN_TASK`, `Run_ACTS` |
| Pipeline Data | `pipeLineInfo`, `RBuf`, result assignment, cleanup loop |
| Gate Sensing | `GateSensing`, debounce, pulse width filtering, new object creation |
| Command Protocol | Huge `recv_jsonRaw_data` if-else ladder, building responses |
| Messaging | `TaskQ2CommInfo`, `AUX2CommInfoQ`, debug & trigger notifications |
| Hardware Control | Direct `digitalWrite`, timer ISR `onTimer`, stepper stepping |
| Configuration | Stage offset struct `stagePulseOffset`, JSON import/export |
| Error Handling | `ERROR_HIST`, `GEN_ERROR_CODE`, transition to error states |
| Diagnostics | Debug print wrappers, countdown logic |

## Refactor Stages
### Stage 0 (Done / In Progress)
- [x] Extract `BoardConfig.hpp`, `SystemTypes.hpp`.
- [x] Convert `main.hpp` into façade including new headers.
- [ ] Commit baseline & build verification.

### Stage 1: Modularize Pipeline & Scheduling
Files to create:
- `include/Pipeline.hpp`: `pipeLineInfo`, registration API, lifecycle reset.
- `src/pipeline/Pipeline.cpp`: Implements queue ops & cleanup, exposes `registerObject(gatePulse)`.
- `include/Scheduler.hpp`: Defines `ACT_INFO`, enums (or reuse existing), and scheduling API `scheduleForObject`, `runScheduled(curPulse)`.
- `src/scheduler/Scheduler.cpp`: Extract `ACT_PUSH_TASK`, logic from `Run_ACTS` except hardware side-effects (those become callbacks).
Design Shift:
- Introduce callback struct or function pointers for actions (CAM1_on/off, lights, selectors) so logic is testable without GPIO.
Risks:
- Tight coupling with global variables (counts, SEL1 countdown). Mitigate by passing a context struct.

### Stage 2: Gate Sensing Isolation
- `include/GateSensor.hpp` & `src/pipeline/GateSensor.cpp`.
- Provide `GateSensor::tick(pulseCount)` called from ISR to update internal state and, when an object identified, call back `onObjectDetected(gatePulse, width)`.
- Replace direct use of globals in `GateSensing()`.

### Stage 3: State Machine Extraction
- `include/StateMachine.hpp`: Public API `init()`, `applyAction(SYS_STATE_ACT act, int extra=0)`, `currentState()`, and callback registration for enter/exit events.
- `src/state/StateMachine.cpp`: Move `SYS_STATE_LIFECYCLE`, `SYS_STATE_Transfer`.
- Replace direct modifications in other modules with calls.

### Stage 4: Message Bus & Diagnostics
- `include/MessageBus.hpp`: Defines message types (trigger, systemInfo, debug, respFrame).
- `src/comm/MessageBus.cpp`: Wrap `TaskQ2CommInfoQ` + `AUX2CommInfoQ` into a unified queue API.
- `include/Diagnostics.hpp` + `src/diagnostics/Diagnostics.cpp`: Encapsulate `ERROR_HIST`, push + export JSON helpers.

### Stage 5: Command Handler Extraction
- `include/CommandHandler.hpp`: `processCommand(JsonDocument&, JsonDocument& response, CommandContext&)` returning enum (ACK/NO_ACK/ASYNC).
- `src/comm/CommandHandler.cpp`: Rebuild giant if-else ladder using a registry of handlers (`unordered_map<string, handlerFn>`). Keep original command semantics & field names.
- Unit-testable by feeding JSON docs (future host build target).

### Stage 6: Protocol Layer Cleanup
- Move `Data_Layer_*` into `src/protocol/` (adjust include paths).
- Add adapter that feeds parsed JSON strings into CommandHandler.
- (Optional) Add interface `ITransport` for future network / WebSocket extension.

### Stage 7: HAL & ISR Layer
- `include/HAL.hpp` & `src/hal/StepperTimer.cpp` implement: timer init, frequency ramp logic, tick callback injection.
- Extract frequency ramp logic (currently inside `loop()`) to `StepperController` class.
- Provide functions: `setTargetFreq(float)`, `currentFreq()`, `isStable()`, `enableStepper(bool)`.

### Stage 8: Main Loop Simplification
After previous stages, `loop()` should:
```
readSerialIntoProtocol();
messageBus.flushOutbound(transport);
stepperController.update();
stateMachine.pump();
```
ISR: calls `halTick()` which calls scheduler + gateSensor.

### Stage 9: Testing Hooks (Optional Early if Needed)
- Introduce `#ifdef HOST_TEST` builds to allow running logic on desktop.
- Add simple tests for: state transitions, command parsing of `report`, scheduling latency boundaries, error code generation.

### Stage 10: Documentation & Cleanup
- Update README to reference new modular structure.
- Add architecture diagram (ASCII or Mermaid) in `DOC/ARCHITECTURE.md`.
- Deprecate macros in favor of constexpr where safe.

## Cross-Cutting Improvements
| Topic | Action |
|-------|--------|
| Magic Numbers | Replace with named `constexpr` (pulse widths, debounce thresholds). |
| Globals | Wrap into context structs (`RuntimeStats`, `Config`, `SystemContext`). |
| Error Propagation | Return enums or status objects instead of silent returns. |
| Logging | Centralize through `Diagnostics` with categories. |
| Concurrency | Guard shared queues with lightweight `portENTER_CRITICAL` or FreeRTOS mutexes only where needed; avoid blocking in ISR. |
| Naming | Transition from ALL_CAPS for non-macros to `CamelCase` / `snake_case` gradually. |

## Rollback Strategy
Each stage produces a compiling firmware. If a regression appears, revert only that stage's new files & integrations (isolated commit boundaries). No API changes to external host tools until Stage 5+ complete and verified.

## Immediate Next Action
Proceed with Stage 1: create `Pipeline.hpp`, `Scheduler.hpp` scaffolding and migrate `pipeLineInfo`, `ACT_INFO`, and related queues into them with minimal code movement. Wire back into `SyncTask.cpp` via includes to keep behavior stable.

---
(End of Plan – iterative updates will append a Changelog section below.)

### Progress Snapshot (2025-09-11)
- Completed: Extraction of core headers (`BoardConfig.hpp`, `SystemTypes.hpp`).
- Added modules: `Pipeline` (object queue), `Scheduler` (action queues), `GateSensor` (object detection), `StateMachine` (state management), `MessageBus` (message routing), `Diagnostics` (error management), `ITransport` (communication abstraction) with full integration; firmware builds cleanly.
- Completed Stages 0-4, 6: Core headers, Pipeline & Scheduler, Gate Sensing, State Machine extraction, Message Bus & Diagnostics, Protocol Layer Cleanup.
- Skipped Stage 5: CommandHandler registry per user preference - existing if-else ladder works well.
- Next: S7.0 define HAL interface headers for platform abstraction layer.

## Added: Portability & Future STM32 Migration Strategy

To enable migration from ESP32 (Arduino framework) to STM32 (e.g., STM32Cube HAL / LL or bare-metal) the refactor introduces clean abstraction seams early.

### Abstraction Layers
| Layer | Purpose | Migration Impact |
|-------|---------|------------------|
| Logic (pipeline, scheduler, state machine, command handler) | Pure C++ w/ STL-lite or custom containers | Remains unchanged when porting |
| Protocol (JSON framing, CRC) | Byte + text processing only | Unchanged |
| HAL Interfaces | Define hardware contract (`IGpio`, `ITimerTickSource`, `IStepperDriver`, `ITransport`, `IClock`) | Provide new STM32 implementations |
| Platform Implementations | `hal/esp32/` (current), future `hal/stm32/` | Swapped per build target |
| Board Configuration | `BoardConfig.hpp` selects pin map via `#if defined(TARGET_ESP32)` / `TARGET_STM32_xx` | Compile-time selection |

### New Interface Headers (to be introduced around Stage 6–7)
```
include/hal/IGpio.hpp          // pinMode, write, read (non-blocking)
include/hal/ITimerTickSource.hpp// start/stop, setFrequencyHz, registerISRCallback
include/hal/IStepperDriver.hpp  // step pulse issue (or direction + enable control)
include/hal/ITransport.hpp      // write(), available(), read()
include/hal/IClock.hpp          // micros(), millis(), delay(ms), delayUs(us)
include/hal/ILock.hpp           // Optional lightweight mutex wrapper
```

### Guiding Rules
1. No direct `digitalWrite`, `pinMode`, `Serial.*` outside `hal/` or a transport adapter after Stage 7.
2. ISR (`onTimer`) becomes a thin static C wrapper calling a registered `tick()` function from `StepperTimer` (portable).
3. Frequency ramp logic lives in a platform-neutral `StepperController` using only `ITimerTickSource` + `IClock`.
4. Gate sensing uses `IGpio` read method—no Arduino global functions.
5. Logging via `ILogger` interface; current implementation wraps JSON debug output, later can map to SWO/UART.
6. Avoid heap allocation in HAL and ISR; use static or ring buffers.
7. Keep JSON handling off ISR path (already true) to ease low-RAM STM32 variants.

### Conditional Compilation Strategy
```
#if defined(TARGET_ESP32)
  // include hal/esp32/* implementations
#elif defined(TARGET_STM32F4)
  // include hal/stm32f4/* implementations
#endif
```
Only HAL implementation files contain `#ifdef`s—core logic does not.

### Migration Checklist (Later Execution)
1. Implement `hal/stm32/TransportUart.cpp` using DMA or interrupt-driven ring buffer.
2. Implement `hal/stm32/TimerTickSource.cpp` binding TIMx update IRQ to `tick()`.
3. Provide pin mappings in `BoardConfig.hpp` under `#elif defined(TARGET_STM32F4)`.
4. Validate timing jitter for scheduler vs. ESP32 baseline (log pulse deltas).
5. Run host-side logic tests (optional) to confirm parity.

### Testing Hooks
Add a mock HAL layer (`hal/mock/`) for host builds enabling deterministic tests of:
- Scheduling correctness (CAM trigger ordering)
- State transitions under simulated errors
- Command parsing & response payload

### Risks & Mitigations
| Risk | Mitigation |
|------|------------|
| Hidden Arduino dependencies leak back in | Enforce code owners / grep CI for `digitalWrite` outside `hal/` |
| Timing drift on STM32 due to different timer resolution | Define required tick resolution; assert in HAL init |
| Different GPIO latency affecting pulse width | Provide configurable safety margins via constants |
| Serial buffering differences cause protocol fragmentation | Transport layer performs line / packet reassembly identical to ESP32 implementation |

### Updated Stage Ordering Note
Insert a "Stage 7A" after initial HAL extraction: Create interface headers & implement ESP32 adapters while still compiling old paths. Then progressively switch modules to interfaces.

Changelog:
- 2025-09-09: Added portability & STM32 strategy section.
- 2025-09-10: Completed Stages 2-3 (Gate Sensing & State Machine extraction). Added GateSensor module with callback-based object detection and StateMachine module with clean state management API. All modules integrated and building successfully.
- 2025-09-10: Completed Stage 4 (Message Bus & Diagnostics). Added MessageBus module with unified message routing API and Diagnostics module with error history management. Replaced legacy TaskQ2CommInfoQ/AUX2CommInfoQ and ERROR_HIST usage while maintaining backward compatibility. Firmware builds successfully with improved modularity.
- 2025-09-11: Completed Stage 6 (Protocol Layer Cleanup). Relocated Data_Layer files to src/protocol/ directory for better organization. Added ITransport abstraction interface and SerialTransport implementation for platform-agnostic communication. Fixed circular dependency issues in Diagnostics.hpp. Command handling if-else ladder preserved as-is per user preference. All changes maintain backward compatibility and firmware builds successfully.

