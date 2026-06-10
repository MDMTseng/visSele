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
#include "inspd_log_ws.h"

#include <log_modules_region.h>
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
    /* Disk-protection default: only WARN+ trickles to insp.log so a 24/7
     * machine barely writes the disk. INFO/DEBUG stay in the RAM ring and can
     * be captured in full on demand (log_request_dump -> crash_<utc>.dump) or
     * on a real crash. Override with INSP_LOG_PERSIST_LEVEL=info while debugging. */
    int         persist_min_lv = LOG_LV_WARN;
    /* WS log-stream server for the WebUI "Core Logs" panel
     * (CoreLogClient -> ws://127.0.0.1:4091/log). On by default now that the
     * daemon is; set INSP_LOG_WS_PORT=0 to disable. */
    int         ws_port        = 4091;
};

/* Parse a producer-formatted log line into structured fields.
 *
 * Format (from logctrl.cpp's emit path):
 *   [<time>][<L>][<module       >][<file>:<line> <func>] <body>\n
 * Example:
 *   [     4.585][E][core          ][wiringPanel.cpp     :5008 cp_main] msg
 *
 * Mutates `buf` in place (writes NULs at section boundaries) and points
 * the LogRecord string fields into it.  The buffer must outlive the
 * LogRecord use.  Returns true on a successful parse.
 *
 * On failure (truncated / malformed), falls back to setting body=buf and
 * leaving the rest empty -- the line still goes out, just unstructured.
 */
bool parse_log_line(char *buf, int slot_line, LogRecord *out) {
    out->module = "";
    out->file = "";
    out->function = "";
    out->line = slot_line;
    out->body = buf;

    char *p = buf;
    if (*p != '[') return false;
    char *p1 = std::strchr(p, ']');  if (!p1) return false;          /* end of time */
    /* Parse the float "ssss.mmm" between p and p1 into ms-since-start. */
    {
        char saved = *p1; *p1 = '\0';
        double secs = std::atof(p + 1);
        *p1 = saved;
        if (secs >= 0.0) out->ts_ms_since_start = (uint64_t)(secs * 1000.0);
    }
    char *p2 = std::strchr(p1+1, ']'); if (!p2) return false;        /* end of [L] */
    char *mod_start = p2 + 1;
    if (*mod_start != '[') return false;
    ++mod_start;
    char *p3 = std::strchr(mod_start, ']'); if (!p3) return false;   /* end of module */
    *p3 = '\0';
    out->module = mod_start;
    /* trim trailing spaces from module */
    for (char *t = p3 - 1; t >= mod_start && *t == ' '; --t) *t = '\0';

    char *file_start = p3 + 1;
    if (*file_start != '[') return false;
    ++file_start;
    char *colon = std::strchr(file_start, ':'); if (!colon) return false;
    *colon = '\0';
    out->file = file_start;
    for (char *t = colon - 1; t >= file_start && *t == ' '; --t) *t = '\0';

    char *line_start = colon + 1;
    char *space = std::strchr(line_start, ' '); if (!space) return false;
    *space = '\0';
    /* keep slot_line (authoritative); but also parseable: out->line = atoi(line_start) */

    char *func_start = space + 1;
    while (*func_start == ' ') ++func_start;
    char *p4 = std::strchr(func_start, ']'); if (!p4) return false;  /* end of fileinfo */
    *p4 = '\0';
    out->function = func_start;

    char *body = p4 + 1;
    while (*body == ' ') ++body;
    /* strip trailing newline */
    size_t bl = std::strlen(body);
    while (bl > 0 && (body[bl-1] == '\n' || body[bl-1] == '\r')) body[--bl] = '\0';
    out->body = body;
    return true;
}

const char *severity_text_for(int lv) {
    switch (lv) {
        case LOG_LV_TRACE: return "TRACE";
        case LOG_LV_DEBUG: return "DEBUG";
        case LOG_LV_INFO:  return "INFO";
        case LOG_LV_WARN:  return "WARN";
        case LOG_LV_ERROR: return "ERROR";
        case LOG_LV_FATAL: return "FATAL";
        default:           return "?";
    }
}

/* OTel severityNumber per docs/LOGGING_WEBUI.md. */
int severity_number_for(int lv) {
    switch (lv) {
        case LOG_LV_TRACE: return 1;
        case LOG_LV_DEBUG: return 5;
        case LOG_LV_INFO:  return 9;
        case LOG_LV_WARN:  return 13;
        case LOG_LV_ERROR: return 17;
        case LOG_LV_FATAL: return 21;
        default:           return 0;
    }
}

