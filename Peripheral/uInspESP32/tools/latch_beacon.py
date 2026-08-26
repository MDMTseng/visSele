#!/usr/bin/env python3
"""A latched link must SAY SO, once a second, and say what to send to fix it.

    python latch_beacon.py --port COM3

THIS REBOOTS THE BOARD (opening the port asserts DTR/RTS). Bench boards only.

Why this exists
---------------
A protocol error latches the link. The board then keeps emitting its 1 Hz
SYSTIME heartbeat and accepts no command, so from the host it looks alive and
well -- the symptom recorded in three separate documents in this project as
"從主機看它是活的". The core meanwhile pings a deaf device forever.

Two things fix that, and both are checked here.

A missing field is a SEMANTIC error, not a protocol one. `{}` on its own used
to latch the link; the framing was perfect, the CRC matched, the JSON parsed.
Now it is answered with `err:"missing_type"` and the link stays up.

And while the link IS genuinely latched, a `latched` frame goes out once a
second carrying how long, what to send to recover, and -- the field that
matters -- how many bytes have been discarded. "The board went silent" and
"the board is throwing away everything you send" look identical from the host
and are completely different problems.

The beacon is a SEPARATE frame from SYSTIME on purpose: three diagnostic tools
parse that line's exact text (cmd_sweep.mjs matches /SYSTIME: (\d+) ms/ to spot
a reboot), and a heartbeat whose format moves is one they stop trusting.
"""
import argparse
import json
import sys
import time

import serial
NL=bytes([10])
def crc16(d):
    c=0xFFFF
    for b in d:
        c^=b<<8
        for _ in range(8): c=((c<<1)^0x1021)&0xFFFF if c&0x8000 else (c<<1)&0xFFFF
    return c
_ap = argparse.ArgumentParser()
_ap.add_argument("--port", required=True)
_args = _ap.parse_args()
ser=serial.Serial(_args.port,230400,timeout=0.3); time.sleep(1.0); ser.reset_input_buffer()
fails=0
def ok(c,m,d=""):
    global fails
    print(("PASS  " if c else "FAIL  ")+m+(("  -- "+d) if d else ""))
    if not c: fails+=1
def pump(sec):
    buf="";out=[];d=0;instr=False;esc=False;t0=time.time()
    while time.time()-t0<sec:
        for ch in ser.read(1024).decode("utf-8","replace"):
            if d==0 and ch!="{": continue
            buf+=ch
            if esc: esc=False; continue
            if ch=="\\": esc=True; continue
            if ch=='"': instr=not instr; continue
            if instr: continue
            if ch=="{": d+=1
            elif ch=="}":
                d-=1
                if d==0: out.append(buf); buf=""
    return out,buf
def send(txt,sec=1.8):
    raw=txt.encode(); raw+=b"*%04X"%crc16(raw)+NL
    ser.write(raw); ser.flush(); return pump(sec)
def alive():
    for _ in range(3):
        f,_=send('{"type":"PING","id":555}')
        if any('"pong"' in x for x in f): return True
    return False
# Wait for the board to be UP, not for a duration. Straight after a flash it is
# still booting and every check below fails for that reason and no other.
up=False
for _ in range(20):
    f,_=pump(1.0)
    if any('SYSTIME' in x for x in f): up=True; break
ok(up,"board is up (heartbeat seen)")
ser.reset_input_buffer()
ok(alive(),"baseline: board answers PING")

# --- 1. a bare {} must be answered, not latch -----------------------------
f,_=send('{}')
answered=[x for x in f if 'missing_type' in x]
ok(bool(answered),"a bare {} is ANSWERED with a reason", answered[0][:80] if answered else repr(f)[:80])
ok(alive(),"and the link is NOT latched afterwards")

# --- 2. the 1 Hz beacon while genuinely latched ---------------------------
ser.write(b"G"*800); ser.flush()          # real protocol error, no newline
f,_=pump(4.0)
beacons=[x for x in f if '"latched"' in x]
ok(len(beacons)>=2,"while latched, a beacon arrives ~1/sec","n=%d in 4s"%len(beacons))
if beacons:
    print("     ",beacons[0][:150])
    print("     ",beacons[-1][:150])
    import re
    ds=[int(m.group(1)) for m in (re.search(r"discarded=(\d+)",b) for b in beacons) if m]
    ok(bool(ds) and max(ds)>0,"the beacon reports bytes discarded","discarded=%s"%ds[:4])
    ok(len(ds)>1 and ds[-1]>=ds[0],"and the count reflects continuing discards","%s -> %s"%(ds[0],ds[-1]))
    sm=[int(m.group(1)) for m in (re.search(r"since_ms=(\d+)",b) for b in beacons) if m]
    ok(len(sm)>1 and sm[-1]>sm[0],"and says how long it has been latched","%s -> %s ms"%(sm[0],sm[-1]))

# more garbage must push the discarded count up
before = beacons[-1] if beacons else ""
ser.write(b"H"*1200); ser.flush()
f2,_=pump(3.0)
b2=[x for x in f2 if '"latched"' in x]
import re
d1=[int(m.group(1)) for m in (re.search(r"discarded=(\d+)",b) for b in beacons) if m]
d2=[int(m.group(1)) for m in (re.search(r"discarded=(\d+)",b) for b in b2) if m]
ok(bool(d2) and bool(d1) and max(d2)>max(d1),"more garbage raises the discarded count","%s -> %s"%(max(d1) if d1 else None,max(d2) if d2 else None))

# --- 3. recovery, and the beacon stops ------------------------------------
send('{"type":"clear_error"}',2.0)
f3,_=pump(3.0)
ok(not [x for x in f3 if '"latched"' in x],"after clear_error the beacon STOPS")
ok(alive(),"and the board answers commands again")
ser.close()
print(("%d FAILURES"%fails) if fails else "--- all pass ---")
sys.exit(1 if fails else 0)
