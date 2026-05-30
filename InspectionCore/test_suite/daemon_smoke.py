#!/usr/bin/env python3
"""
Daemon smoke test — sends a few BPG packets to the sandbox visSele on
ws://localhost:4190 and verifies it stays alive + responds. This is the
minimum-viable BPG-protocol test client; it enables future migration of the
daemon's LoadIMGFile sites to cv::imread under real coverage.

BPG framing (9-byte header, see BPG_Protocol/BPG_Protocol.cpp:22-50):
    [0..1] tl       : 2-byte command (e.g. "HR", "II")
    [2]    prop     : 1-byte property
    [3..4] pgID     : 16-bit big-endian packet group id
    [5..8] size     : 32-bit big-endian payload length
    [9..]  payload  : raw bytes (typically JSON text)

WebSocket transport: each BPG packet rides in one (or more) BINARY frames.
This client sends every packet as a single BINARY frame and reassembles
inbound frames into BPG packets via the same header.

Run:
    python3 daemon_smoke.py [port]    # default port 4190
Exit 0 = daemon responds + survives; nonzero = crash / timeout / no response.
"""
import asyncio, json, struct, sys, os
import websockets


PORT     = int(sys.argv[1]) if len(sys.argv) > 1 else 4190
URI      = f"ws://localhost:{PORT}"
GOLDEN   = "/Users/mdm/workspace/HY_sync/DEV/test/10221 BOS-LT12BH4211 SORTING_bk.png"


def bpg_pack(tl: bytes, payload: bytes = b"", prop: int = 0, pgid: int = 0) -> bytes:
    assert len(tl) == 2, "tl must be 2 bytes"
    hdr = struct.pack(">2sBHI", tl, prop, pgid, len(payload))
    return hdr + payload


def bpg_unpack(buf: bytes):
    if len(buf) < 9:
        return None, buf
    tl, prop, pgid, size = struct.unpack(">2sBHI", buf[:9])
    if len(buf) < 9 + size:
        return None, buf
    payload = buf[9:9 + size]
    return (tl.decode("ascii", "replace"), prop, pgid, payload), buf[9 + size:]


async def recv_one(ws, overall_timeout=5.0):
    """Wait up to overall_timeout for one full BPG packet from inbound frames."""
    deadline = asyncio.get_event_loop().time() + overall_timeout
    buf = b""
    while True:
        remaining = deadline - asyncio.get_event_loop().time()
        if remaining <= 0:
            return None, buf
        try:
            frame = await asyncio.wait_for(ws.recv(), timeout=remaining)
        except asyncio.TimeoutError:
            return None, buf
        if isinstance(frame, str):
            frame = frame.encode("utf-8", "replace")
        buf += frame
        pkt, buf = bpg_unpack(buf)
        if pkt:
            return pkt, buf


async def smoke():
    results = {"port": PORT, "uri": URI, "tests": []}

    def add(name, ok, detail=""):
        results["tests"].append({"name": name, "ok": ok, "detail": detail})
        marker = "PASS" if ok else "FAIL"
        print(f"  {marker}  {name:<30} {detail}")

    print(f"==== BPG smoke @ {URI} ====")

    # 1. Connect
    try:
        ws = await websockets.connect(URI, max_size=2**24)
    except Exception as e:
        add("connect", False, f"{type(e).__name__}: {e}")
        return results, 1
    add("connect", True, "")

    # 2. HR heartbeat -- the daemon's wiringPanel:1557 handler. Empty payload.
    try:
        await ws.send(bpg_pack(b"HR"))
        pkt, _ = await recv_one(ws, overall_timeout=3.0)
        if pkt is None:
            add("HR_response", False, "no reply within 3s")
        else:
            tl, prop, pgid, payload = pkt
            add("HR_response", True, f"tl={tl} prop={prop} pgid={pgid} payload[:60]={payload[:60]!r}")
    except Exception as e:
        add("HR_response", False, f"{type(e).__name__}: {e}")

    # 3. Daemon still alive after heartbeat? Quick second HR.
    try:
        await ws.send(bpg_pack(b"HR"))
        pkt, _ = await recv_one(ws, overall_timeout=3.0)
        add("HR_again", pkt is not None,
            "" if pkt else "no reply (daemon may have wedged)")
    except Exception as e:
        add("HR_again", False, f"{type(e).__name__}: {e}")

    # 4. II image-inspection with imgsrc pointing at the golden file --
    # this exercises wiringPanel's BPG LoadIMGFile site (~line 2033/2372).
    if os.path.exists(GOLDEN):
        payload = json.dumps({"imgsrc": GOLDEN}).encode("utf-8")
        try:
            await ws.send(bpg_pack(b"II", payload))
            pkt, _ = await recv_one(ws, overall_timeout=15.0)
            if pkt is None:
                add("II_imgsrc_response", False, "no reply within 15s")
            else:
                tl, prop, pgid, payload = pkt
                add("II_imgsrc_response", True, f"tl={tl} size={len(payload)}")
        except Exception as e:
            add("II_imgsrc_response", False, f"{type(e).__name__}: {e}")

        # 5. Daemon still alive after II?
        try:
            await ws.send(bpg_pack(b"HR"))
            pkt, _ = await recv_one(ws, overall_timeout=3.0)
            add("HR_after_II", pkt is not None,
                "" if pkt else "no reply (daemon wedged after II)")
        except Exception as e:
            add("HR_after_II", False, f"{type(e).__name__}: {e}")
    else:
        add("II_imgsrc_response", False, f"GOLDEN missing: {GOLDEN}")

    # 6. Malformed II (nonexistent imgsrc): daemon must reject, not crash.
    payload = json.dumps({"imgsrc": "/tmp/does_not_exist_xyz.png"}).encode("utf-8")
    try:
        await ws.send(bpg_pack(b"II", payload))
        pkt, _ = await recv_one(ws, overall_timeout=5.0)
        add("II_bad_imgsrc", pkt is not None,
            "" if pkt else "no reply (daemon may have crashed on bad imgsrc)")
    except Exception as e:
        add("II_bad_imgsrc", False, f"{type(e).__name__}: {e}")

    # 7. Final liveness check.
    try:
        await ws.send(bpg_pack(b"HR"))
        pkt, _ = await recv_one(ws, overall_timeout=3.0)
        add("liveness_final", pkt is not None,
            "" if pkt else "no final HR reply (daemon dead)")
    except Exception as e:
        add("liveness_final", False, f"{type(e).__name__}: {e}")

    await ws.close()
    fails = sum(1 for t in results["tests"] if not t["ok"])
    return results, fails


def main():
    try:
        results, fails = asyncio.run(smoke())
    except KeyboardInterrupt:
        return 130
    print(f"\n{len(results['tests']) - fails}/{len(results['tests'])} pass")
    return 0 if fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