void env_load(Config &c) {
    if (const char *e = std::getenv("INSP_LOG_RING_NAME")) c.ring_name = e;
    if (const char *e = std::getenv("INSP_LOG_RING_MB"))   c.ring_mb   = std::atoi(e);
    if (const char *e = std::getenv("INSP_LOG_DIR"))       c.log_dir   = e;
    if (const char *e = std::getenv("INSP_LOG_FILE"))      c.log_basename = e;
    if (const char *e = std::getenv("INSP_LOG_ROTATE_MB"))
        c.rotate_bytes = std::atoi(e) * 1024 * 1024;
    if (const char *e = std::getenv("INSP_LOG_ROTATE_KEEP"))
        c.rotate_keep = std::atoi(e);
    if (const char *e = std::getenv("INSP_LOG_WS_PORT")) c.ws_port = std::atoi(e);
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
std::string write_crash_dump(const Config &cfg,
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
        return std::string();
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
    return std::string(fname);
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

    /* Optional WebSocket bridge for WebUI live tail.  Default: off. */
    LogWsServer *ws = nullptr;
    uint64_t started_unix_nano = 0;
    if (cfg.ws_port > 0) {
        LogWsServer::HelloInfo hello;
        hello.ring_version = h->version;
        hello.ring_slots   = h->slot_count;
        hello.ring_mb      = cfg.ring_mb;
        /* Prefer the producer's own wall-clock anchor so timestamps line up
         * with what the producer would print to its stderr sink.  Fall
         * back to drainer-attach time only for legacy rings (field=0). */
        if (h->producer_started_unix_nano != 0) {
            started_unix_nano = h->producer_started_unix_nano;
        } else {
            struct timespec tspec;
            clock_gettime(CLOCK_REALTIME, &tspec);
            started_unix_nano =
                (uint64_t)tspec.tv_sec * 1000000000ULL + (uint64_t)tspec.tv_nsec;
        }
        hello.started_unix_nano = started_unix_nano;
        hello.log_dir      = cfg.log_dir;
        hello.service_name = "visSele";
#ifdef _WIN32
        hello.producer_pid = (long)GetCurrentProcessId();
        char hn[256]; DWORD hns = sizeof(hn);
        if (GetComputerNameA(hn, &hns)) hello.host_name = hn;
#else
        hello.producer_pid = (long)getppid();
        char hn[256] = {0};
        if (gethostname(hn, sizeof(hn)-1) == 0) hello.host_name = hn;
#endif
        ws = new LogWsServer(cfg.ws_port, hello);

        /* Attach modules region (#30).  Best-effort -- if the producer
         * hasn't created it yet we just retry on first getModules. */
        static LogModulesRegion *modules_rgn = nullptr;
        static std::string modules_name = cfg.ring_name + "_modules";
        auto attach_modules = [&]() -> LogModulesRegion * {
            if (modules_rgn) return modules_rgn;
            ShareMemoryInfo mi = {};
            if (connSharedMemory(modules_name, LOG_MODULES_REGION_BYTES, &mi) == 0
                && mi.ptr) {
                auto *r = static_cast<LogModulesRegion *>(mi.ptr);
                if (r->magic == LOG_MODULES_MAGIC) modules_rgn = r;
            }
            return modules_rgn;
        };
        attach_modules();

        ws->set_get_modules_handler(
            [&attach_modules](std::vector<std::string> &names,
                              std::vector<int> &levels) {
                auto *r = attach_modules();
                if (!r) return;
                /* Seq tear protocol: retry if mid-write or seq changed. */
                for (int attempt = 0; attempt < 4; ++attempt) {
                    uint64_t before = r->seq.load(std::memory_order_acquire);
                    if (before & 1) { thread_sleep_ms(1); continue; }
                    uint32_t n = r->count;
                    if (n > LOG_MODULES_MAX) n = LOG_MODULES_MAX;
                    names.clear(); levels.clear();
                    names.reserve(n); levels.reserve(n);
                    for (uint32_t i = 0; i < n; ++i) {
                        names.emplace_back(r->modules[i].name);
                        levels.push_back(r->modules[i].level);
                    }
                    uint64_t after = r->seq.load(std::memory_order_acquire);
                    if (after == before) return;
                    /* torn -- retry */
                }
                names.clear(); levels.clear();
            });

        /* Backlog provider: walk the ring from (head - tail_n) up to head,
         * parsing each slot.  Owned-text vector keeps the parsed strings
         * alive until the chunk is serialized. */
        /* dumpNow: synthesize a "no-crash" dump on demand for support
         * snapshots.  Reuses write_crash_dump with LOG_CRASH_OTHER. */
        ws->set_dump_handler([&cfg, h, &ephemeral]() -> std::string {
            return write_crash_dump(cfg, h, ephemeral, LOG_CRASH_OTHER);
        });

        /* setLevel: convert OTel severityNumber to producer's LOG_LV_*,
         * write the command into the ring header, bump cmd_seq so the
         * producer notices on next emit. */
        ws->set_level_handler([h](const char *scope, const char *module,
                                  int sn) -> bool {
            int lv;
            if      (sn <= 1)  lv = LOG_LV_TRACE;
            else if (sn <= 5)  lv = LOG_LV_DEBUG;
            else if (sn <= 9)  lv = LOG_LV_INFO;
            else if (sn <= 13) lv = LOG_LV_WARN;
            else if (sn <= 17) lv = LOG_LV_ERROR;
            else               lv = LOG_LV_FATAL;
            h->ctrl_cmd_level = lv;
            std::memset(h->ctrl_cmd_module, 0, sizeof(h->ctrl_cmd_module));
            if (std::strcmp(scope, "global") == 0) {
                /* leave module empty -> global */
            } else if (module && *module) {
                std::strncpy(h->ctrl_cmd_module, module,
                             sizeof(h->ctrl_cmd_module) - 1);
            } else {
                return false;
            }
            h->ctrl_cmd_seq.fetch_add(1, std::memory_order_release);
            return true;
        });

        ws->set_backlog_provider(
            [h, started_unix_nano](int tail_n,
                                   std::vector<LogRecord> &out,
                                   std::vector<std::string> &owned) {
                uint64_t head = h->head.load(std::memory_order_acquire);
                if (head == 0) return;
                uint64_t max_back = std::min<uint64_t>(tail_n, h->slot_count);
                uint64_t start = (head > max_back) ? head - max_back : 0;
                /* Allocate up-front so string addresses don't invalidate. */
                owned.reserve(static_cast<size_t>(head - start));
                out.reserve(static_cast<size_t>(head - start));
                for (uint64_t i = start; i < head; ++i) {
                    LogSlot *slot = log_ring_slot(h, i);
                    uint64_t before = slot->seq.load(std::memory_order_acquire);
                    if (before & 1) continue;
                    char text[LOG_SLOT_TEXT];
                    std::memcpy(text, slot->text, LOG_SLOT_TEXT);
                    int lv = slot->level;
                    int ln = slot->line;
                    uint64_t after = slot->seq.load(std::memory_order_acquire);
                    if (after != before) continue;
                    owned.emplace_back(text);
                    LogRecord rec{};
                    parse_log_line(&owned.back()[0], ln, &rec);
                    rec.time_unix_nano  = started_unix_nano
                                          + rec.ts_ms_since_start * 1000000ULL;
                    rec.severity_number = severity_number_for(lv);
                    rec.severity_text   = severity_text_for(lv);
                    out.push_back(rec);
                }
            });
    }

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

#ifdef _WIN32
    /* Reliable parent-death detection: poll the producer's process handle. The
     * handle becomes signaled the instant the producer exits -- unlike the
     * heartbeat, it never false-positives when the producer is alive but idle. */
    HANDLE parent_h = nullptr;
    if (const char *pp = std::getenv("INSP_LOG_PARENT_PID")) {
        DWORD ppid = (DWORD)std::strtoul(pp, nullptr, 10);
        if (ppid) parent_h = OpenProcess(SYNCHRONIZE, FALSE, ppid);
    }
#endif

    while (!parent_dead) {
        uint64_t head = h->head.load(std::memory_order_acquire);

        /* Crash marker -- drain whatever we can, then dump. A real crash dumps
         * and EXITS; an on-demand snapshot (LOG_CRASH_DUMP_REQUEST) dumps,
         * resets the marker, and KEEPS RUNNING. */
        uint32_t cm = h->crash_marker.load(std::memory_order_acquire);
        if (cm != LOG_CRASH_NONE) {
            bool on_demand = (cm == LOG_CRASH_DUMP_REQUEST);
            std::fprintf(stderr, "[inspd_log] %s marker %u; writing dump\n",
                         on_demand ? "on-demand dump" : "crash", cm);
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
            std::string dump_path = write_crash_dump(cfg, h, ephemeral,
                                       on_demand ? LOG_CRASH_OTHER : cm);
            if (on_demand) {
                /* Acknowledge so we don't re-dump every poll, then keep running. */
                h->crash_marker.store(LOG_CRASH_NONE, std::memory_order_release);
                std::fprintf(stderr, "[inspd_log] on-demand dump -> %s\n",
                             dump_path.c_str());
                continue;
            }
            if (ws) {
                struct timespec tspec;
                clock_gettime(CLOCK_REALTIME, &tspec);
                uint64_t now_ns =
                    (uint64_t)tspec.tv_sec * 1000000000ULL +
                    (uint64_t)tspec.tv_nsec;
                ws->push_crash(crash_marker_name(cm), (int)h->crash_signal,
                               now_ns, dump_path.c_str());
                /* One last tick to flush the crash frames out the socket. */
                struct timeval tv = { 0, 50000 };
                ws->tick(&tv);
                delete ws;
            }
            return 0;
        }

        /* Parent-death detection.
         *
         * The old approach watched heartbeat_ms (producer-side, bumped on
         * every log_emit), but that conflated "process dead" with "process
         * alive but quiet" -- and visSele can go genuinely idle for tens
         * of seconds at a time waiting for clients.  We now use real OS
         * signals.
         *
         * POSIX: when the parent dies, the kernel reparents us to launchd
         * (pid 1).  getppid() == 1 is an unambiguous death signal that
         * works even for SIGKILL / OOM-kill where no handler could run.
         *
         * Windows: TODO -- needs Job Object + JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO
         * or OpenProcess + GetExitCodeProcess polling.  For now we keep
         * the (now-very-generous) heartbeat fallback so a stalled producer
         * isn't immortal.
         */
#ifndef _WIN32
        if (getppid() == 1) {
            std::fprintf(stderr,
                "[inspd_log] parent process gone (reparented to init); exiting\n");
            parent_dead = true;
        }
        (void)last_heartbeat; (void)last_heartbeat_seen;
#else
        auto now = std::chrono::steady_clock::now();
        if (parent_h) {
            /* Authoritative: signaled the moment the producer exits. No false
             * positive for an alive-but-idle producer. */
            if (WaitForSingleObject(parent_h, 0) == WAIT_OBJECT_0) {
                std::fprintf(stderr, "[inspd_log] parent process exited; exiting\n");
                parent_dead = true;
            }
            (void)last_heartbeat; (void)last_heartbeat_seen;
        } else {
            /* Fallback only when we couldn't get the parent PID: generous
             * heartbeat watch (a quiet-but-alive producer can still trip this --
             * which is exactly why we prefer the handle above). */
            uint64_t hb = h->heartbeat_ms.load(std::memory_order_relaxed);
            if (hb != last_heartbeat) {
                last_heartbeat = hb;
                last_heartbeat_seen = now;
            } else {
                auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   now - last_heartbeat_seen).count();
                if (idle_ms > 60000 && tail == head) {
                    std::fprintf(stderr,
                        "[inspd_log] parent heartbeat stalled %lld ms; exiting\n",
                        (long long)idle_ms);
                    parent_dead = true;
                }
            }
        }
#endif

        /* Drive the WS server.  If enabled, its tick() does the select()
         * so we don't double-sleep; otherwise fall back to a plain sleep. */
        if (ws) {
            struct timeval tv;
            tv.tv_sec  = 0;
            tv.tv_usec = (tail == head) ? POLL_INTERVAL_MS * 1000 : 0;
            ws->tick(&tv);
        } else if (tail == head) {
            thread_sleep_ms(POLL_INTERVAL_MS);
        }
        if (tail == head) continue;

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
            /* Fan out to WS subscribers.  Parser mutates a scratch copy. */
            if (ws && ws->peer_count() > 0) {
                char scratch[LOG_SLOT_TEXT];
                std::memcpy(scratch, text, tlen + 1);
                LogRecord rec{};
                parse_log_line(scratch, slot->line, &rec);
                rec.time_unix_nano  = started_unix_nano
                                      + rec.ts_ms_since_start * 1000000ULL;
                rec.severity_number = severity_number_for(lv);
                rec.severity_text   = severity_text_for(lv);
                ws->broadcast_log(rec);
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

    if (ws) delete ws;
    return 0;
}

} /* anonymous namespace */

int main(int argc, char **argv) {
#ifdef _WIN32
    /* The WS log server needs Winsock initialised. The producer calls
     * WSAStartup, but the drainer is a SEPARATE process -- without this its
     * socket() fails and "listening on 4091" is a lie (nothing binds, so the
     * WebUI Core Logs panel can never connect). */
    { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); }
#endif
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
