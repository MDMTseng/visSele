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

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
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
 * multiple substrings match. Returns the effective minimum level. */
int effective_level_for(const char *file) {
    int lv = _log_global_level_cache;
    if (!file) return lv;
    for (auto &t : g_tag_levels) {
        if (strstr(file, t.tag.c_str()) != nullptr) lv = t.lv;
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
            if (lv >= 0) _log_global_level_cache = lv;
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
}

} /* anonymous namespace */

extern "C" {

void log_set_global_level(int lv) {
    if (lv < LOG_LV_TRACE) lv = LOG_LV_TRACE;
    if (lv > LOG_LV_OFF)   lv = LOG_LV_OFF;
    std::lock_guard<std::mutex> lk(g_log_mutex);
    _log_global_level_cache = lv;
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
}

void log_clear_tag_levels(void) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    g_tag_levels.clear();
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

    /* Build the prefixed line. Keep newline at end. */
    char line_buf[1280];
    const char *fname = short_filename(file);
    int n = snprintf(line_buf, sizeof(line_buf),
                     "[%10.3f][%c][%-20.20s:%-4d %s] %s\n",
                     now_ms(), level_letter(lv),
                     fname, line, func ? func : "?", user_buf);
    if (n < 0) n = 0;
    if (n >= (int)sizeof(line_buf)) {
        n = (int)sizeof(line_buf) - 1;
        line_buf[n - 1] = '\n';
    }

    std::lock_guard<std::mutex> lk(g_log_mutex);

    /* Lazy env parse on first emit, while we hold the mutex. */
    ensure_env_parsed_locked();

    /* Per-tag check (may override the macro's global-cache check; either way
     * the user wanted level X for this tag and we honor it here). */
    if (!g_tag_levels.empty()) {
        int eff = effective_level_for(file);
        if (lv < eff) return;
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
