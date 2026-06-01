/* inspd_log -- drainer daemon for the InspectionCore log system.
 *
 * Runs as a child of the main visSele process.  Reads the shm ring (see
 * log_ring.h), routes by level:
 *   INFO+    -> rolling disk file (default /var/log/insp/insp.log; 10 MB x 5)
 *   DEBUG/TRACE -> ephemeral in-RAM buffer (drained to disk only on crash;
 *                  Phase G plumbing lands the crash-dump path)
 *
 * Parent-death detection: heartbeat-only (portable).  If LogRingHeader.
 * heartbeat_ms hasn't advanced in HEARTBEAT_TIMEOUT_MS, treat the main
 * process as dead.
 *
 * Phase F.1 scope: drain loop + disk routing + heartbeat watch.
 * Phase F.2 (follow-up): WebSocket server on its own port for WebUI tail.
 * Phase G (follow-up): on crash, dump entire ring + ephemeral buffer to
 *                      crash_<utc>.dump and exit.
 *
 * Cross-platform: smem_channel handles POSIX vs Windows shm; stat / rename
 * for log rotation are portable; usleep -> Sleep handled below.
 */

#include <log_ring.h>
#include <logctrl.h>
#include <sp.hpp>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <string>

#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
  #include <dbghelp.h>
  static void thread_sleep_ms(int ms) { Sleep(ms); }
  static int  mkdir_p_one(const char *p) { return _mkdir(p); }
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <execinfo.h>
  static void thread_sleep_ms(int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
  }
  static int mkdir_p_one(const char *p) { return mkdir(p, 0755); }
#endif

namespace {

/* ---------- defaults / config ---------- */

constexpr int HEARTBEAT_TIMEOUT_MS = 3000;  /* parent considered dead after */
constexpr int POLL_INTERVAL_MS     = 50;    /* idle wakeup interval */
constexpr int EPHEMERAL_CAP        = 16384; /* DEBUG/TRACE in-RAM cap (lines) */

struct Config {
    std::string ring_name      = "insp_log_ring";
    int         ring_mb        = 16;
    std::string log_dir        = ".";
    std::string log_basename   = "insp.log";
    int         rotate_bytes   = 10 * 1024 * 1024;
    int         rotate_keep    = 5;
    int         persist_min_lv = LOG_LV_INFO;
};

void env_load(Config &c) {
    if (const char *e = std::getenv("INSP_LOG_RING_NAME")) c.ring_name = e;
    if (const char *e = std::getenv("INSP_LOG_RING_MB"))   c.ring_mb   = std::atoi(e);
    if (const char *e = std::getenv("INSP_LOG_DIR"))       c.log_dir   = e;
    if (const char *e = std::getenv("INSP_LOG_FILE"))      c.log_basename = e;
    if (const char *e = std::getenv("INSP_LOG_ROTATE_MB"))
        c.rotate_bytes = std::atoi(e) * 1024 * 1024;
    if (const char *e = std::getenv("INSP_LOG_ROTATE_KEEP"))
        c.rotate_keep = std::atoi(e);
    if (const char *e = std::getenv("INSP_LOG_PERSIST_LEVEL")) {
        std::string s(e);
        if      (s == "trace" || s == "v") c.persist_min_lv = LOG_LV_TRACE;
        else if (s == "debug" || s == "d") c.persist_min_lv = LOG_LV_DEBUG;
        else if (s == "info"  || s == "i") c.persist_min_lv = LOG_LV_INFO;
        else if (s == "warn"  || s == "w") c.persist_min_lv = LOG_LV_WARN;
        else if (s == "error" || s == "e") c.persist_min_lv = LOG_LV_ERROR;
        else if (s == "off")               c.persist_min_lv = LOG_LV_OFF;
    }
}

/* ---------- log file with size-based rotation ---------- */

class RollingLogFile {
    std::string dir_, basename_;
    int rotate_bytes_, rotate_keep_;
    FILE *fp_      = nullptr;
    long   cur_sz_ = 0;

    std::string path_(int gen) const {
        if (gen == 0) return dir_ + "/" + basename_;
        char buf[32]; std::snprintf(buf, sizeof(buf), ".%d", gen);
        return dir_ + "/" + basename_ + buf;
    }

public:
    RollingLogFile(const std::string &dir, const std::string &basename,
                   int rotate_bytes, int rotate_keep)
        : dir_(dir), basename_(basename),
          rotate_bytes_(rotate_bytes), rotate_keep_(rotate_keep) {
        /* mkdir -p (single level only for now -- callers should pass a dir
         * that already exists or is at most one level deep). */
        mkdir_p_one(dir_.c_str());
        std::string p = path_(0);
        fp_ = std::fopen(p.c_str(), "a");
        if (fp_) {
            std::fseek(fp_, 0, SEEK_END);
            cur_sz_ = std::ftell(fp_);
        } else {
            std::fprintf(stderr,
                "[inspd_log] cannot open %s for append\n", p.c_str());
        }
    }
    ~RollingLogFile() { if (fp_) std::fclose(fp_); }

