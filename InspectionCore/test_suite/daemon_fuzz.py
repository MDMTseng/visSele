#!/usr/bin/env python3
"""
Daemon BPG fuzzer — sends deliberately-weird BPG packets to ws://localhost:4190
and after EACH case verifies the daemon still answers an HR heartbeat within 3s.

PASS = HR reply received after the weird packet.
FAIL = no HR reply (timeout / connection dropped / daemon wedged).

Run:
    python3 daemon_fuzz.py [port]
"""
import asyncio, json, struct, sys, os, random

import websockets


PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 4190
URI  = f"ws://localhost:{PORT}"


def bpg_pack(tl: bytes, payload: bytes = b"", prop: int = 0, pgid: int = 0,
             size_override: int = None) -> bytes:
    assert len(tl) == 2
    size = size_override if size_override is not None else len(payload)
    hdr = struct.pack(">2sBHI", tl, prop & 0xFF, pgid & 0xFFFF, size & 0xFFFFFFFF)
    return hdr + payload


def bpg_unpack(buf: bytes):
    if len(buf) < 9:
        return None, buf
    tl, prop, pgid, size = struct.unpack(">2sBHI", buf[:9])
    if len(buf) < 9 + size:
        return None, buf
    return (tl.decode("ascii", "replace"), prop, pgid, buf[9:9 + size]), buf[9 + size:]


async def recv_one(ws, overall_timeout=3.0):
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
        except Exception as e:
            return None, ("__exc__:" + repr(e)).encode()
        if isinstance(frame, str):
            frame = frame.encode("utf-8", "replace")
        buf += frame
        pkt, buf = bpg_unpack(buf)
        if pkt:
            return pkt, buf


async def hr_alive(ws, timeout=3.0):
    """Send HR, return True iff a reply arrives within timeout."""
    try:
        await ws.send(bpg_pack(b"HR"))
    except Exception:
        return False, "send-failed"
    pkt, _ = await recv_one(ws, overall_timeout=timeout)
    if pkt is None:
        return False, "no-reply-3s"
    tl, prop, pgid, payload = pkt
    return True, f"tl={tl} sz={len(payload)}"


async def reconnect():
    return await websockets.connect(URI, max_size=2**24)


async def run_case(ws, name, sender):
    """Run a single weird-packet case and verify HR liveness after."""
    detail = ""
    try:
        await sender(ws)
    except Exception as e:
        # If our send itself blew up (e.g. ws closed by daemon), record it.
        detail = f"send-exc={type(e).__name__}:{e}; "
    # Drain any unsolicited reply (best-effort, very short).
    try:
        await asyncio.wait_for(ws.recv(), timeout=0.1)
    except Exception:
        pass
    # Now liveness check.
    try:
        ok, info = await hr_alive(ws, timeout=3.0)
    except Exception as e:
        ok, info = False, f"hr-exc={type(e).__name__}:{e}"
    return ok, detail + info


# ---------- Senders ----------

async def s_invalid_tl_XX(ws):
    await ws.send(bpg_pack(b"XX", b"hello"))

async def s_invalid_tl_ZZ(ws):
    await ws.send(bpg_pack(b"ZZ", json.dumps({"x": 1}).encode()))

async def s_invalid_tl_nul(ws):
    await ws.send(bpg_pack(b"\x00\x00", b""))

async def s_invalid_tl_random(ws):
    await ws.send(bpg_pack(bytes([random.randint(0, 255), random.randint(0, 255)]),
                           b"random-junk"))

async def s_oversize_field(ws):
    # Claims 1 MiB payload, only sends 4 bytes.
    await ws.send(bpg_pack(b"HR", b"abcd", size_override=1024 * 1024))

async def s_too_short_header(ws):
    # 5 bytes, less than the 9-byte header.
    await ws.send(b"\x00\x01\x02\x03\x04")

async def s_II_nonsense_json(ws):
    await ws.send(bpg_pack(b"II", b"{this is not json::::"))

async def s_CI_nonsense_json(ws):
    await ws.send(bpg_pack(b"CI", b"not-json-at-all <<<>>>"))

async def s_FI_nonsense_json(ws):
    await ws.send(bpg_pack(b"FI", b"\xff\xfe\xfd garbage"))

async def s_LD_malformed(ws):
    await ws.send(bpg_pack(b"LD", b'{"def": "broken'))  # unterminated string

async def s_SV_no_body(ws):
    await ws.send(bpg_pack(b"SV"))

async def s_II_shell_meta(ws):
    payload = json.dumps({"imgsrc": "/tmp/$(rm -rf /).png ; cat /etc/passwd `id`"}).encode()
    await ws.send(bpg_pack(b"II", payload))

