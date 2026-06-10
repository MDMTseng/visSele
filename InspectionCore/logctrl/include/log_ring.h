/* log_ring.h -- shared-memory ring buffer for the InspectionCore logger.
 *
 * Layout shared between the main process (producer) and the inspd_log
 * drainer (consumer, Phase F).  Both processes mmap the same region.
 *
 * Memory map:
 *
 *   offset 0       : LogRingHeader    (128 bytes, padded)
 *   offset 128     : LogSlot[slot_count]
 *
 *   slot_size = 256 bytes  (fixed; producer truncates longer lines)
 *   default ring = 16 MB   ->  65535 slots
 *
 * The header carries the atomic head index and a producer heartbeat for the
 * Phase F drainer to use as a parent-death signal.
 *
 * Slot tear detection (single producer, one consumer in another process):
 *   producer:
 *     slot.seq.store(s | 1, release);   // odd = mid-write
 *     ... write level + line + text ...
 *     slot.seq.store(s + 2, release);   // even = committed
 *   consumer:
 *     uint64_t before = slot.seq.load(acquire);
 *     if (before & 1) skip;             // mid-write
 *     ... read text / level / line ...
 *     uint64_t after  = slot.seq.load(acquire);
 *     if (after != before) retry;       // overwritten during read
 *
 * This is platform-independent C++ (atomics, alignas).  smem_channel.hpp
 * already abstracts shm_open vs CreateFileMapping per-platform.
 */

#ifndef LOG_RING_H
#define LOG_RING_H

#include <stdint.h>
#include <atomic>

/* ---------- protocol constants ---------- */

static constexpr uint32_t LOG_RING_MAGIC   = 0x474E524C;  /* 'LRNG' little-endian */
static constexpr uint32_t LOG_RING_VERSION = 2;            /* bump: added crash trace */

static constexpr uint32_t LOG_SLOT_BYTES   = 256;
static constexpr uint32_t LOG_HEADER_BYTES = 512;          /* expanded to fit crash trace */
static constexpr uint32_t LOG_SLOT_TEXT    = 240;  /* per-slot text bytes incl. NUL */

/* Phase G: max stack frames captured by the signal handler.  256 bytes = 32
 * 8-byte addresses; deep stacks get truncated. */
static constexpr uint32_t LOG_CRASH_FRAME_MAX = 32;

/* Crash marker codes written into LogRingHeader.crash_marker by the Phase G
 * signal handlers.  Drainer treats any non-zero marker as "main process
 * crashed -- dump and exit". */
static constexpr uint32_t LOG_CRASH_NONE   = 0;
static constexpr uint32_t LOG_CRASH_SIGSEGV = 1;
static constexpr uint32_t LOG_CRASH_SIGABRT = 2;
static constexpr uint32_t LOG_CRASH_SIGFPE  = 3;
static constexpr uint32_t LOG_CRASH_SIGBUS  = 4;
static constexpr uint32_t LOG_CRASH_OTHER   = 99;
/* NOT a crash: the producer sets this marker to ask the drainer for an
 * on-demand dump of the current ring (a "flight recorder" snapshot). The
 * drainer writes the dump but, unlike a real crash, RESETS the marker and
 * keeps running. Distinct value so it never collides with a real crash code. */
static constexpr uint32_t LOG_CRASH_DUMP_REQUEST = 100;

/* ---------- layout ---------- */

/* Header at offset 0 of the mapping.  std::atomic<uint64_t> on shared mem:
 * works as long as both processes were compiled with the same ABI.  Both
 * platforms (clang on macOS arm64 and MSVC/MinGW on Windows) place
 * std::atomic<uint64_t> at 8-byte alignment with the lock-free integer
 * layout, so this is safe in practice. */
struct alignas(64) LogRingHeader {
    uint32_t magic;          /* must equal LOG_RING_MAGIC */
    uint32_t version;        /* must equal LOG_RING_VERSION */
    uint32_t slot_size;      /* always LOG_SLOT_BYTES */
    uint32_t slot_count;     /* number of slots in the ring */

    /* Next slot index to write (producer increments).  Reads/writes use
     * acq_rel; the slot's own seq does the tear detection. */
    std::atomic<uint64_t> head;