    void write(const char *s, size_t n) {
        if (!fp_) return;
        if (cur_sz_ >= rotate_bytes_) rotate_();
        std::fwrite(s, 1, n, fp_);
        cur_sz_ += (long)n;
    }

    void flush() { if (fp_) std::fflush(fp_); }

private:
    void rotate_() {
        if (fp_) { std::fclose(fp_); fp_ = nullptr; }
        /* delete the oldest, shift the rest down */
        for (int g = rotate_keep_; g > 0; --g) {
            std::string from = path_(g - 1);
            std::string to   = path_(g);
            if (g == rotate_keep_) std::remove(to.c_str());
            std::rename(from.c_str(), to.c_str());
        }
        std::string p = path_(0);
        fp_ = std::fopen(p.c_str(), "a");
        cur_sz_ = 0;
    }
};

/* ---------- ephemeral buffer for DEBUG/TRACE ---------- */

struct EphemeralBuf {
    std::deque<std::string> q;
    size_t cap;
    explicit EphemeralBuf(size_t c) : cap(c) {}
    void push(std::string s) {
        if (q.size() >= cap) q.pop_front();
        q.push_back(std::move(s));
    }
};

/* ---------- crash dump writer ---------- */

const char *crash_marker_name(uint32_t m) {
    switch (m) {
        case LOG_CRASH_SIGSEGV: return "SIGSEGV";
        case LOG_CRASH_SIGABRT: return "SIGABRT";
        case LOG_CRASH_SIGFPE:  return "SIGFPE";
        case LOG_CRASH_SIGBUS:  return "SIGBUS";
        case LOG_CRASH_OTHER:   return "OTHER";
        default:                return "?";
    }
}

/* Cross-platform symbol-resolution for one address.  Returns into out[]
 * a printable "0x<addr>  <symbol> (<file:line if known>)" string.  Falls
 * back to raw hex if symbolication fails. */
void symbolicate_one(uint64_t addr, char *out, size_t outsz) {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
        initialized = true;
    }
    SYMBOL_INFO *sym = (SYMBOL_INFO *)calloc(
        sizeof(SYMBOL_INFO) + 256, 1);
    if (sym) {
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;
        DWORD64 disp64 = 0;
        bool ok = SymFromAddr(GetCurrentProcess(),
                              (DWORD64)addr, &disp64, sym);
        if (ok) {
            IMAGEHLP_LINE64 line = {};
            line.SizeOfStruct = sizeof(line);
            DWORD disp32 = 0;
            if (SymGetLineFromAddr64(GetCurrentProcess(),
                                     (DWORD64)addr, &disp32, &line)) {
                std::snprintf(out, outsz, "0x%016llx  %s  (%s:%lu)",
                              (unsigned long long)addr, sym->Name,
                              line.FileName, line.LineNumber);
            } else {
                std::snprintf(out, outsz, "0x%016llx  %s",
                              (unsigned long long)addr, sym->Name);
            }
            free(sym);
            return;
        }
        free(sym);
    }
    std::snprintf(out, outsz, "0x%016llx", (unsigned long long)addr);
#else
    /* backtrace_symbols formats "binary(symbol+offset) [addr]" or similar.
     * We only need to feed it one address; allocate, copy, free. */
    void *p = reinterpret_cast<void *>(addr);
    char **syms = backtrace_symbols(&p, 1);
    if (syms && syms[0]) {
        std::snprintf(out, outsz, "0x%016llx  %s",
                      (unsigned long long)addr, syms[0]);
        free(syms);
    } else {
        std::snprintf(out, outsz, "0x%016llx", (unsigned long long)addr);
    }
#endif
}

/* Write the crash dump file: header + stack trace + entire ring + the
 * drainer's ephemeral DEBUG/TRACE buffer.  Format is plaintext so it can
 * be opened in any editor / streamed to WebUI as-is. */