async def s_II_rapid_burst(ws):
    payload = json.dumps({"imgsrc": "/tmp/nope_burst.png"}).encode()
    pkt = bpg_pack(b"II", payload)
    for _ in range(30):
        await ws.send(pkt)

async def s_II_bloated_json(ws):
    # imgsrc path is short, but the JSON object is bloated with garbage keys.
    garbage = "A" * (1 * 1024 * 1024)  # 1 MiB of A's inside JSON
    payload = json.dumps({"imgsrc": "/tmp/x.png", "junk": garbage}).encode()
    await ws.send(bpg_pack(b"II", payload))

async def s_HR_huge_bogus_payload(ws):
    blob = os.urandom(512 * 1024)  # 512 KiB
    await ws.send(bpg_pack(b"HR", blob))

async def s_split_across_frames(ws):
    # One BPG packet split across multiple WebSocket binary frames.
    pkt = bpg_pack(b"HR", b"split-test")
    mid = len(pkt) // 2
    await ws.send(pkt[:mid])
    await asyncio.sleep(0.05)
    await ws.send(pkt[mid:])

async def s_header_then_drip(ws):
    # Header announces 32 bytes, send 4 bytes, then 28 bytes 200ms later.
    payload = b"DRIP" + b"x" * 28
    hdr = struct.pack(">2sBHI", b"HR", 0, 0, len(payload))
    await ws.send(hdr + payload[:4])
    await asyncio.sleep(0.2)
    await ws.send(payload[4:])


CASES = [
    ("invalid_tl_XX",          s_invalid_tl_XX),
    ("invalid_tl_ZZ",          s_invalid_tl_ZZ),
    ("invalid_tl_nul",         s_invalid_tl_nul),
    ("invalid_tl_random",      s_invalid_tl_random),
    ("oversize_size_field",    s_oversize_field),
    ("too_short_header",       s_too_short_header),
    ("II_nonsense_json",       s_II_nonsense_json),
    ("CI_nonsense_json",       s_CI_nonsense_json),
    ("FI_nonsense_json",       s_FI_nonsense_json),
    ("LD_malformed",           s_LD_malformed),
    ("SV_no_body",             s_SV_no_body),
    ("II_imgsrc_shell_meta",   s_II_shell_meta),
    ("II_rapid_30x_burst",     s_II_rapid_burst),
    ("II_bloated_json_1MiB",   s_II_bloated_json),
    ("HR_huge_bogus_512KiB",   s_HR_huge_bogus_payload),
    ("split_across_2_frames",  s_split_across_frames),
    ("header_then_drip",       s_header_then_drip),
]


async def fuzz():
    print(f"==== BPG fuzz @ {URI} ====")
    try:
        ws = await reconnect()
    except Exception as e:
        print(f"  FAIL  connect  {type(e).__name__}: {e}")
        return 0, 1, 1

    # Baseline HR.
    ok, info = await hr_alive(ws)
    print(f"  {'PASS' if ok else 'FAIL'}  baseline_HR                  {info}")
    if not ok:
        try: await ws.close()
        except: pass
        return 0, 1, 1

    passed = 0
    failed = 0
    total  = 0
    real_issues = []

    for name, sender in CASES:
        total += 1
        try:
            ok, detail = await run_case(ws, name, sender)
        except Exception as e:
            ok, detail = False, f"case-exc={type(e).__name__}:{e}"

        if not ok:
            # Try to reconnect to keep going + figure out if daemon-side is dead.
            try: await ws.close()
            except: pass
            try:
                ws = await reconnect()
                ok2, info2 = await hr_alive(ws)
                if ok2:
                    detail += f"; recovered-after-reconnect ({info2})"
                    real_issues.append(f"{name}: daemon dropped connection (recoverable)")
                else:
                    detail += f"; reconnect-HR-failed ({info2})"
                    real_issues.append(f"{name}: daemon UNREACHABLE after reconnect")
            except Exception as e:
                detail += f"; reconnect-exc={type(e).__name__}:{e}"
                real_issues.append(f"{name}: reconnect raised {type(e).__name__}")
                break

        if ok:
            passed += 1
            print(f"  PASS  {name:<28} {detail}")
        else:
            failed += 1
            print(f"  FAIL  {name:<28} {detail}")

    try: await ws.close()
    except: pass

    print(f"\n{passed}/{total} pass, {failed} fail")
    if real_issues:
        print("\nReal daemon issues observed:")
        for r in real_issues:
            print(f"  - {r}")
    return passed, total, failed


def main():
    try:
        passed, total, failed = asyncio.run(fuzz())
    except KeyboardInterrupt:
        return 130
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
