/* log_crash.h -- Phase G crash handlers for InspectionCore.
 *
 * Install platform-appropriate fatal-signal handlers (POSIX sigaction for
 * SIGSEGV/SIGABRT/SIGFPE/SIGBUS; Win SetUnhandledExceptionFilter +
 * AddVectoredExceptionHandler).  On a fatal signal the handler:
 *   1. Writes a CRASH MARKER + signal number into LogRingHeader.
 *   2. Captures the crashing thread's raw return addresses into
 *      LogRingHeader.crash_frames (async-signal-safe primitives only).
 *   3. Re-raises the original signal so the process dies naturally and any
 *      OS coredump configuration still kicks in.
 *
 * The drainer (Phase F) picks up the CRASH MARKER, symbolicates the
 * addresses via atos / addr2line / DbgHelp, and emits a human-readable
 * crash_<utc>.dump containing the stack trace plus the entire ring (incl.
 * DEBUG/TRACE that never went to disk under normal operation).
 *
 * Cross-platform: same C entry point.  Implementation lives in
 * log_crash_posix.cpp and log_crash_win.cpp.
 */

#ifndef LOG_CRASH_H
#define LOG_CRASH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Install fatal-signal handlers.  Idempotent: subsequent calls are no-ops.
 * Requires that log_open_shm_ring() has already attached the ring so that
 * the handler has somewhere to write crash data. */
void log_install_crash_handlers(void);

/* Test-only: synthesise a crash by raising SIGABRT (POSIX) / DebugBreak
 * (Win).  Used by the integration test.  Doesn't return. */
void log_trigger_test_crash(void);

#ifdef __cplusplus
}
#endif

#endif /* LOG_CRASH_H */