void write_crash_dump(const Config &cfg,
                      LogRingHeader *h,
                      const EphemeralBuf &eph,
                      uint32_t marker) {
    /* Filename: crash_<utc>.dump in the log dir.  Use system clock so the
     * stamp matches what a human sees in syslog. */
    time_t now = std::time(nullptr);
    char ts[64];
    std::strftime(ts, sizeof(ts), "%Y%m%dT%H%M%SZ", std::gmtime(&now));
    char fname[256];
    std::snprintf(fname, sizeof(fname),
                  "%s/crash_%s.dump", cfg.log_dir.c_str(), ts);

    FILE *fp = std::fopen(fname, "w");
    if (!fp) {
        std::fprintf(stderr,
            "[inspd_log] cannot open crash dump '%s'\n", fname);
        return;
    }

    std::fprintf(fp, "=== InspectionCore crash dump ===\n");
    std::fprintf(fp, "timestamp: %s\n", ts);
    std::fprintf(fp, "signal:    %s (raw=%u)\n",
                 crash_marker_name(marker), h->crash_signal);
    std::fprintf(fp, "pid:       %ld\n",
#ifdef _WIN32
                 (long)GetCurrentProcessId()
#else
                 (long)getppid()  /* parent pid -- this is the drainer's pid view */
#endif
                 );
    std::fprintf(fp, "\n");

    /* Stack trace. */
    uint32_t nf = h->crash_frame_count.load(std::memory_order_acquire);
    if (nf > LOG_CRASH_FRAME_MAX) nf = LOG_CRASH_FRAME_MAX;
    std::fprintf(fp, "--- Stack trace (%u frames) ---\n", nf);
    for (uint32_t i = 0; i < nf; ++i) {
        char line[512];
        symbolicate_one(h->crash_frames[i], line, sizeof(line));
        std::fprintf(fp, "#%-2u %s\n", i, line);
    }
    std::fprintf(fp, "\n");

    /* Ring tail-to-head dump (everything the producer wrote). */
    std::fprintf(fp, "--- Ring (entire retained history, incl. verbose) ---\n");
    uint64_t head = h->head.load(std::memory_order_acquire);
    uint64_t tail = (head > h->slot_count) ? head - h->slot_count : 0;
    uint32_t emitted = 0;
    for (uint64_t i = tail; i < head; ++i) {
        LogSlot *slot = log_ring_slot(h, i);
        uint64_t before = slot->seq.load(std::memory_order_acquire);
        if (before & 1) continue;
        char text[LOG_SLOT_TEXT];
        std::memcpy(text, slot->text, LOG_SLOT_TEXT);
        uint64_t after = slot->seq.load(std::memory_order_acquire);
        if (after != before) continue;
        std::fputs(text, fp);
        emitted++;
    }
    std::fprintf(fp, "(%u lines)\n\n", emitted);

    /* Ephemeral buffer (the DEBUG/TRACE that never went to disk -- this
     * is the post-mortem prize). */
    std::fprintf(fp, "--- Ephemeral DEBUG/TRACE buffer (%zu lines) ---\n",
                 eph.q.size());
    for (auto &s : eph.q) std::fputs(s.c_str(), fp);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "=== end of dump ===\n");
    std::fclose(fp);
    std::fprintf(stderr, "[inspd_log] crash dump written to %s\n", fname);
}

/* ---------- main drain loop ---------- */

