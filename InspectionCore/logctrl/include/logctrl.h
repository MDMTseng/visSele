/* logctrl.h  --  unified logging for InspectionCore.
 *
 * Backward-compatible: same LOG[VDIE] macro names + same logv/logd/logi/loge
 * direct-call functions as the legacy header. New capabilities layered on top:
 *
 *   - Levels: TRACE / DEBUG / INFO / WARN / ERROR / FATAL.
 *   - Cheap macro-level filter (atomic load + compare + branch); muted calls
 *     skip the format work entirely.
 *   - Per-tag (substring of __FILENAME__) overrides via INSP_LOG env var, e.g.
 *       INSP_LOG=warn,sig360:debug,Aravis:info,bpg:off
 *   - Thread-safe emit (coarse mutex; no interleaved lines under OpenMP).
 *   - Pluggable sinks (callback fn + ctx); stderr default-on, auto-strip ANSI
 *     when stderr is not a TTY.
 *   - Single-source cross-platform (std::mutex / std::chrono / atomics in the
 *     impl; only TTY-detect is per-OS).
 *
 * Output format (current):
 *   [    12.487][I][sig360_circle_line:4711 FeatureMatching] msg...
 *   ^timestamp    ^level
 *
 * Caller migration: no source changes required.  Macros gain the level filter
 * automatically.  New code may also use LOGW (warn) and LOGF (fatal).
 *
 * Phase A: this header + logctrl.cpp.  Phase A2 adds a shared-memory ring
 * sink (built on contrib/smem_channel/).  Phase F adds the inspd_log drainer
 * daemon that reads the ring and routes to disk + crash dump.
 */

#ifndef LOGCTRL_HEADER
#define LOGCTRL_HEADER

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 *  Levels
 * -------------------------------------------------------------------------- */

enum {
    LOG_LV_TRACE = 0,   /* every-iteration trivia */
    LOG_LV_DEBUG = 1,   /* dev-time diagnostics */
    LOG_LV_INFO  = 2,   /* production state changes */
    LOG_LV_WARN  = 3,   /* recoverable anomaly */
    LOG_LV_ERROR = 4,   /* op failed */
    LOG_LV_FATAL = 5,   /* process must die */
    LOG_LV_OFF   = 6    /* sentinel: no output */
};

/* --------------------------------------------------------------------------
 *  Runtime configuration (thread-safe)
 * -------------------------------------------------------------------------- */

/* Set/get the global minimum level.  Default LOG_LV_INFO. */
void log_set_global_level(int lv);
int  log_get_global_level(void);

/* Per-tag override.  Tag is matched as a substring against __FILENAME__.
 * Multiple tags may match a single emit; later overrides win.
 * Pass NULL/empty tag to no-op. */
void log_set_tag_level(const char *tag, int lv);
void log_clear_tag_levels(void);

/* Parse INSP_LOG-style spec: "global_level,tag:lv,tag:lv,...".
 * Global token is the bare level word (e.g. "warn") or omitted.
 * Examples:
 *    INSP_LOG=warn
 *    INSP_LOG=warn,sig360:debug,Aravis:info,bpg:off
 *    INSP_LOG=trace                            // global trace (very noisy)
 * Call from main() with getenv("INSP_LOG") at startup. */
void log_parse_spec(const char *spec);

/* --------------------------------------------------------------------------
 *  Sinks
 * -------------------------------------------------------------------------- */

/* Sink callback: receives a fully-formatted line including trailing '\n'.
 * line_text is one logical message, valid only during the call.
 * Sinks are called under the logger's internal mutex; do not log from inside. */
typedef void (*log_sink_fn)(int lv, const char *file, int line,
                            const char *line_text, void *ctx);

/* Register/unregister.  Returns a non-zero sink id, or 0 on failure. */
int  log_register_sink(log_sink_fn fn, void *ctx);
void log_unregister_sink(int sink_id);

/* Toggle the default stderr sink.  Enabled by default. */
void log_set_stderr_enabled(int enabled);

/* --------------------------------------------------------------------------
 *  Internal: emit (called by the macros after the cheap level check)
 * -------------------------------------------------------------------------- */

void log_emit(int lv, const char *file, int line, const char *func,
              const char *fmt, ...);

