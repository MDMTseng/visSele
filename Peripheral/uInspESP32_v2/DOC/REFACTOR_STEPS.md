# Refactor Execution Steps

Status Legend: 
- TODO     = Not started
- WORKING  = In progress (implementation ongoing)
- TESTING  = Implemented; validating build/runtime
- DONE     = Merged / Stable

Each step lists: Goal, Actions, Deliverables, Exit Criteria, Dependencies, Rollback.

| ID | Step | Status | Owner | Depends |
|----|------|--------|-------|---------|
| S0.1 | Extract core type & pin headers (`BoardConfig.hpp`, `SystemTypes.hpp`) | DONE | - | - |
| S0.2 | Convert `main.hpp` to façade include | DONE | - | S0.1 |
| S0.3 | Baseline build verification & tag pre-refactor commit | DONE | - | S0.2 |
| S1.1 | Create `Pipeline.hpp/.cpp` scaffolding (pipeLineInfo, RBuf wrapper) | DONE | - | S0.3 |
| S1.2 | Create `Scheduler.hpp/.cpp` (ACT_INFO, scheduling API, callbacks) | DONE | - | S1.1 |
| S1.3 | Integrate pipeline & scheduler into `SyncTask.cpp` (minimal code movement) | DONE | - | S1.2 |
| S1.4 | Build & runtime smoke test (object detection & triggers intact) | DONE | - | S1.3 |
| S2.1 | Extract gate sensing to `GateSensor.*` with callback | DONE | - | S1.4 |
| S2.2 | Replace direct call sites & test debounce correctness | DONE | - | S2.1 |
| S3.1 | Extract state machine (`StateMachine.hpp/.cpp`) | DONE | - | S2.2 |
| S3.2 | Replace global `SYS_STATE_Transfer` usage with interface | DONE | - | S3.1 |
| S3.3 | Add unit-test harness (host build) for transitions (optional) | TODO | - | S3.2 |
| S4.1 | Implement `MessageBus` abstraction | DONE | - | S3.2 |
| S4.2 | Implement `Diagnostics` (error history encapsulation) | DONE | - | S4.1 |
| S5.1 | Introduce `CommandHandler` registry + command context | TODO | - | S4.2 |
| S5.2 | Migrate existing if-else command ladder | TODO | - | S5.1 |
| S5.3 | Add host test for key commands (`report`, `get_setup`, `enter_insp_mode`) | TODO | - | S5.2 |
| S6.1 | Relocate Data_Layer files to `src/protocol/` | TODO | - | S5.3 |
| S6.2 | Add `ITransport` abstraction & adapt current serial | TODO | - | S6.1 |
| S7.0 | (Prep) Define HAL interface headers (`IGpio`, `ITimerTickSource`, etc.) | TODO | - | S6.2 |
| S7.1 | ESP32 implementations under `hal/esp32/` | TODO | - | S7.0 |
| S7.2 | Replace direct hardware calls with interfaces | TODO | - | S7.1 |
| S7.3 | Extract stepper frequency ramp to `StepperController` | TODO | - | S7.2 |
| S7A.1 | Verify ISR minimal & platform-neutral | TODO | - | S7.3 |
| S8.1 | Simplify `loop()` to high-level orchestration | TODO | - | S7A.1 |
| S9.1 | Add mock HAL + host logic tests (scheduler, command, state) | TODO | - | S8.1 |
| S10.1 | Documentation update (ARCHITECTURE.md + README sections) | TODO | - | S9.1 |
| S10.2 | Replace selected macros with constexpr (non-breaking) | TODO | - | S10.1 |
| S11.1 | Portability prep: add STM32 `#ifdef` scaffolding only (no impl) | TODO | - | S10.2 |
| S11.2 | (Future) Implement STM32 HAL layer (separate task) | TODO | - | S11.1 |

---
## Detailed Step Descriptions

### S0.3 Baseline Build Verification
Goal: Ensure current code (post header extraction) builds & runs identically.
Actions: Clean build, flash, quick functional test (triggers, JSON PING).
Deliverables: Build log, optional git tag `refactor-baseline`.
Exit: No new warnings & runtime behavior unchanged.
Rollback: Revert header extraction commit.

### S1.x Pipeline & Scheduler Extraction
Goal: Isolate object lifecycle & action scheduling logic.
Actions: Create minimal façade classes; move structs (`pipeLineInfo`, `ACT_INFO`) & queue operations without renaming fields.
Deliverables: New headers + updated includes; `SyncTask.cpp` shrinks.
Exit: Build passes; triggers & selectors still operate.
Rollback: Revert new files and reinstate original code block.

