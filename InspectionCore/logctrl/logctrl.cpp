/* logctrl.cpp -- C++ implementation of the unified logging system.
 *
 * Cross-platform single-source via the C++ stdlib:
 *   std::chrono::steady_clock for monotonic timestamps
 *   std::mutex for the coarse emit mutex
 *   std::atomic via volatile-int (callable from C; aligned int loads are
 *     atomic on every platform we ship to)
 *
 * Only platform-specific code: isatty() vs _isatty().
 */

#include <logctrl.h>
#include <log_ring.h>
#include <sp.hpp>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
  #include <io.h>
  #define LOG_ISATTY_FN(fd)  _isatty(fd)
  #define LOG_FILENO_FN(fp)  _fileno(fp)
#else
  #include <unistd.h>
  #define LOG_ISATTY_FN(fd)  isatty(fd)
  #define LOG_FILENO_FN(fp)  fileno(fp)
#endif

/* The atomic-ish cached global level the macros compare against. INFO
 * default; suppresses anything below "production state changes". */
extern "C" volatile int _log_global_level_cache = LOG_LV_INFO;

namespace {

std::mutex g_log_mutex;
const std::chrono::steady_clock::time_point g_log_t0 =
    std::chrono::steady_clock::now();

struct TagLevel { std::string tag; int lv; };
std::vector<TagLevel> g_tag_levels;
int g_global_level_requested = LOG_LV_INFO;  /* what the user/env asked for */

/* Re-derive _log_global_level_cache as min(requested, any tag override).
 * Called under g_log_mutex whenever the global request or tag set changes. */
void recompute_cache_locked() {
    int min_lv = g_global_level_requested;
    for (auto &t : g_tag_levels) if (t.lv < min_lv) min_lv = t.lv;
    _log_global_level_cache = min_lv;
}

/* TU -> module name registry.  Populated by LOG_MODULE() static initialisers
 * during program load (before main).  Static-init order across TUs is not
 * guaranteed, so we wrap the map in a function-local static (Meyer's
 * singleton) -- guaranteed constructed on first use.  Reads are done under
 * g_log_mutex. */
inline std::unordered_map<std::string, std::string> &file_to_module() {
    static std::unordered_map<std::string, std::string> m;
    return m;
}

struct SinkEntry {
    int id;
    log_sink_fn fn;
    void *ctx;
};
std::vector<SinkEntry> g_sinks;
int g_next_sink_id = 1;

bool g_stderr_enabled = true;
int  g_stderr_is_tty  = -1;   /* -1 = not yet detected; 0/1 = cached */
bool g_env_parsed     = false;

/* SHM ring state (Phase A2).  Held under g_log_mutex while emit walks
 * sinks; producer-only on the writer side. */
struct ShmRingState {
    ShareMemoryInfo info;        /* from contrib/smem_channel */
    LogRingHeader  *hdr;         /* mapped header */
    int             sink_id;     /* id returned by log_register_sink */
    bool            owns_unlink; /* if we created it, we shm_unlink on close */
    std::string     name;
    bool            attached;
};
ShmRingState g_shm_ring = {};

double now_ms() {
    auto t = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t - g_log_t0).count();
}

char level_letter(int lv) {
    switch (lv) {
        case LOG_LV_TRACE: return 'V';
        case LOG_LV_DEBUG: return 'D';
        case LOG_LV_INFO:  return 'I';
        case LOG_LV_WARN:  return 'W';
        case LOG_LV_ERROR: return 'E';
        case LOG_LV_FATAL: return 'F';
        default:           return '?';
    }
}

/* ANSI color escape per level. Only emitted when stderr is a TTY. */
const char *level_ansi(int lv) {
    switch (lv) {
        case LOG_LV_FATAL: return "\x1b[1;35m";   /* bold magenta */
        case LOG_LV_ERROR: return "\x1b[31m";     /* red */
        case LOG_LV_WARN:  return "\x1b[33m";     /* yellow */
        case LOG_LV_DEBUG: return "\x1b[36m";     /* cyan */
        case LOG_LV_TRACE: return "\x1b[90m";     /* bright black/gray */
        case LOG_LV_INFO:
        default:           return "";             /* no color for info */
    }
}

