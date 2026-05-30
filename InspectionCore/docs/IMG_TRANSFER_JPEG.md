# JPEG-per-event image transfer (wire format note)

The BPG `SEND_acvImage` path (`Core0_1/wiringPanel.cpp`) gained an opt-in JPEG
mode for per-event image transfers (snapshots, datView pushes, the live
inspection stream). Wire format is back-compatible: the receiver just needs to
look at the first byte of the existing 15-byte metadata sub-frame.

## Setting it

Send a `GS` (settings) BPG packet with payload JSON:

```json
{ "IMG_STREAMING_JPEG_QUALITY": 75 }
```

- `0`  → legacy raw RGBA, 4 bytes / pixel (current default).
- `1`..`100` → JPEG with that quality. Typical: 75 for high-fidelity,
  60-65 for bandwidth-bound, 85+ for archival.

Set it once per session; takes effect on the next outgoing image.

## Wire format the receiver sees

Each image transfer is one WebSocket message reassembled from multiple
fragments. The first fragment (after the 9-byte BPG header) carries a 15-byte
**metadata sub-frame**:

| Offset | Bytes | Meaning |
|--------|-------|---------|
| 0      | 1     | **format** (NEW): `0` = raw RGBA (legacy), `1` = JPEG |
| 1      | 1     | **JPEG quality used** (only when format==1; legacy: `0`) |
| 2..3   | 2     | offsetX (big-endian) |
| 4..5   | 2     | offsetY (big-endian) |
| 6..7   | 2     | width (big-endian) -- of the ROI being sent |
| 8..9   | 2     | height (big-endian) -- of the ROI |
| 10     | 1     | scale (down-sample level) |
| 11..12 | 2     | fullWidth (big-endian) -- of the un-downsampled image |
| 13..14 | 2     | fullHeight (big-endian) |

Subsequent fragments contain the payload:

- **format = 0** (legacy): width*height pixels, 4 bytes each in `R,G,B,255` order.
- **format = 1**: a complete JPEG file (everything after the metadata sub-frame
  is JPEG bytes; reassemble the fragments and feed to any JPEG decoder /
  `Image()` constructor / `Blob` -> `URL.createObjectURL`).

## Receiver pseudocode

```js
// after reassembling all fragments into one ArrayBuffer `msg`:
const meta = new DataView(msg, 9, 15);     // 9 = BPG header
const format = meta.getUint8(0);
const w = meta.getUint16(6), h = meta.getUint16(8);
const payload = new Uint8Array(msg, 9 + 15);

if (format === 1) {
  // JPEG -- everything after the 15-byte metadata sub-frame
  const blob = new Blob([payload], { type: 'image/jpeg' });
  imgEl.src = URL.createObjectURL(blob);
} else {
  // legacy raw RGBA
  // ...existing 4-bytes-per-pixel decoder
}
```

## Performance sketch on the 5 MP golden

- Raw RGBA: ~20 MB/frame.
- JPEG quality 75: ~300-500 kB/frame (40-60× smaller).
- Encode cost: a single `cv::imencode` call per frame on the daemon side, well
  under 100 ms on the dev hardware.
- Decode: any browser does it for free.

## Why the receiver has to opt in

Default stays raw RGBA so the current WebUI keeps working unchanged. Once the
WebUI receiver handles both formats, the flag can be enabled per-session via
the `GS` command and the WebUI receives JPEG.
