/* log_crash_win.cpp -- Windows fatal-exception handler.
 *
 * Uses SetUnhandledExceptionFilter so we run after all SEH/__try blocks
 * have declined to handle the exception.  Captures the crashing thread's
 * stack via RtlCaptureStackBackTrace (no symbols; drainer resolves).
 *
 * Vectored exception handler (AddVectoredExceptionHandler) is an
 * alternative -- it runs earlier in the chain -- but SetUnhandledException
 * Filter is sufficient for fatal-crash logging and avoids interfering
 * with user-mode debuggers.
 */

#ifdef _WIN32

#include <log_crash.h>
#include <log_ring.h>
#include <logctrl.h>

#include <atomic>
#include <cstring>
#include <windows.h>

extern "C" void *log_get_shm_ring_mapping(void);

namespace {

std::atomic<bool> g_installed{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_prev_filter = nullptr;

uint32_t code_to_marker(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_IN_PAGE_ERROR:        return LOG_CRASH_SIGSEGV;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_INT_OVERFLOW:
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        case EXCEPTION_FLT_OVERFLOW:
        case EXCEPTION_FLT_UNDERFLOW:
        case EXCEPTION_FLT_INVALID_OPERATION:return LOG_CRASH_SIGFPE;
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_BREAKPOINT:
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return LOG_CRASH_SIGABRT;
        case EXCEPTION_DATATYPE_MISALIGNMENT:return LOG_CRASH_SIGBUS;
        default:                             return LOG_CRASH_OTHER;
    }
}

LONG WINAPI handler(EXCEPTION_POINTERS *ep) {
    LogRingHeader *h =
        static_cast<LogRingHeader *>(log_get_shm_ring_mapping());
    if (h) {
        DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
        h->crash_signal = static_cast<uint32_t>(code);

        /* RtlCaptureStackBackTrace fills caller-supplied buffer with raw
         * return addresses.  Safe in an exception filter context. */
        void *frames[LOG_CRASH_FRAME_MAX];
        USHORT n = RtlCaptureStackBackTrace(
            0, LOG_CRASH_FRAME_MAX, frames, nullptr);
        for (USHORT i = 0; i < n; ++i) {
            h->crash_frames[i] = reinterpret_cast<uint64_t>(frames[i]);
        }
        h->crash_frame_count.store(static_cast<uint32_t>(n),
                                   std::memory_order_release);

        h->head.fetch_add(1, std::memory_order_acq_rel);

        h->crash_marker.store(code_to_marker(code),
                              std::memory_order_release);

        Sleep(50);   /* let drainer react */
    }

    /* Return EXCEPTION_CONTINUE_SEARCH so any debugger attached and the
     * OS' default crash handling still kicks in -- this gives us a
     * Windows Error Reporting dialog / coredump as usual. */
    return EXCEPTION_CONTINUE_SEARCH;
}

} /* anonymous namespace */

extern "C" void log_install_crash_handlers(void) {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) return;
    g_prev_filter = SetUnhandledExceptionFilter(handler);
}

extern "C" void log_trigger_test_crash(void) {
    /* DebugBreak raises EXCEPTION_BREAKPOINT.  Not strictly fatal under a
     * debugger, but for our crash test it's the cleanest stand-in.
     * MSVC supports __try/__except (SEH); mingw-w64 with GCC only supports
     * it under specific build configs (and not at all in some homebrew
     * builds). Fall through to ExitProcess if the breakpoint isn't
     * intercepted by a debugger -- our SetUnhandledExceptionFilter handler
     * will still fire if the breakpoint actually crashes the process. */
    /* GCC (mingw-w64) doesn't accept __try/__except even with -fseh -- only
     * MSVC and Clang do. Detect MSVC/Clang and use SEH there; otherwise just
     * fire the breakpoint and exit (the SetUnhandledExceptionFilter handler
     * still catches the real crash path). */
#if defined(_MSC_VER) || (defined(__clang__) && defined(_MSC_EXTENSIONS))
    __try {
        DebugBreak();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ExitProcess(1);
    }
#else
    DebugBreak();
    ExitProcess(1);
#endif
}

#endif /* _WIN32 */
