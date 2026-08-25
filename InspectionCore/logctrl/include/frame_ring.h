/* frame_ring -- the last N inspection frames, in shared memory, for a core
 * that has already died.
 *
 * WHY THIS EXISTS
 * ---------------
 * The snapshot path writes evidence to disk as it happens. Measured on the
 * bench, that cost 146 KB a part at ~11 parts/s into a folder capped at a
 * handful of files: ~138 GB a day written to retain about a megabyte, because
 * almost every byte was deleted within a second of being written. Snapshots
 * now default to off for exactly that reason -- which leaves the question the
 * disk path was really there to answer:
 *
 *   "the core just died. what was it looking at?"
 *
 * That question does not need a continuous disk write. It needs the last few
 * frames to still exist at the moment of death, and it needs someone who is
 * still alive to write them down. Both already exist: inspd_log is a separate
 * process that outlives the core (heartbeat-based parent-death detection) and
 * already writes crash_<utc>.dump. This ring is the image half of that dump.
 *
 * Steady-state disk writes: zero. Frames land in RAM, are overwritten in
 * place, and only reach a disk when the core crashes or an operator asks.
 *
 * LAYOUT
 * ------
 * A SEPARATE shm region from the log ring, named "<ring>_frames" (the same
 * convention as "<ring>_modules"). Deliberately separate: LogRingHeader is
 * pinned at 128 bytes and LogSlot at 256 by static_assert for binary
 * compatibility, and a 100 KB JPEG does not go in a 256-byte text slot.
 *
 * TEARING
 * -------
 * The producer can die HALFWAY THROUGH a memcpy -- that is the case this ring
 * exists for, so it is the case that must not produce a plausible-looking
 * corrupt JPEG. Same protocol as LogSlot: seq is bumped to odd before the
 * write and to even after it. A reader that sees an odd seq, or a seq that
 * changed across the read, discards that slot. A discarded frame is a frame
 * that was being written when everything stopped; the one before it is intact.
 */
#ifndef FRAME_RING_H
#define FRAME_RING_H

#include <atomic>
#include <cstdint>
#include <cstring>

static constexpr uint32_t FRAME_RING_MAGIC   = 0x474E5246; /* 'FRNG' */
static constexpr uint32_t FRAME_RING_VERSION = 1;

/* 256 KB a slot against a measured 103 KB image + 43 KB report leaves room for
 * a less compressible frame without a second sizing knob. 16 slots is ~4 MB --
 * about 1.5 s of history at 11 parts/s, which is "what did it see just before
 * it stopped" rather than a trend. */
static constexpr uint32_t FRAME_SLOT_BYTES  = 256u * 1024u;
static constexpr uint32_t FRAME_SLOT_HEADER = 64u;
static constexpr uint32_t FRAME_PAYLOAD_MAX = FRAME_SLOT_BYTES - FRAME_SLOT_HEADER;
static constexpr uint32_t FRAME_RING_SLOTS_DEFAULT = 16;

/* Verdict codes, matching the core's SnapVerdict. Stored so a dump can be read
 * without the core's headers. */
enum : uint32_t { FRAME_V_OK = 0, FRAME_V_NG = 1, FRAME_V_NA = 2 };

struct alignas(64) FrameRingHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t slot_size;
    uint32_t slot_count;
    std::atomic<uint64_t> head;   /* next slot index to write */
    uint32_t reserved[10];
};

struct alignas(64) FrameSlot {
    std::atomic<uint64_t> seq;    /* odd = being written; see TEARING above */
    uint64_t ts_ms;               /* producer wall clock at push */
    uint64_t frame_id;            /* monotonic index, for ordering a dump */
    uint32_t verdict;             /* FRAME_V_* */
    uint32_t img_len;             /* bytes of encoded image (0 = none) */
    uint32_t rep_len;             /* bytes of report JSON, follows the image */
    uint32_t reserved0;
    uint8_t  payload[FRAME_PAYLOAD_MAX];
};

static_assert(sizeof(FrameRingHeader) == 64, "FrameRingHeader must stay 64 bytes");
static_assert(sizeof(FrameSlot) == FRAME_SLOT_BYTES, "FrameSlot must fill its slot exactly");

inline size_t frame_ring_total_bytes(uint32_t slot_count) {
    return sizeof(FrameRingHeader) + (size_t)slot_count * FRAME_SLOT_BYTES;
}

inline FrameSlot *frame_ring_slot(void *mapping, uint64_t idx) {
    auto *h = static_cast<FrameRingHeader *>(mapping);
    auto *base = reinterpret_cast<uint8_t *>(mapping) + sizeof(FrameRingHeader);
    return reinterpret_cast<FrameSlot *>(base + (idx % h->slot_count) * FRAME_SLOT_BYTES);
}

#endif /* FRAME_RING_H */