/* The atomic cached global level the macros compare against.
 * Treat as read-only from C; mutate via log_set_global_level() only.
 * volatile-int is good enough on every platform we care about (the value is
 * only ever set under the logger mutex; readers do plain aligned int loads). */
extern volatile int _log_global_level_cache;

/* --------------------------------------------------------------------------
 *  Macros
 * -------------------------------------------------------------------------- */

#ifndef __FILENAME__
/* Last path component of __FILE__.  Compile-time-ish (depends on linker
 * deduping const strings).  We accept the runtime strrchr cost. */
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
     (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))
#endif

/* Legacy: prepend a comma to non-empty __VA_ARGS__.  Used by some external
 * macros (e.g., MT_LOCK in wiringPanel.cpp).  Kept for back-compat. */
#ifndef VA_ARGS
#define VA_ARGS(...) , ##__VA_ARGS__
#endif

/* Hot path when muted: 1 read of _log_global_level_cache + 1 compare + branch.
 * NO format work, NO function call. */
#define LOG_IF_(lv, fmt, ...) do { \
    if ((int)(lv) >= _log_global_level_cache) { \
        log_emit((int)(lv), __FILENAME__, __LINE__, __func__, \
                 fmt, ##__VA_ARGS__); \
    } \
} while (0)

/* LOGI / LOGW / LOGE / LOGF: enabled through the level filter. */
#define LOGI(fmt, ...) LOG_IF_(LOG_LV_INFO,  fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_IF_(LOG_LV_WARN,  fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG_IF_(LOG_LV_ERROR, fmt, ##__VA_ARGS__)
#define LOGF(fmt, ...) LOG_IF_(LOG_LV_FATAL, fmt, ##__VA_ARGS__)

/* LOGV / LOGD: kept as compile-time no-ops in Phase A.
 *
 * Background: the legacy header had these as commented-out empty macros, so
 * ~80 LOGV/LOGD call-sites in the codebase silently grew expressions that
 * reference non-existent struct members (e.g. cir.circleTar.* in
 * FeatureManager_sig360_circle_line) -- the arguments were never compiled.
 * Turning LOGV into a real call would expose these latent bugs and break the
 * build.
 *
 * Phase D audits + repairs each LOGV/LOGD site, then re-enables the macros
 * via LOG_IF_.  Until then, set INSP_LOG_ENABLE_TRACE_DEBUG to opt in for
 * one TU at a time during the audit. */
#ifdef INSP_LOG_ENABLE_TRACE_DEBUG
#define LOGV(fmt, ...) LOG_IF_(LOG_LV_TRACE, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) LOG_IF_(LOG_LV_DEBUG, fmt, ##__VA_ARGS__)
#else
/* Argument list is discarded by the preprocessor; latent bugs stay dormant. */
#define LOGV(fmt, ...) ((void)0)
#define LOGD(fmt, ...) ((void)0)
#endif

/* LOGS: format a message into a caller-supplied buffer instead of emitting
 * it.  Pre-existing API used by a couple of stage_light callers that need
 * the formatted string for later inclusion in JSON.  Kept for back-compat;
 * does NOT go through the level filter or the sinks. */
#define LOGS(buf, fmt, ...) \
    sprintf((buf), "%s:%d %s:$ " fmt "\n", \
            __FILENAME__, __LINE__, __func__, ##__VA_ARGS__)

/* --------------------------------------------------------------------------
 *  Legacy direct-call functions
 *
 *  Kept for any caller that bypasses the macros (some third-party adapter
 *  code does this).  These BYPASS the level filter and write straight to
 *  the same stderr the default sink uses.  Format-string-is-user-supplied
 *  semantics preserved.
 * -------------------------------------------------------------------------- */

void logv(const char *fmt, ...);
void logd(const char *fmt, ...);
void logi(const char *fmt, ...);
void loge(const char *fmt, ...);

/* --------------------------------------------------------------------------
 *  Legacy utility helpers (pre-existing thread-unsafe API; kept for compat)
 * -------------------------------------------------------------------------- */

char *byteArrString(uint8_t *data, int length, int spaceInterval);
char *_SubString(const char *str, int Count);
char *_SubString_Align(const char *str, int Count);

#ifdef __cplusplus
} /* extern "C" */

/* C++-only overload (single-arg byteArrString defaulting spaceInterval to 4) */
char *byteArrString(uint8_t *data, int length);
#endif

#endif /* LOGCTRL_HEADER */