bool stderr_is_tty() {
    if (g_stderr_is_tty < 0) {
        g_stderr_is_tty = LOG_ISATTY_FN(LOG_FILENO_FN(stderr)) ? 1 : 0;
    }
    return g_stderr_is_tty != 0;
}

/* Walk the tag table; later-set tags override earlier ones for files where
 * multiple substrings match.  A tag matches if either:
 *   - the file's registered module name equals the tag exactly, OR
 *   - the tag is a substring of the filename
 * Returns the effective minimum level for this file/module.
 *
 * Uses g_global_level_requested (the value the user/env asked for) as the
 * baseline, NOT _log_global_level_cache -- the cache is the MIN across all
 * tags so it lets the macro hot-path skip muted calls. */
int effective_level_for(const char *file, const char *module) {
    int lv = g_global_level_requested;
    if (!file && !module) return lv;
    for (auto &t : g_tag_levels) {
        bool hit = false;
        if (module && !t.tag.empty() && std::strcmp(module, t.tag.c_str()) == 0) {
            hit = true;
        } else if (file && std::strstr(file, t.tag.c_str()) != nullptr) {
            hit = true;
        }
        if (hit) lv = t.lv;
    }
    return lv;
}

const char *short_filename(const char *path) {
    if (!path) return "?";
    const char *s = strrchr(path, '/');
    if (s) return s + 1;
    s = strrchr(path, '\\');
    if (s) return s + 1;
    return path;
}

int parse_level_word(const std::string &s) {
    if (s == "trace" || s == "v") return LOG_LV_TRACE;
    if (s == "debug" || s == "d") return LOG_LV_DEBUG;
    if (s == "info"  || s == "i") return LOG_LV_INFO;
    if (s == "warn"  || s == "w") return LOG_LV_WARN;
    if (s == "error" || s == "e") return LOG_LV_ERROR;
    if (s == "fatal" || s == "f") return LOG_LV_FATAL;
    if (s == "off"   || s == "n") return LOG_LV_OFF;
    return -1;
}

void ensure_env_parsed_locked() {
    if (g_env_parsed) return;
    g_env_parsed = true;
    const char *spec = std::getenv("INSP_LOG");
    if (!spec || !*spec) return;
    /* Inline mini-parser; can't call log_parse_spec because we hold the
     * mutex it would also try to take. */
    std::string s(spec);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t colon = tok.find(':');
        if (colon == std::string::npos) {
            int lv = parse_level_word(tok);
            if (lv >= 0) g_global_level_requested = lv;
        } else {
            std::string tag = tok.substr(0, colon);
            int lv = parse_level_word(tok.substr(colon + 1));
            if (lv >= 0 && !tag.empty()) {
                g_tag_levels.push_back(TagLevel{tag, lv});
            }
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    recompute_cache_locked();
}

} /* anonymous namespace */

extern "C" {

void log_set_global_level(int lv) {
    if (lv < LOG_LV_TRACE) lv = LOG_LV_TRACE;
    if (lv > LOG_LV_OFF)   lv = LOG_LV_OFF;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_global_level_requested = lv;
    recompute_cache_locked();
}

int log_get_global_level(void) {
    return _log_global_level_cache;
}

void log_set_tag_level(const char *tag, int lv) {
    if (!tag || !*tag) return;
    if (lv < LOG_LV_TRACE) lv = LOG_LV_TRACE;
    if (lv > LOG_LV_OFF)   lv = LOG_LV_OFF;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_tag_levels.push_back(TagLevel{std::string(tag), lv});
    recompute_cache_locked();
}

void log_clear_tag_levels(void) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_tag_levels.clear();
    recompute_cache_locked();
}

void log_register_tu_module(const char *file, const char *module_name) {
    if (!file || !module_name) return;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    /* Last registration wins (idempotent for re-init). */
    file_to_module()[std::string(file)] = std::string(module_name);
}

const char *log_module_for_file(const char *file) {
    if (!file) return nullptr;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    auto it = file_to_module().find(std::string(file));
    if (it == file_to_module().end()) return nullptr;
    return it->second.c_str();
}

