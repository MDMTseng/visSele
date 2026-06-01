/* log_crash_posix.cpp -- POSIX fatal-signal handlers (Linux + macOS).
 *
 * Async-signal-safety: the handler MAY use:
 *   - plain memory writes to mapped regions
 *   - atomic store / load
 *   - backtrace() / backtrace_symbols_fd() per POSIX (de-facto safe on
 *     glibc and Apple libc)
 *   - sigaction with SA_RESETHAND so re-raising propagates to default
 *     handler
 * It MUST NOT use:
 *   - malloc/free, printf, fprintf
 *   - std::string, std::mutex, std::unordered_map, std::cout, etc.
 *
 * We capture raw stack addresses; the drainer symbolicates in user
 * context.
 */

#if !defined(_WIN32)

#include <log_crash.h>
#include <log_ring.h>
#include <logctrl.h>

#include <atomic>
#include <cstring>
#include <execinfo.h>   /* backtrace() */
#include <signal.h>
#include <unistd.h>

extern "C" void *log_get_shm_ring_mapping(void);

namespace {

std::atomic<bool> g_installed{false};
struct sigaction  g_prev_segv;
struct sigaction  g_prev_abrt;
struct sigaction  g_prev_fpe;
struct sigaction  g_prev_bus;

uint32_t signal_to_marker(int signo) {
    switch (signo) {
        case SIGSEGV: return LOG_CRASH_SIGSEGV;
        case SIGABRT: return LOG_CRASH_SIGABRT;
        case SIGFPE:  return LOG_CRASH_SIGFPE;
        case SIGBUS:  return LOG_CRASH_SIGBUS;
        default:      return LOG_CRASH_OTHER;
    }
}

void handler(int signo, siginfo_t * /*info*/, void * /*ucontext*/) {
    /* Try to write the crash record into the ring.  If no ring is mapped
     * (handlers installed before the ring -- shouldn't happen but defend
     * against it) we just fall through to re-raise so the process dies
     * the normal way. */
    LogRingHeader *h =
        static_cast<LogRingHeader *>(log_get_shm_ring_mapping());
    if (h) {
        h->crash_signal = static_cast<uint32_t>(signo);

        /* Capture the stack.  backtrace() writes raw return addresses into
         * the caller-supplied buffer; max LOG_CRASH_FRAME_MAX = 32.  On
         * Linux/Mac this is async-signal-safe per the de-facto
         * standard. */
        void *frames[LOG_CRASH_FRAME_MAX];
        int n = backtrace(frames, LOG_CRASH_FRAME_MAX);
        if (n < 0) n = 0;
        if (n > (int)LOG_CRASH_FRAME_MAX) n = LOG_CRASH_FRAME_MAX;
        for (int i = 0; i < n; ++i) {
            h->crash_frames[i] = reinterpret_cast<uint64_t>(frames[i]);
        }
        h->crash_frame_count.store(static_cast<uint32_t>(n),
                                   std::memory_order_release);

        /* Bump head to give the drainer a hint that something happened,
         * even if no LOG* was emitted in the handler.  Writes are
         * async-signal-safe (atomic ops). */
        h->head.fetch_add(1, std::memory_order_acq_rel);

        /* Marker last: drainer treats nonzero crash_marker as the trigger
         * for a dump. */
        h->crash_marker.store(signal_to_marker(signo),
                              std::memory_order_release);

        /* Give the drainer ~50ms to react before we die.  Crude but lets
         * the dump happen before the kernel core-dumps the parent. */
        usleep(50 * 1000);
    }

    /* Re-raise via the previous handler / default.  SA_RESETHAND in our
     * sigaction means the OS reset to SIG_DFL before invoking us, so a
     * normal raise() now produces the default fatal behaviour (coredump
     * + exit). */
    raise(signo);
}

void install_one(int signo, struct sigaction *prev) {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(signo, &sa, prev);
}

} /* anonymous namespace */

extern "C" void log_install_crash_handlers(void) {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) return;
    install_one(SIGSEGV, &g_prev_segv);
    install_one(SIGABRT, &g_prev_abrt);
    install_one(SIGFPE,  &g_prev_fpe);
    install_one(SIGBUS,  &g_prev_bus);
}

extern "C" void log_trigger_test_crash(void) {
    /* SIGABRT is the cleanest test signal -- doesn't depend on
     * dereferencing anything, won't be caught by ASan etc. */
    raise(SIGABRT);
}

#endif /* !_WIN32 */