    /* Monotonic-ms timestamp updated by the producer's emit path; drainer
     * watches it as a parent-death heartbeat. */
    std::atomic<uint64_t> heartbeat_ms;

    /* Phase G crash region.  Signal handlers in main write here using only
     * async-signal-safe primitives (atomic stores + memcpy to mapped memory).
     *
     * crash_marker     - one of LOG_CRASH_* above; 0 means no crash.
     * crash_signal     - the signal number (or platform exception code).
     * crash_frame_count- number of valid entries in crash_frames[].
     * crash_frames[]   - raw return addresses captured by backtrace() /
     *                    RtlCaptureStackBackTrace.  Drainer resolves symbols
     *                    at dump time (atos / addr2line / DbgHelp).
     */
    std::atomic<uint32_t> crash_marker;
    uint32_t              crash_signal;
    std::atomic<uint32_t> crash_frame_count;
    uint32_t              crash_reserved0;
    uint64_t              crash_frames[LOG_CRASH_FRAME_MAX];

    /* Phase F.2/#29: drainer -> producer control IPC.
     *
     * Drainer writes one command at a time:
     *   - bump ctrl_cmd_seq (release) after filling slot
     *   - ctrl_cmd_module = "" means "set global level"
     *   - ctrl_cmd_module = "*" means "clear all module overrides"
     *   - ctrl_cmd_level  = INT32_MIN means "no change" (used at startup)
     *
     * Producer reads on each LOG_emit:
     *   - if ctrl_cmd_seq != last_seen, apply, update last_seen.
     *
     * Single-slot is enough because the drainer serializes WS commands.
     * Multiple rapid commands are coalesced by the drainer (it only sends
     * the latest before bumping seq).
     */
    std::atomic<uint32_t> ctrl_cmd_seq;
    int32_t  ctrl_cmd_level;
    char     ctrl_cmd_module[28];   /* NUL-terminated; empty = global */

    /* Producer's wall-clock start time (CLOCK_REALTIME ns since epoch).
     * Written once at log_open_shm_ring(); drainer uses it as the anchor
     * for converting per-line `[%10.3f]` seconds-since-start prefixes
     * into absolute timeUnixNano on the WS stream.  Zero means "unknown"
     * (legacy producer); drainer falls back to its own attach time. */
    uint64_t producer_started_unix_nano;

    /* Pad up to LOG_HEADER_BYTES.  Layout: 328 + 4 + 4 + 28 + 8 = 372. */
    uint8_t pad[LOG_HEADER_BYTES - 372];
};

/* One log entry.  Fixed-size; producer truncates rather than splitting
 * across slots. */
struct alignas(8) LogSlot {
    /* Sequence counter for tear detection (see file-level comment). */
    std::atomic<uint64_t> seq;

    /* Log level (one of LOG_LV_*) so the drainer can route without parsing
     * the formatted text. */
    int32_t level;

    /* Source line number (informational; also embedded in text). */
    int32_t line;

    /* Formatted text including timestamp/level/module prefix and trailing
     * newline.  NUL-terminated within LOG_SLOT_TEXT bytes. */
    char text[LOG_SLOT_TEXT];
};

static_assert(sizeof(LogRingHeader) == LOG_HEADER_BYTES,
              "LogRingHeader must be exactly 128 bytes for binary compat");
static_assert(sizeof(LogSlot) == LOG_SLOT_BYTES,
              "LogSlot must be exactly 256 bytes for binary compat");

/* Slot pointer arithmetic (producer + drainer use the same macro). */
inline LogSlot *log_ring_slot(void *mapping, uint64_t idx) {
    LogRingHeader *h = static_cast<LogRingHeader *>(mapping);
    uint64_t mod = idx % h->slot_count;
    auto *base = reinterpret_cast<uint8_t *>(mapping) + LOG_HEADER_BYTES;
    return reinterpret_cast<LogSlot *>(base + mod * LOG_SLOT_BYTES);
}

/* Total bytes needed in the shm region for slot_count slots. */
inline size_t log_ring_total_bytes(uint32_t slot_count) {
    return LOG_HEADER_BYTES + static_cast<size_t>(slot_count) * LOG_SLOT_BYTES;
}

#endif /* LOG_RING_H */