void log_parse_spec(const char *spec) {
    if (!spec || !*spec) return;
    /* Re-use the locked parser via the env-init path. Equivalent semantics. */
    std::string s(spec);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t colon = tok.find(':');
        if (colon == std::string::npos) {
            int lv = parse_level_word(tok);
            if (lv >= 0) log_set_global_level(lv);
        } else {
            std::string tag = tok.substr(0, colon);
            int lv = parse_level_word(tok.substr(colon + 1));
            if (lv >= 0) log_set_tag_level(tag.c_str(), lv);
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

int log_register_sink(log_sink_fn fn, void *ctx) {
    if (!fn) return 0;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    int id = g_next_sink_id++;
    g_sinks.push_back(SinkEntry{id, fn, ctx});
    return id;
}

void log_unregister_sink(int sink_id) {
    if (sink_id <= 0) return;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    for (auto it = g_sinks.begin(); it != g_sinks.end(); ++it) {
        if (it->id == sink_id) { g_sinks.erase(it); return; }
    }
}

void log_set_stderr_enabled(int enabled) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_stderr_enabled = (enabled != 0);
}

/* ---------- SHM ring buffer sink (Phase A2) ---------- */

/* Sink callback.  Already runs under g_log_mutex (registered via the normal
 * sink interface), so no additional locking is needed against other sinks.
 *
 * Cross-process consistency:  we use the per-slot seq protocol (odd =
 * writing, even = stable) so the Phase F drainer can detect torn reads even
 * though the drainer lives in a separate address space. */
static void shm_ring_sink(int lv, const char *file, int line,
                          const char *line_text, void *ctx) {
    auto *st = static_cast<ShmRingState *>(ctx);
    if (!st || !st->attached || !st->hdr) return;

    LogRingHeader *h = st->hdr;
    /* fetch_add returns the previous value; that's the slot we write. */
    uint64_t idx = h->head.fetch_add(1, std::memory_order_acq_rel);
    LogSlot *slot = log_ring_slot(h, idx);

    /* Mark slot as in-write (odd seq). Use a fresh-and-increasing seq based
     * on idx*2; that way wraparound reuse gives the drainer a strictly
     * monotonic seq to compare against. */
    uint64_t target_seq = (idx + 1) * 2;
    slot->seq.store(target_seq | 1, std::memory_order_release);

    slot->level = lv;
    slot->line  = line;

    /* Copy the formatted line text, truncating to LOG_SLOT_TEXT-1 to leave
     * room for NUL.  We don't need to copy file -- it's in line_text. */
    (void)file;
    size_t n = std::strlen(line_text);
    if (n > LOG_SLOT_TEXT - 1) n = LOG_SLOT_TEXT - 1;
    std::memcpy(slot->text, line_text, n);
    slot->text[n] = '\0';

    /* Commit (even seq).  Drainer reads acquire here. */
    slot->seq.store(target_seq, std::memory_order_release);

    /* Heartbeat (steady-clock ms; drainer compares to its own clock with a
     * generous tolerance, so absolute alignment isn't required). */
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    h->heartbeat_ms.store(static_cast<uint64_t>(ms),
                          std::memory_order_relaxed);
}

int log_open_shm_ring(const char *shm_name, int size_mb) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    if (g_shm_ring.attached) return g_shm_ring.sink_id;

    /* Name resolution: arg > env > default. */
    std::string name;
    if (shm_name && *shm_name) {
        name = shm_name;
    } else if (const char *e = std::getenv("INSP_LOG_RING_NAME")) {
        name = e;
    } else {
        name = "insp_log_ring";
    }

    /* Size: arg > env > default 16 MB. */
    int mb = size_mb;
    if (mb <= 0) {
        if (const char *e = std::getenv("INSP_LOG_RING_MB")) {
            mb = std::atoi(e);
        }
    }
    if (mb <= 0) mb = 16;
    if (mb > 1024) mb = 1024;  /* sanity cap */

    /* Slot count derived from mb. */
    size_t total = static_cast<size_t>(mb) * 1024 * 1024;
    uint32_t slot_count = static_cast<uint32_t>(
        (total - LOG_HEADER_BYTES) / LOG_SLOT_BYTES);
    if (slot_count < 16) return 0;  /* refuse silly small */

    size_t need = log_ring_total_bytes(slot_count);

    /* Try connect first (drainer may have created it).  If not, create. */
    ShareMemoryInfo info = {};
    bool owns_unlink = false;
    int rc = connSharedMemory(name, need, &info);
    if (rc != 0) {
        rc = createSharedMemory(name, need, &info);
        if (rc != 0) {
            std::fprintf(stderr,
                "[logctrl] shm open '%s' failed (create rc=%d)\n",
                name.c_str(), rc);
            return 0;
        }
        owns_unlink = true;
    }
    if (!info.ptr) {
        std::fprintf(stderr,
            "[logctrl] shm '%s' mapped null\n", name.c_str());
        return 0;
    }

    LogRingHeader *h = static_cast<LogRingHeader *>(info.ptr);

    /* If the region is fresh (or stale from a previous run with a different
     * size), initialize the header.  If the magic + size match, reuse the
     * existing head index so we don't lose drainer-side context. */
    if (h->magic != LOG_RING_MAGIC ||
        h->version != LOG_RING_VERSION ||
        h->slot_size != LOG_SLOT_BYTES ||
        h->slot_count != slot_count) {
        std::memset(h, 0, LOG_HEADER_BYTES);
        h->magic      = LOG_RING_MAGIC;
        h->version    = LOG_RING_VERSION;
        h->slot_size  = LOG_SLOT_BYTES;
        h->slot_count = slot_count;
        new (&h->head)          std::atomic<uint64_t>(0);
        new (&h->heartbeat_ms)  std::atomic<uint64_t>(0);
        new (&h->crash_marker)  std::atomic<uint32_t>(LOG_CRASH_NONE);

        /* Zero all slot seqs so the drainer doesn't trust stale data. */
        auto *slots = reinterpret_cast<uint8_t *>(info.ptr) + LOG_HEADER_BYTES;
        std::memset(slots,
                    0,
                    static_cast<size_t>(slot_count) * LOG_SLOT_BYTES);
    }

    g_shm_ring.info         = info;
    g_shm_ring.hdr          = h;
    g_shm_ring.owns_unlink  = owns_unlink;
    g_shm_ring.name         = name;
    g_shm_ring.attached     = true;

    /* Register the sink while we still hold the mutex -- log_register_sink
     * takes the same mutex.  Inline the registration instead. */
    int id = g_next_sink_id++;
    g_sinks.push_back(SinkEntry{id, shm_ring_sink, &g_shm_ring});
    g_shm_ring.sink_id = id;

    std::fprintf(stderr,
        "[logctrl] shm ring '%s' attached: %u slots x %u bytes = %.1f MB%s\n",
        name.c_str(), slot_count, LOG_SLOT_BYTES,
        (double)need / (1024.0 * 1024.0),
        owns_unlink ? " (created)" : " (connected)");

    return id;
}

