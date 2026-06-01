/* test_ring.cpp -- smoke test for the Phase A2 shm ring buffer.
 *
 * Opens a fresh ring, emits a handful of log lines, then opens the same ring
 * as a reader and verifies the slot layout / seq tear-detection / text
 * round-trips correctly.
 *
 * Build (added via CMakeLists as logctrl_test_ring):
 *   ./logctrl_test_ring
 */

#include <logctrl.h>
#include <log_ring.h>
#include <sp.hpp>

#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
#define EXPECT(cond, msg) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", msg); g_fail++; } \
} while (0)

int main() {
    std::fprintf(stderr, "=== Phase A2 shm ring smoke test ===\n");

    /* Use a unique name so we don't collide with anyone else's run. */
    std::string name = "insp_log_ring_test_phaseA2";

    /* Make sure no stale region from a previous failed run. */
    ShareMemoryInfo stale = {};
    stale.name = name;
    (void)deleteSharedMemory(stale);   /* shm_unlink, harmless on Win */

    /* Open ring at 1 MB (small for the test). */
    int sink_id = log_open_shm_ring(name.c_str(), /*size_mb=*/1);
    EXPECT(sink_id > 0, "log_open_shm_ring returned a sink id");
    if (sink_id <= 0) return 1;

    /* Emit a few lines.  Default level is INFO, so LOGE and LOGW are above
     * threshold; LOGD is suppressed. */
    LOGE("first error msg %d", 42);
    LOGW("a warning %s", "warn-text");
    LOGI("info one");
    LOGI("info two");

    /* Inspect via the producer's own mapping (cross-process I/O is verified
     * later in the Phase F drainer integration test).  This avoids the
     * subtle size mismatch between createSharedMemory's ftruncate and a
     * re-connSharedMemory call in the same process. */
    LogRingHeader *h =
        static_cast<LogRingHeader *>(log_get_shm_ring_mapping());
    EXPECT(h != nullptr, "log_get_shm_ring_mapping returned a pointer");
    if (!h) { log_close_shm_ring(); return 1; }
    EXPECT(h->magic == LOG_RING_MAGIC, "header magic ok");
    EXPECT(h->version == LOG_RING_VERSION, "header version ok");
    EXPECT(h->slot_size == LOG_SLOT_BYTES, "header slot size ok");

    uint64_t head = h->head.load();
    EXPECT(head >= 4, "head index advanced for >=4 writes");
    std::fprintf(stderr, "  head=%llu, slot_count=%u\n",
                 (unsigned long long)head, h->slot_count);

    /* Read back the slots we wrote (in [head-4, head)). */
    int found_err = 0, found_warn = 0, found_info = 0;
    for (uint64_t i = head - 4; i < head; ++i) {
        LogSlot *s = log_ring_slot(h, i);
        uint64_t seq_before = s->seq.load();
        EXPECT((seq_before & 1) == 0, "slot seq is even (committed)");
        char text[LOG_SLOT_TEXT];
        std::memcpy(text, s->text, LOG_SLOT_TEXT);
        int lv = s->level;
        uint64_t seq_after = s->seq.load();
        EXPECT(seq_before == seq_after, "slot didn't tear during read");

        std::fprintf(stderr, "  slot[%llu] lv=%d text=%.80s\n",
                     (unsigned long long)i, lv, text);

        if (std::strstr(text, "first error msg 42")) found_err++;
        if (std::strstr(text, "warn-text"))           found_warn++;
        if (std::strstr(text, "info one"))            found_info++;
    }
    EXPECT(found_err == 1, "found the LOGE round-trip");
    EXPECT(found_warn == 1, "found the LOGW round-trip");
    EXPECT(found_info >= 1, "found at least one LOGI round-trip");

    /* Done.  Close and unlink. */
    log_close_shm_ring();

    if (g_fail == 0) {
        std::fprintf(stderr, "=== ALL PASS ===\n");
        return 0;
    } else {
        std::fprintf(stderr, "=== %d FAIL ===\n", g_fail);
        return 1;
    }
}