int run(const Config &cfg) {
    /* Try attach (created by main); retry briefly in case we win the race. */
    ShareMemoryInfo info = {};
    size_t need = log_ring_total_bytes(
        (uint32_t)((size_t)cfg.ring_mb * 1024 * 1024 - LOG_HEADER_BYTES)
            / LOG_SLOT_BYTES);
    int rc = -1;
    for (int t = 0; t < 100; ++t) {
        rc = connSharedMemory(cfg.ring_name, need, &info);
        if (rc == 0) break;
        thread_sleep_ms(50);
    }
    if (rc != 0 || !info.ptr) {
        std::fprintf(stderr,
            "[inspd_log] cannot attach shm '%s' after 5s\n",
            cfg.ring_name.c_str());
        return 1;
    }

    auto *h = reinterpret_cast<LogRingHeader *>(info.ptr);
    if (h->magic != LOG_RING_MAGIC) {
        std::fprintf(stderr, "[inspd_log] shm header magic mismatch\n");
        return 1;
    }
    std::fprintf(stderr,
        "[inspd_log] attached '%s' (slots=%u, version=%u)\n",
        cfg.ring_name.c_str(), h->slot_count, h->version);

    RollingLogFile disk(cfg.log_dir, cfg.log_basename,
                        cfg.rotate_bytes, cfg.rotate_keep);
    EphemeralBuf ephemeral(EPHEMERAL_CAP);

    /* Tail starts at the oldest still-in-ring slot.  This lets us catch up
     * on lines written between log_open_shm_ring() and our attach.  For a
     * 16 MB / 65535-slot ring on a freshly-launched process, head starts at
     * 0 so tail starts at 0; for an already-warm ring tail starts at
     * head-slot_count so we drain everything currently retained. */
    uint64_t head_now = h->head.load(std::memory_order_acquire);
    uint64_t tail = (head_now > h->slot_count)
                      ? head_now - h->slot_count
                      : 0;
    uint64_t last_heartbeat = h->heartbeat_ms.load(std::memory_order_relaxed);
    auto last_heartbeat_seen = std::chrono::steady_clock::now();
    bool parent_dead = false;

    while (!parent_dead) {
        uint64_t head = h->head.load(std::memory_order_acquire);

        /* Crash marker -- drain whatever we can, then dump and exit. */
        uint32_t cm = h->crash_marker.load(std::memory_order_acquire);
        if (cm != LOG_CRASH_NONE) {
            std::fprintf(stderr,
                "[inspd_log] crash marker %u observed; writing dump\n", cm);
            /* Drain final logs first so the ring contains everything the
             * producer wrote before / during the crash. */
            uint64_t head_now = h->head.load(std::memory_order_acquire);
            for (; tail < head_now; ++tail) {
                LogSlot *slot = log_ring_slot(h, tail);
                uint64_t before = slot->seq.load(std::memory_order_acquire);
                if (before & 1) continue;
                char text[LOG_SLOT_TEXT];
                std::memcpy(text, slot->text, LOG_SLOT_TEXT);
                int lv = slot->level;
                uint64_t after = slot->seq.load(std::memory_order_acquire);
                if (after != before) continue;
                size_t tlen = std::strlen(text);
                if (lv >= cfg.persist_min_lv) disk.write(text, tlen);
                else ephemeral.push(std::string(text, tlen));
            }
            disk.flush();
            write_crash_dump(cfg, h, ephemeral, cm);
            return 0;
        }

        /* Heartbeat watch: if no producer-side activity, main is dead. */
        uint64_t hb = h->heartbeat_ms.load(std::memory_order_relaxed);
        auto now = std::chrono::steady_clock::now();
        if (hb != last_heartbeat) {
            last_heartbeat = hb;
            last_heartbeat_seen = now;
        } else {
            auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - last_heartbeat_seen).count();
            if (idle_ms > HEARTBEAT_TIMEOUT_MS && tail == head) {
                std::fprintf(stderr,
                    "[inspd_log] parent heartbeat stalled %lld ms; exiting\n",
                    (long long)idle_ms);
                parent_dead = true;
            }
        }

        if (tail == head) {
            thread_sleep_ms(POLL_INTERVAL_MS);
            continue;
        }

        /* Drain [tail, head).  Skip slots that overran us (producer ran
         * ahead by more than slot_count). */
        if (head - tail > h->slot_count) {
            uint64_t skipped = (head - tail) - h->slot_count;
            std::fprintf(stderr,
                "[inspd_log] producer outran consumer; skipping %llu slots\n",
                (unsigned long long)skipped);
            tail = head - h->slot_count;
        }

        for (; tail < head; ++tail) {
            LogSlot *slot = log_ring_slot(h, tail);
            /* Tear-detection seq protocol (see log_ring.h). */
            uint64_t before = slot->seq.load(std::memory_order_acquire);
            if (before & 1) {
                /* mid-write; backoff one slot and retry next iter */
                break;
            }
            /* Copy the payload onto the stack so the recheck is meaningful. */
            char text[LOG_SLOT_TEXT];
            std::memcpy(text, slot->text, LOG_SLOT_TEXT);
            int   lv   = slot->level;
            uint64_t after = slot->seq.load(std::memory_order_acquire);
            if (after != before) {
                /* The slot got overwritten while we were copying; the only
                 * safe thing is to advance and accept the loss.  This only
                 * happens when the producer has wrapped past us. */
                continue;
            }

            size_t tlen = std::strlen(text);
            if (lv >= cfg.persist_min_lv) {
                disk.write(text, tlen);
            } else {
                ephemeral.push(std::string(text, tlen));
            }
        }
        disk.flush();
    }

    /* Final drain on the way out: parent declared dead, but there may still
     * be uncommitted-to-disk lines we haven't routed yet. */
    {
        uint64_t head_final = h->head.load(std::memory_order_acquire);
        for (; tail < head_final; ++tail) {
            LogSlot *slot = log_ring_slot(h, tail);
            uint64_t before = slot->seq.load(std::memory_order_acquire);
            if (before & 1) continue;
            char text[LOG_SLOT_TEXT];
            std::memcpy(text, slot->text, LOG_SLOT_TEXT);
            int lv = slot->level;
            uint64_t after = slot->seq.load(std::memory_order_acquire);
            if (after != before) continue;
            if (lv >= cfg.persist_min_lv)
                disk.write(text, std::strlen(text));
        }
        disk.flush();
    }

    return 0;
}

} /* anonymous namespace */

int main(int argc, char **argv) {
    Config cfg;
    env_load(cfg);

    /* Crude argv support: --ring NAME --dir PATH --mb N */
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ring") cfg.ring_name = argv[++i];
        else if (a == "--dir")  cfg.log_dir   = argv[++i];
        else if (a == "--mb")   cfg.ring_mb   = std::atoi(argv[++i]);
    }

    return run(cfg);
}