void *log_get_shm_ring_mapping(void) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    return g_shm_ring.attached ? static_cast<void *>(g_shm_ring.hdr) : nullptr;
}

void log_close_shm_ring(void) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    if (!g_shm_ring.attached) return;

    /* Unregister the sink first so future emits don't try to write into the
     * region we're about to unmap. */
    for (auto it = g_sinks.begin(); it != g_sinks.end(); ++it) {
        if (it->id == g_shm_ring.sink_id) { g_sinks.erase(it); break; }
    }

    if (g_shm_ring.owns_unlink) {
        deleteSharedMemory(g_shm_ring.info);
    }
    g_shm_ring = ShmRingState{};
}

void log_emit(int lv, const char *file, int line, const char *func,
              const char *fmt, ...) {
    /* Format the user message into a stack buffer. */
    char user_buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int user_len = vsnprintf(user_buf, sizeof(user_buf), fmt, ap);
    va_end(ap);
    if (user_len < 0) user_len = 0;
    if (user_len >= (int)sizeof(user_buf)) user_len = (int)sizeof(user_buf) - 1;

    std::lock_guard<std::mutex> lk(g_log_mutex);

    /* Lazy env parse on first emit, while we hold the mutex. */
    ensure_env_parsed_locked();

    /* Look up module for this TU.  Lookup is cheap (hash by file ptr). */
    const char *module = nullptr;
    if (file) {
        auto it = file_to_module().find(std::string(file));
        if (it != file_to_module().end()) module = it->second.c_str();
    }

    /* Per-tag check (may override the macro's global-cache check; either way
     * the user wanted level X for this tag and we honor it here). */
    if (!g_tag_levels.empty()) {
        int eff = effective_level_for(file, module);
        if (lv < eff) return;
    }

    /* Build the prefixed line.  Includes module column iff the TU has one
     * registered via LOG_MODULE; otherwise omit to save width. */
    char line_buf[1280];
    const char *fname = short_filename(file);
    int n;
    if (module) {
        n = snprintf(line_buf, sizeof(line_buf),
                     "[%10.3f][%c][%-14.14s][%-20.20s:%-4d %s] %s\n",
                     now_ms(), level_letter(lv), module,
                     fname, line, func ? func : "?", user_buf);
    } else {
        n = snprintf(line_buf, sizeof(line_buf),
                     "[%10.3f][%c][%-20.20s:%-4d %s] %s\n",
                     now_ms(), level_letter(lv),
                     fname, line, func ? func : "?", user_buf);
    }
    if (n < 0) n = 0;
    if (n >= (int)sizeof(line_buf)) {
        n = (int)sizeof(line_buf) - 1;
        line_buf[n - 1] = '\n';
    }

    /* Default stderr sink. ANSI only when TTY; auto-stripped on pipes/files. */
    if (g_stderr_enabled) {
        if (stderr_is_tty()) {
            const char *ansi = level_ansi(lv);
            if (ansi[0]) {
                fputs(ansi, stderr);
                fwrite(line_buf, 1, (size_t)n, stderr);
                fputs("\x1b[0m", stderr);
            } else {
                fwrite(line_buf, 1, (size_t)n, stderr);
            }
        } else {
            fwrite(line_buf, 1, (size_t)n, stderr);
        }
    }

    /* User-registered sinks (shm ring producer, WebUI callback, etc). */
    for (auto &sk : g_sinks) {
        sk.fn(lv, file, line, line_buf, sk.ctx);
    }
}

