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
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <windows.h>
#include <dbghelp.h>

extern "C" void *log_get_shm_ring_mapping(void);

namespace {

std::atomic<bool> g_installed{false};
LPTOP_LEVEL_EXCEPTION_FILTER g_prev_filter = nullptr;
std::terminate_handler g_prev_terminate = nullptr;

/* Write a Windows minidump (.dmp -- the "core dump" equivalent) directly to a
 * file from inside the crash handler. This is SYNCHRONOUS and file-based, so it
 * survives even when the async log drainer has already exited (which is why the
 * ring-based backtrace below can go missing). MiniDumpWriteDump is resolved
 * dynamically from dbghelp.dll so there's no link-time dependency.
 *
 * Default dump captures the stack + thread/handle/module info + referenced
 * memory (compact, usually enough for a backtrace + locals). Set
 * INSP_CRASH_FULLDUMP=1 for a full-memory dump (large). Dir from INSP_LOG_DIR,
 * else the cwd. Symbolize a MinGW build with addr2line on the exe (build with
 * RelWithDebInfo/-g for file:line); WinDbg/VS can open the .dmp directly. */
void write_minidump(EXCEPTION_POINTERS *ep) {
    typedef BOOL (WINAPI *PMiniDumpWriteDump)(
        HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION);

    HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
    if (!dbghelp) return;
    PMiniDumpWriteDump pDump =
        (PMiniDumpWriteDump)(void *)GetProcAddress(dbghelp, "MiniDumpWriteDump");
    if (!pDump) return;

    SYSTEMTIME st; GetLocalTime(&st);
    DWORD pid = GetCurrentProcessId();
    const char *dir = getenv("INSP_LOG_DIR");
    char path[MAX_PATH];
    if (dir && *dir)
        snprintf(path, sizeof(path),
                 "%s\\insp_crash_%lu_%04d%02d%02d_%02d%02d%02d.dmp", dir, pid,
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    else
        snprintf(path, sizeof(path),
                 "insp_crash_%lu_%04d%02d%02d_%02d%02d%02d.dmp", pid,
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(
        MiniDumpWithDataSegs | MiniDumpWithThreadInfo |
        MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithHandleData);
    if (const char *f = getenv("INSP_CRASH_FULLDUMP"); f && *f == '1')
        type = (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithHandleData |
                               MiniDumpWithThreadInfo);

    pDump(GetCurrentProcess(), pid, hFile, type, ep ? &mei : nullptr,
          nullptr, nullptr);
    CloseHandle(hFile);

    /* stderr survives even if the drainer is gone -- tell the user where it is. */
    fprintf(stderr, "\n[CRASH] minidump written: %s\n", path);
    fflush(stderr);
}

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

/* Record the main exe's load geometry so the drainer can DWARF-symbolicate our
 * own frames (addr2line): VA = image_base + (frame - module_base), for frames in
 * [module_base, module_base+module_size). Reads the in-memory PE header -- just
 * memory loads + GetModuleHandle(NULL), safe in an exception-filter context. */
static void record_module_geometry(LogRingHeader *h) {
    HMODULE hm = GetModuleHandleW(nullptr);
    h->crash_module_base = reinterpret_cast<uint64_t>(hm);
    h->crash_image_base  = 0;
    h->crash_module_size = 0;
    if (!hm) return;
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(hm);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(
        reinterpret_cast<uint8_t *>(hm) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    h->crash_image_base  = static_cast<uint64_t>(nt->OptionalHeader.ImageBase);
    h->crash_module_size = static_cast<uint64_t>(nt->OptionalHeader.SizeOfImage);
}

LONG WINAPI handler(EXCEPTION_POINTERS *ep) {
    /* Synchronous, file-based core dump first -- survives drainer death. */
    write_minidump(ep);

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
        record_module_geometry(h);

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

/* Uncaught C++ exceptions (cv::Exception, std::runtime_error from JFetEx, ...)
 * call std::terminate, which the SEH unhandled-exception filter above does NOT
 * reliably intercept on mingw-w64. Hook it so those crashes also produce a dump
 * + a backtrace. Runs on the throwing thread before unwinding, so the stack
 * still includes the throw site. */
void terminate_handler() {
    fprintf(stderr, "\n[CRASH] std::terminate() -- uncaught C++ exception\n");
    fflush(stderr);
    write_minidump(nullptr);   /* no EXCEPTION_POINTERS, still dumps this stack */

    LogRingHeader *h =
        static_cast<LogRingHeader *>(log_get_shm_ring_mapping());
    if (h) {
        void *frames[LOG_CRASH_FRAME_MAX];
        USHORT n = RtlCaptureStackBackTrace(
            0, LOG_CRASH_FRAME_MAX, frames, nullptr);
        for (USHORT i = 0; i < n; ++i)
            h->crash_frames[i] = reinterpret_cast<uint64_t>(frames[i]);
        h->crash_frame_count.store(static_cast<uint32_t>(n),
                                   std::memory_order_release);
        record_module_geometry(h);
        h->head.fetch_add(1, std::memory_order_acq_rel);
        h->crash_marker.store(LOG_CRASH_SIGABRT, std::memory_order_release);
        Sleep(50);
    }

    if (g_prev_terminate) g_prev_terminate();
    abort();
}

} /* anonymous namespace */

extern "C" void log_install_crash_handlers(void) {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) return;
    g_prev_filter = SetUnhandledExceptionFilter(handler);
    g_prev_terminate = std::set_terminate(terminate_handler);
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