### S2.x Gate Sensing
Goal: Abstract pulse detection for portability & test injection.
Actions: Implement `GateSensor` with configurable debounce/constants; inject callback to pipeline.
Exit: Detection rate matches pre-refactor (log sample counts).

### S3.x State Machine
Goal: Decouple state transitions & side-effects.
Actions: Move macros & lifecycle into `StateMachine`; register enter/exit callbacks used by other modules.
Exit: All former direct calls replaced; transitions verified via logs.

### S4.x Message & Diagnostics
Goal: Central message routing and error reporting.
Actions: Replace raw ring buffers with `MessageBus`; encapsulate `ERROR_HIST` manage + JSON export.
Exit: Existing outbound JSON frames unchanged (diff by functional test).

### S5.x Command Handler
Goal: Maintainable extensible command parsing.
Actions: Introduce registry; map legacy commands to handlers; add default unknown handler.
Exit: Host script diff shows identical responses for command set.

### S6.x Protocol Relocation
Goal: Organized folder structure for data framing.
Actions: Move Data_Layer files; adjust includes; no logic changes.
Exit: Clean build.

### S7.x HAL Abstraction
Goal: Platform independence & slim ISR.
Actions: Define interfaces; provide ESP32 implementations; replace direct functions.
Exit: Only HAL layer touches Arduino/ESP-IDF symbols.

### S7A.x ISR Neutrality
Goal: Confirm ISR generic; measure pulse jitter (optional log).
Exit: Jitter within acceptable window vs baseline.

### S8.x Main Loop Simplification
Goal: Clear orchestration code path.
Actions: Collapse loop to orchestrator calls; remove global drift logic duplicates.

### S9.x Host Tests
Goal: Safety net for remaining refactors & future ports.
Actions: Add mock HAL; test scheduling, state transitions, command flows.

### S10.x Documentation & Macro Modernization
Goal: Developer clarity & modernization.
Actions: Architecture doc; selective macro -> constexpr (non-ABI affecting).

### S11.x Portability Scaffold
Goal: Prepare for STM32 implementation.
Actions: Add `TARGET_ESP32` guards, placeholder `hal/stm32/README.md`.

---
## Progress Log
(Add dated entries as steps advance.)

| Date | Step | New Status | Notes |
|------|------|------------|-------|
| 2025-09-09 | S0.1 | DONE | Headers extracted |
| 2025-09-09 | S0.2 | DONE | Façade main.hpp created |
| 2025-09-09 | S0.3 | DONE | PlatformIO build success (no errors; warnings recorded) |
| 2025-09-09 | S1.1 | DONE | Pipeline module extracted (Pipeline.hpp / Pipeline.cpp) |
| 2025-09-09 | S1.2 | DONE | Scheduler module extracted (Scheduler.hpp / Scheduler.cpp) |
| 2025-09-10 | S1.3 | DONE | Integration completed: removed PIPE_INFO_LEN, updated RESET_ALL_PIPELINE_QUEUE to use modules |
| 2025-09-10 | S1.4 | DONE | Smoke test passed: build successful, object detection flow verified, trigger mechanisms intact, pipeline/scheduler integration working |
| 2025-09-10 | S2.1 | DONE | GateSensor module extracted: created GateSensor.hpp/.cpp with callback-based object detection, debounce logic, and ISR-safe tick() method |
| 2025-09-10 | S2.2 | DONE | Gate sensing integration completed: replaced GateSensing() calls with gateSensor.tick(), removed old functions, maintained backward compatibility |
| 2025-09-10 | S3.1 | DONE | StateMachine module extracted: created StateMachine.hpp/.cpp with clean API, state transition logic, and lifecycle management |
| 2025-09-10 | S3.2 | DONE | State machine integration completed: replaced SYS_STATE_Transfer() calls with stateMachine.applyAction(), removed old functions, maintained compatibility |
| 2025-09-10 | S4.1 | DONE | MessageBus abstraction implemented: created MessageBus.hpp/.cpp with unified message routing, replaced TaskQ2CommInfoQ/AUX2CommInfoQ usage, maintained backward compatibility |
| 2025-09-10 | S4.2 | DONE | Diagnostics module implemented: created Diagnostics.hpp/.cpp with error history management, replaced ERROR_HIST usage, added JSON export functionality |