/* -------------------------------------------------------------------------- */
/* Legacy direct-call functions: bypass the level filter, write straight to   */
/* stderr.  Used by a handful of third-party adapters.                         */
/* -------------------------------------------------------------------------- */

void logv(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
void logd(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
void logi(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
void loge(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

/* -------------------------------------------------------------------------- */
/* Legacy utility helpers (pre-existing thread-unsafe API; kept for compat).  */
/* These return pointers into static buffers -- not thread-safe. Existing     */
/* code already lives with that, so no change in semantics.                   */
/* -------------------------------------------------------------------------- */

char *byteArrString(uint8_t *data, int length, int spaceInterval) {
    const int buffL = 500;
    static char buff[buffL + 30];
    if (spaceInterval <= 0) spaceInterval = 4;
    int maxArrL = buffL / (spaceInterval * 2 + 1) * spaceInterval;
    int pLength = length;
    if (pLength > maxArrL) pLength = maxArrL;
    char *strptr = buff;
    int scount = 0;
    for (int i = 0; i < pLength; i++) {
        strptr += sprintf(strptr, "%02X", data[i]);
        if (scount++ == spaceInterval) {
            strptr += sprintf(strptr, " ");
            scount = 0;
        }
    }
    if (pLength != length) {
        strptr += sprintf(strptr, "...(%d more)", length - pLength);
    }
    return buff;
}

char *_SubString(const char *str, int Count) {
    static char buff[100];
    if (str == NULL) return NULL;
    if (Count > (int)(sizeof(buff) - 1)) Count = (int)(sizeof(buff) - 1);
    int i = 0;
    for (i = 0; i < Count; i++) {
        if (str[i] == '\0') break;
        buff[i] = str[i];
    }
    buff[i] = '\0';
    return buff;
}

char *_SubString_Align(const char *str, int Count) {
    static char buff[100];
    if (str == NULL) return NULL;
    if (Count > (int)(sizeof(buff) - 1)) Count = (int)(sizeof(buff) - 1);
    int i = 0;
    int isend = 0;
    for (i = 0; i < Count; i++) {
        if (!isend && str[i] == '\0') isend = 1;
        buff[i] = (char)(isend ? ' ' : str[i]);
    }
    buff[i] = '\0';
    return buff;
}

} /* extern "C" */

/* C++-only overload of byteArrString. */
char *byteArrString(uint8_t *data, int length) {
    return byteArrString(data, length, 4);
}
