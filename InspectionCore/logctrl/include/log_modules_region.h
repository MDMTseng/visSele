/* log_modules_region.h -- producer-published snapshot of registered
 * LOG_MODULE names + their currently-effective levels.
 *
 * Lives in its own shm object (so it can grow independently of the log
 * ring) named "<ring>_modules" by convention.  Producer writes via the
 * existing g_log_mutex; drainer reads under the seq-tear protocol.
 *
 * Used to power the WebUI's `getModules` response so the log panel can
 * populate its module tree with real names + slider positions.
 */

#ifndef LOG_MODULES_REGION_H
#define LOG_MODULES_REGION_H

#include <stdint.h>
#include <atomic>

static constexpr uint32_t LOG_MODULES_MAGIC   = 0x4D4F444Cu;  /* 'LDOM' */
static constexpr uint32_t LOG_MODULES_VERSION = 1;

static constexpr uint32_t LOG_MODULES_NAME_BYTES = 24;
static constexpr uint32_t LOG_MODULES_MAX        = 64;

struct LogModuleEntry {
    char    name[LOG_MODULES_NAME_BYTES];   /* NUL-terminated */
    int32_t level;                           /* LOG_LV_* effective for module */
};

struct alignas(64) LogModulesRegion {
    uint32_t magic;
    uint32_t version;

    /* Seq protocol: producer stores (seq | 1) before edit, (seq + 2)
     * when stable.  Drainer retries on odd-after or seq-change. */
    std::atomic<uint64_t> seq;

    uint32_t count;
    uint32_t reserved0;

    LogModuleEntry modules[LOG_MODULES_MAX];
};

static constexpr size_t LOG_MODULES_REGION_BYTES = sizeof(LogModulesRegion);

#endif /* LOG_MODULES_REGION_H */
