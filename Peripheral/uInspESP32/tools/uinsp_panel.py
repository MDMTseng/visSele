#!/usr/bin/env python3
"""uinsp_panel -- browser panel for bringing up a new uInspESP32 board.

Serves a single-page UI (stdlib http.server, no dependencies) that exercises
each peripheral over the firmware's JSON-serial protocol:

  * stepper   -- enable/disable, plate_freq, start/stop (enter/exit_insp_mode)
  * blow valve / feeder -- PIN_ON / PIN_OFF on any output pin (default 21)
  * selectors -- sel_act pulses on SEL1/2/3 (pins 25/26/32)
  * gate sensor -- live level of pin 27 via PIN_READ polling (firmware >= the
    build that adds PIN_READ; the panel degrades gracefully without it)
  * raw command box + live async event log (cam_trig, system_info, dbg)

Usage:
    python3 uinsp_panel.py --port /dev/cu.usbserial-0001 [--http-port 8765]
"""

import argparse
import json
import queue
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from uinsp_test import UInspLink, LinkReset, LinkDead

# Pin map mirrored from include/config/HardwareConfig.hpp
PIN_MAP = [
    ("GATE (in)", 27),
    ("FEEDER/blow", 21),
    ("SEL1", 25),
    ("SEL2", 26),
    ("SEL3", 32),
    ("STEP EN", 13),
    ("L1A", 16),
    ("CAM1", 17),
    ("L2A", 18),
    ("CAM2", 19),
]
POLL_PINS = [p for _, p in PIN_MAP]

STATE_NAMES = {
    0: "INIT", 100: "IDLE", 101: "READY", 112: "ERROR",
    113: "FATAL", 140: "TEST",
}


class Panel:
    """Owns the serial link and fans device data out to SSE clients."""

    def __init__(self, port):
        self.port = port
        self.link = None
        self.link_err = "connecting..."
        # One command in flight at a time: UInspLink.send's id counter is not
        # designed for concurrent callers, and interleaved writes would corrupt
        # frames anyway.
        self.cmd_lock = threading.Lock()
        self.clients = set()
        self.clients_lock = threading.Lock()
        self.pin_read_ok = None   # None=unknown, True/False once probed
        # Camera-less test mode: answer every cam_trig with a verdict following
        # a repeating pattern of M ok, N ng, O NA. None = off.
        self.autorep = None
        self._stop = False
        threading.Thread(target=self._link_keeper, daemon=True).start()
        threading.Thread(target=self._poller, daemon=True).start()
        threading.Thread(target=self._async_pump, daemon=True).start()

    # -- link lifecycle ----------------------------------------------------

    def _link_keeper(self):
        """Open the link; if it can't be opened (board unplugged), retry
        forever so the panel can be started before the board is connected."""
        while not self._stop:
            if self.link is None:
                try:
                    lk = UInspLink(self.port)
                    lk.auto_reconnect = True
                    self.link = lk
                    self.link_err = None
                    self.pin_read_ok = None
                    self.broadcast({"panel": "link", "up": True})
                except Exception as exc:
                    self.link_err = str(exc)
            time.sleep(1.0)

    def send(self, obj, timeout=2.0, nowait=False):
        lk = self.link
        if lk is None:
            return {"panel_err": "link down: %s" % self.link_err}
        with self.cmd_lock:
            try:
                if nowait:
                    lk.send_nowait(obj)
                    return {"sent": True}
                rep = lk.send(obj, timeout=timeout)
                return rep if rep is not None else {"panel_err": "no reply (timeout)"}
            except (LinkReset, LinkDead) as exc:
                return {"panel_err": "link reset: %s" % exc.__class__.__name__}
            except Exception as exc:
                return {"panel_err": str(exc)}

    # -- SSE fan-out -------------------------------------------------------

    def broadcast(self, obj):
        raw = json.dumps(obj, separators=(",", ":"))
        with self.clients_lock:
            for q in list(self.clients):
                try:
                    q.put_nowait(raw)
                except queue.Full:
                    pass

    def subscribe(self):
        q = queue.Queue(maxsize=500)
        with self.clients_lock:
            self.clients.add(q)
        return q

    def unsubscribe(self, q):
        with self.clients_lock:
            self.clients.discard(q)

    def _async_pump(self):
        """Dedicated low-latency consumer of device async messages.

        Auto-report verdicts must beat the SWITCH pulse deadline; the poller
        loop's pin/stat round-trips (up to ~1.5s) are far too slow for that,
        so this thread wakes on the link's async event and answers a
        cam_trig within milliseconds. It is also the single drain point --
        SSE forwarding happens here too.
        """
        while not self._stop:
            lk = self.link
            if lk is None:
                time.sleep(0.5)
                continue
            lk._async_ev.wait(0.2)
            lk._async_ev.clear()
            for msg in lk.drain_async():
                self._maybe_autoreport(lk, msg)
                obj = msg[1] if isinstance(msg, (list, tuple)) and len(msg) == 2 else msg
                if isinstance(obj, dict) and obj.get("type") == "system_info" \
                        and obj.get("state") == 112:
                    threading.Thread(target=self._diag_error, daemon=True).start()
                self.broadcast({"panel": "async", "msg": msg})

    def _diag_error(self):
        """Fired when the machine drops to ERROR: explain the timing math so
        the log says WHY, not just that it stopped."""
        try:
            g = self.send({"type": "get_setup"}, timeout=3.0)
            spo = (g or {}).get("stage_pulse_offset") or {}
            st = self.send({"type": "get_running_stat"}, timeout=3.0)
            freq = float(g.get("plate_freq") or 0)
            win = int(spo.get("SWITCH", 0)) - int(spo.get("CAM1_on", 0))
            win_ms = win / (2.0 * freq) * 1000 if freq else 0
            lat = (st or {}).get("report_latency") or {}
            lat_ms = lat.get("avg_us", 0) / 1000.0
            txt = ("ERROR analysis: answer window CAM1->SWITCH = %d ticks ="
                   " %.0fms @ plate_freq %g; host report latency avg %.0fms"
                   " (n=%d)." % (win, win_ms, freq, lat_ms, lat.get("n", 0)))
            if lat.get("n") and win_ms and lat_ms >= win_ms:
                txt += (" Latency >= window: the host CANNOT answer in time --"
                        " raise the SWITCH offset (Station offsets card) or"
                        " lower plate_freq for serial testing.")
            elif not (self.autorep):
                txt += (" Auto-report is OFF -- nothing answers objects, so"
                        " any part reaching SWITCH stops the line.")
            self.broadcast({"panel": "diag", "text": txt})
        except Exception:
            pass

    def _maybe_autoreport(self, lk, msg):
        ar = self.autorep
        if not ar:
            return
        obj = msg[1] if isinstance(msg, (list, tuple)) and len(msg) == 2 else msg
        if not isinstance(obj, dict) or obj.get("type") not in ("cam_trig", "cam_trig_tagged"):
            return
        tid = obj.get("tid")
        if tid is None:
            return
        # Every object announces twice (CAM1 cam=1, CAM2 cam=2); a second
        # report for the same tid would advance the pattern AND desync the
        # machine, so answer each tid exactly once.
        if tid in ar["answered"]:
            return
        ar["answered"].add(tid)
        if len(ar["answered"]) > 10000:
            ar["answered"].clear()
        total = ar["m"] + ar["n"] + ar["o"]
        if total <= 0:
            return
        pos = ar["i"] % total
        if pos < ar["m"]:
            cat, key = ar["cat_ok"], "ok"
        elif pos < ar["m"] + ar["n"]:
            cat, key = ar["cat_ng"], "ng"
        else:
            cat, key = ar["cat_na"], "na"
        ar["i"] += 1
        ar[key] = ar.get(key, 0) + 1
        # No cmd_lock here: the link's own _tx_lock serializes the write, and
        # waiting behind a poller round-trip would blow the SWITCH deadline.
        try:
            lk.send_nowait({"type": "report", "tid": tid, "cat": cat})
        except Exception:
            pass

    # -- background poll ---------------------------------------------------

    def _poller(self):
        last_stat = 0.0
        pinread_fails = 0
        while not self._stop:
            lk = self.link
            if lk is None:
                self.broadcast({"panel": "link", "up": False, "err": self.link_err})
                time.sleep(1.0)
                continue

            # (async chatter is drained by _async_pump, not here)

            # Live pin levels. Older firmware has no PIN_READ; stop hammering
            # it after a few silent tries but keep the rest of the panel alive.
            if self.pin_read_ok is not False:
                rep = self.send({"type": "pin_read", "pins": POLL_PINS}, timeout=0.6)
                if rep.get("ack") and "vals" in rep:
                    self.pin_read_ok = True
                    pinread_fails = 0
                    self.broadcast({"panel": "pins",
                                    "pins": POLL_PINS, "vals": rep["vals"]})
                elif "panel_err" in rep or not rep.get("ack"):
                    pinread_fails += 1
                    if pinread_fails >= 4 and self.pin_read_ok is None:
                        self.pin_read_ok = False
                        self.broadcast({"panel": "pins_unsupported"})

            # Slow status: state + counters + plate_freq.
            if time.time() - last_stat > 1.5:
                last_stat = time.time()
                rep = self.send({"type": "get_running_stat"}, timeout=0.8)
                if "state" in rep:
                    st = rep.get("state")
                    ar = self.autorep
                    self.broadcast({"panel": "stat",
                                    "state": st,
                                    "state_name": STATE_NAMES.get(st, str(st)),
                                    "plate_freq": rep.get("plate_freq"),
                                    "count": rep.get("count"),
                                    "pipe": rep.get("pipe"),
                                    "report_latency": rep.get("report_latency"),
                                    "reconnects": lk.reconnects,
                                    "autorep": ({"ok": ar.get("ok", 0),
                                                 "ng": ar.get("ng", 0),
                                                 "na": ar.get("na", 0)}
                                                if ar else None)})
            time.sleep(0.25)


PANEL = None  # set in main()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _json(self, obj, code=200):
        raw = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/index"):
            raw = PAGE.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
        elif self.path == "/events":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            q = PANEL.subscribe()
            try:
                # Prime the client with current knowledge.
                self.wfile.write(b": hello\n\n")
                self.wfile.flush()
                while True:
                    try:
                        raw = q.get(timeout=15.0)
                        self.wfile.write(b"data: " + raw.encode() + b"\n\n")
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
            finally:
                PANEL.unsubscribe(q)
        else:
            self._json({"err": "not found"}, 404)

    def do_POST(self):
        if self.path == "/autoreport":
            try:
                n = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(n).decode() or "{}")
                if body.get("on"):
                    PANEL.autorep = {
                        "m": max(0, int(body.get("m", 1))),
                        "n": max(0, int(body.get("n", 0))),
                        "o": max(0, int(body.get("o", 0))),
                        "cat_ok": int(body.get("cat_ok", 1)),
                        "cat_ng": int(body.get("cat_ng", 2)),
                        "cat_na": int(body.get("cat_na", 0xFFFF)),
                        "i": 0, "ok": 0, "ng": 0, "na": 0,
                        "answered": set(),
                    }
                else:
                    PANEL.autorep = None
                ar = PANEL.autorep
                self._json({"autorep": (
                    {k: v for k, v in ar.items() if k != "answered"}
                    if ar else None)})
            except Exception as exc:
                self._json({"err": str(exc)}, 500)
            return
        if self.path != "/cmd":
            self._json({"err": "not found"}, 404)
            return
        try:
            n = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(n).decode() or "{}")
            cmd = body.get("cmd")
            if not isinstance(cmd, dict) or "type" not in cmd:
                self._json({"err": "body must be {cmd:{type:...}}"}, 400)
                return
            rep = PANEL.send(cmd,
                             timeout=float(body.get("timeout", 2.0)),
                             nowait=bool(body.get("nowait", False)))
            self._json({"reply": rep})
        except Exception as exc:
            self._json({"err": str(exc)}, 500)


PAGE = r"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>uInsp bring-up panel</title>
<style>
  body{font:14px -apple-system,Segoe UI,sans-serif;margin:0;background:#111;color:#ddd}
  header{display:flex;gap:16px;align-items:center;padding:10px 16px;background:#1b1b1f;
         border-bottom:1px solid #333;position:sticky;top:0}
  .dot{width:12px;height:12px;border-radius:50%;background:#666;display:inline-block}
  .dot.up{background:#3c3}.dot.down{background:#c33}
  main{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:12px;padding:12px}
  .card{background:#1b1b1f;border:1px solid #333;border-radius:8px;padding:12px}
  .card h2{margin:0 0 10px;font-size:15px;color:#8cf}
  button{background:#2a2a30;color:#ddd;border:1px solid #444;border-radius:6px;
         padding:7px 12px;margin:2px;cursor:pointer;font-size:13px}
  button:hover{background:#3a3a44}
  button.go{border-color:#3c3;color:#8f8}
  button.stop{border-color:#c33;color:#f99}
  input,select{background:#222;color:#ddd;border:1px solid #444;border-radius:5px;
        padding:6px;width:90px;font-size:13px}
  table{border-collapse:collapse;width:100%}
  td,th{border-bottom:1px solid #2c2c33;padding:3px 6px;text-align:left;font-size:12px}
  .hi{color:#8f8}.lo{color:#888}
  #gate{font-size:22px;font-weight:700;padding:14px;border-radius:8px;text-align:center;margin:6px 0}
  .gclear{background:#12351a;color:#7f7}.gblock{background:#3d1414;color:#f88}.gna{background:#222;color:#888}
  #log{height:220px;overflow-y:auto;background:#0c0c0e;border:1px solid #2a2a2a;
       font:11px Menlo,monospace;padding:6px;white-space:pre-wrap}
  textarea{width:100%;height:56px;background:#0c0c0e;color:#ddd;border:1px solid #444;
           border-radius:5px;font:12px Menlo,monospace;box-sizing:border-box}
  .kv{color:#aaa;font-size:12px}
  .warn{color:#fb5;font-size:12px}
</style></head><body>
<div id="errbanner" style="display:none;background:#5a1616;color:#fbb;padding:10px 16px;font-weight:600">
  Machine in ERROR state (a part reached the selector unanswered?)
  <button onclick="cmd({type:'clear_error'}).then(()=>cmd({type:'enter_insp_mode'}))">Clear error &amp; re-enter READY</button>
  <button onclick="cmd({type:'clear_error'})">Clear only</button>
</div>
<header>
  <span class="dot" id="linkdot"></span>
  <b>uInsp bring-up panel</b>
  <span class="kv" id="stateline">state: ?</span>
  <span class="kv" id="freqline">plate_freq: ?</span>
  <span class="kv" id="reconline"></span>
  <span class="warn" id="pinwarn" style="display:none">PIN_READ unsupported -- flash the new firmware for live pin levels</span>
</header>
<main>

<div class="card"><h2>Stepper</h2>
  <div>
    <button onclick="cmd({type:'stepper_enable'})">Enable driver</button>
    <button onclick="cmd({type:'stepper_disable'})">Disable driver</button>
  </div>
  <div style="margin-top:8px">
    plate_freq <input id="freq" type="number" value="1000" onchange="el('freqslide').value=this.value">
    accel <input id="accel" type="number" value="2000"> Hz/s
    <button onclick="cmd({type:'set_setup',plate_freq:+el('freq').value,plate_accel:+el('accel').value})">Set</button>
  </div>
  <div style="margin-top:8px">
    <input type="range" id="freqslide" min="0" max="4000" step="10" value="1000" style="width:100%"
      oninput="el('freq').value=this.value"
      onchange="cmd({type:'set_setup',plate_freq:+this.value})">
    <div class="kv" style="display:flex;justify-content:space-between"><span>0</span><span>drag = live set on release</span><span>4000</span></div>
  </div>
  <div style="margin-top:8px">
    <button class="go" onclick="cmd({type:'stepper_enable'});cmd({type:'enter_insp_mode'})">&#9654; Start (enter insp mode)</button>
    <button class="stop" onclick="cmd({type:'exit_insp_mode'})">&#9632; Stop</button>
  </div>
  <div class="kv" style="margin-top:6px">start = enable + enter_insp_mode; the plate spins at plate_freq. Stop = exit_insp_mode.</div>
  <div style="margin-top:8px;border-top:1px solid #2c2c33;padding-top:8px">
    EN active <select id="enact" style="width:110px">
      <option value="0">LOW (direct)</option>
      <option value="1">HIGH (common-anode)</option>
    </select>
    DIR <select id="dirlvl" style="width:60px"><option value="0">0</option><option value="1">1</option></select>
    <button onclick="cmd({type:'set_setup',stepper_en_active:+el('enact').value,stepper_dir:+el('dirlvl').value})">Apply</button>
    <button onclick="cmd({type:'save_setup'})">Save NVS</button>
  </div>
  <div class="kv">common +5V (共陽極) driver inputs invert every signal: pick which EN level enables, and the DIR level, then Save to persist.</div>
</div>

<div class="card"><h2>Blow valve / output pin</h2>
  pin <select id="vpin">
    <option value="21" selected>21 FEEDER/blow</option>
    <option value="25">25 SEL1</option>
    <option value="26">26 SEL2</option>
    <option value="32">32 SEL3</option>
    <option value="16">16 L1A</option>
    <option value="18">18 L2A</option>
  </select>
  <button onclick="cmd({type:'pin_mode',pin:+el('vpin').value,mode:'OUTPUT'})">Set OUTPUT</button>
  <div style="margin-top:8px">
    <button class="go" onclick="setOut(+el('vpin').value,true)">ON</button>
    <button class="stop" onclick="setOut(+el('vpin').value,false)">OFF</button>
    pulse <input id="vpulse" type="number" value="200">ms
    <button onclick="pulsePin(+el('vpin').value,+el('vpulse').value)">Pulse</button>
  </div>
  <div class="kv" style="margin-top:6px">ON/OFF are logical -- the wire level follows each output's "ON is" polarity below.</div>
</div>

<div class="card"><h2>IO polarity ("ON" level)</h2>
  <div id="polrows" style="display:grid;grid-template-columns:1fr 1fr;gap:4px"></div>
  <div style="margin-top:8px">
    <button onclick="applyPol()">Apply</button>
    <button onclick="cmd({type:'save_setup'})">Save NVS</button>
  </div>
  <div class="kv" style="margin-top:6px">common +5V (共陽極) loads energise on LOW -- set those outputs to "ON=LOW". Applies to firmware-driven pulses (ISR stages, sel_act, feeder) too.</div>
</div>

<div class="card"><h2>Selectors (air jets)</h2>
  pulse width <input id="seldelay" type="number" value="50"> ms
  <div style="margin-top:8px">
    <button onclick="cmd({type:'sel_act',idx:1,delay:+el('seldelay').value})">SEL1 (pin 25)</button>
    <button onclick="cmd({type:'sel_act',idx:2,delay:+el('seldelay').value})">SEL2 (pin 26)</button>
    <button onclick="cmd({type:'sel_act',idx:3,delay:+el('seldelay').value})">SEL3 (pin 32)</button>
  </div>
  <div class="kv" style="margin-top:6px">sel_act pulses the pin HIGH for the given ms in firmware.</div>
</div>

<div class="card"><h2>Gate sensor (pin 27)</h2>
  <div id="gate" class="gna">no data</div>
  <div class="kv">INPUT_PULLUP, inverted: beam blocked pulls LOW. edges seen: <b id="gedges">0</b></div>
  <div style="margin-top:8px">
    <button onclick="cmd({type:'pin_mode',pin:27,mode:'INPUT_PULLUP'})">Re-arm INPUT_PULLUP</button>
    <button onclick="firePhantom()">Phantom trigger</button>
  </div>
</div>

<div class="card"><h2>Live pins</h2><table id="pintab"><tr><th>pin</th><th>name</th><th>level</th></tr></table></div>

<div class="card"><h2>Gate pulse filter 光閘脈波過濾</h2>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:6px">
    <label class="kv">min width (ticks)<br><input id="pf_min" type="number" style="width:100%;box-sizing:border-box"></label>
    <label class="kv">max width (ticks)<br><input id="pf_max" type="number" style="width:100%;box-sizing:border-box"></label>
    <label class="kv">debounce rise (samples)<br><input id="pf_dr" type="number" style="width:100%;box-sizing:border-box"></label>
    <label class="kv">debounce fall (samples)<br><input id="pf_df" type="number" style="width:100%;box-sizing:border-box"></label>
    <label class="kv">min detect separation (&#181;s)<br><input id="pf_sep" type="number" style="width:100%;box-sizing:border-box"></label>
  </div>
  <div style="margin-top:8px">
    <button onclick="applyPf()">Apply</button>
    <button onclick="cmd({type:'save_setup'})">Save NVS</button>
    <span class="kv" id="pf_hint"></span>
  </div>
  <div class="kv" style="margin-top:6px">Accepts a gate pulse only if min &lt; width &lt; max. rise = 連續 N 樣本才算料件到(擋雜訊假件);fall = 連續 N 樣本才算離開(容忍料件中間縫隙);separation = 兩件最小時間間隔。</div>
</div>

<div class="card"><h2>Sorter status 分選機狀態</h2>
  <table>
    <tr><th>registered on plate</th><td id="ss_reg">-</td></tr>
    <tr><th>waiting for verdict</th><td id="ss_wait">-</td></tr>
    <tr><th>heading &#8594; SEL1 / SEL2 / SEL3 / NA / SKIP</th><td id="ss_head">-</td></tr>
    <tr><th>sorted so far (SEL1/SEL2/SEL3/NA)</th><td id="ss_done">-</td></tr>
    <tr><th>gate&#8594;report latency avg / max</th><td id="ss_lat">-</td></tr>
  </table>
  <div style="margin-top:8px"><button onclick="cmd({type:'reset_running_stat'})">Reset stats</button></div>
  <div class="kv" style="margin-top:6px">Latency counts from gate registration to the report arriving (camera shot is a fixed pulse offset after the gate). Needs the pipeline-stats firmware.</div>
</div>

<div class="card"><h2>Auto report 測試模式 (no camera)</h2>
  <div>
    M ok <input id="arm" type="number" value="1" style="width:55px">
    N ng <input id="arn" type="number" value="0" style="width:55px">
    O NA <input id="aro" type="number" value="0" style="width:55px">
  </div>
  <div style="margin-top:6px">
    ok&#8594;<select id="arok" style="width:90px"><option value="1" selected>cat1 SEL1</option><option value="2">cat2 SEL2</option><option value="3">cat3 SEL3</option></select>
    ng&#8594;<select id="arng" style="width:90px"><option value="1">cat1 SEL1</option><option value="2" selected>cat2 SEL2</option><option value="3">cat3 SEL3</option></select>
    NA&#8594;<select id="arna" style="width:90px"><option value="65535" selected>NA</option><option value="3">cat3 SEL3</option></select>
  </div>
  <div style="margin-top:8px">
    <button class="go" onclick="arSet(true)">Start</button>
    <button class="stop" onclick="arSet(false)">Stop</button>
    <b id="arstat" class="kv">off</b>
  </div>
  <div class="kv" style="margin-top:6px">Pattern repeats every M+N+O objects: 1/0/0 = all ok, 1/1/0 = ok/ng alternating. Panel answers each gate trigger (or Phantom trigger) with the next verdict -- needs READY mode.</div>
</div>

<div class="card"><h2>Station pulse offsets (stage_pulse_offset)</h2>
  <div id="sporows" style="display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:4px"></div>
  <div style="margin-top:8px">
    <button onclick="applySpo()">Apply</button>
    <button onclick="cmd({type:'save_setup'})">Save NVS</button>
    <span class="kv" id="spounit"></span>
  </div>
  <div style="margin-top:8px;border-top:1px solid #2c2c33;padding-top:8px">
    blow width <select id="selw" style="width:70px"><option>SEL1</option><option>SEL2</option><option>SEL3</option></select>
    <input id="selwms" type="number" value="100"> ms
    <button onclick="setSelWidth()">Set width &amp; apply</button>
    <span class="kv">ms &#8594; ticks 依目前 plate_freq 換算(_off = _on + ms&#215;2&#215;freq/1000)</span>
  </div>
  <div class="kv" style="margin-top:6px">Pulse position of each station, counted from the gate trigger. _on/_off = actuation window edges; SWITCH = report deadline.</div>
</div>

<div class="card"><h2>Machine metadata</h2>
  <div>
    pulses/rev <input id="ppr" type="number" value="2400">
    plate &#8960; <input id="pdia" type="number" value="0" step="0.1"> mm
  </div>
  <div style="margin-top:8px">
    <button onclick="applyMeta()">Apply</button>
    <button onclick="cmd({type:'save_setup'})">Save NVS</button>
    <span class="kv" id="convline"></span>
  </div>
  <div class="kv" style="margin-top:6px">Passive: FW never computes with these; hosts use them to convert mm/rpm &#8596; pulses. rpm = plate_freq / pulses_per_rev &#215; 60.</div>
</div>

<div class="card"><h2>Raw command</h2>
  <textarea id="raw">{"type":"get_setup"}</textarea>
  <button onclick="sendRaw()">Send</button>
  <button onclick="cmd({type:'ping'})">PING</button>
  <button onclick="cmd({type:'get_running_stat'})">Stat</button>
  <button onclick="cmd({type:'clear_error'})">Clear error</button>
</div>

<div class="card" style="grid-column:1/-1"><h2 style="display:flex;gap:8px;align-items:center">Log
  <button onclick="el('log').textContent=''">Clear</button>
  <button onclick="navigator.clipboard.writeText(el('log').textContent)">Copy</button>
  <label class="kv" style="font-weight:normal"><input type="checkbox" id="logscroll" checked> auto-scroll</label>
</h2><div id="log"></div></div>
</main>
<script>
const el=id=>document.getElementById(id);
const PINS={27:"GATE",21:"FEEDER",25:"SEL1",26:"SEL2",32:"SEL3",13:"STEP EN",16:"L1A",17:"CAM1",18:"L2A",19:"CAM2"};
let gateLast=null,gateEdges=0,meta={ppr:0,dia:0};
const OUTS=["L1A","CAM1","L2A","CAM2","SEL1","SEL2","SEL3","FEEDER"];
let arOn=false;
async function firePhantom(){
  // An unanswered object stops the line by design -- never fire a phantom
  // without a reporter running.
  if(!arOn){log("autorep","reporter was off -- auto-starting it first");await arSet(true);}
  cmd({type:'trig_phantom_pulse'},true);
}
let ioOn={};  // name -> 1 (ON=HIGH) / 0 (ON=LOW), from get_setup io_on_level
function setOut(pin,on){
  const lvl=ioOn[PINS[pin]] ?? 1;
  const phys=on ? lvl : 1-lvl;
  return cmd(phys ? {type:'pin_on',pin} : {type:'pin_off',pin});
}
function paintPol(){
  el('polrows').innerHTML=OUTS.map(n=>
    `<label class="kv">${n} ON=<select id="pol_${n}" style="width:80px">`+
    `<option value="1"${(ioOn[n]??1)==1?' selected':''}>HIGH</option>`+
    `<option value="0"${(ioOn[n]??1)==0?' selected':''}>LOW</option></select></label>`).join('');
}
const SPO_KEYS=["L1A_on","L1A_off","CAM1_on","CAM1_off","L2A_on","L2A_off","CAM2_on","CAM2_off","SWITCH","SEL1_on","SEL1_off","SEL2_on","SEL2_off","SEL3_on","SEL3_off"];
function paintSpo(spo){
  el('sporows').innerHTML=SPO_KEYS.map(k=>
    `<label class="kv">${k}<br><input id="spo_${k}" type="number" style="width:100%;box-sizing:border-box" value="${spo?.[k]??''}"></label>`).join('');
}
async function applySpo(){
  const spo={};SPO_KEYS.forEach(k=>{const v=el('spo_'+k).value;if(v!=='')spo[k]=+v;});
  await cmd({type:'set_setup',stage_pulse_offset:spo});
}
let lastFreq=0;
function pfHint(){
  if(!(meta.ppr&&meta.dia>0))return;
  const mm=t=>(t/(2*meta.ppr)*Math.PI*meta.dia).toFixed(2);
  el('pf_hint').textContent=`= ${mm(+el('pf_min').value||0)} ~ ${mm(+el('pf_max').value||0)} mm at rim`;
}
async function applyPf(){
  await cmd({type:'set_setup',
    pulse_min_width:+el('pf_min').value, pulse_max_width:+el('pf_max').value,
    gate_debounce_rise:+el('pf_dr').value, gate_debounce_fall:+el('pf_df').value,
    min_detect_sep_us:+el('pf_sep').value});
  pfHint();
}
async function setSelWidth(){
  const f=lastFreq||+el('freq').value;
  if(!f){log("err","plate_freq unknown/zero -- set it first");return;}
  const n=el('selw').value, ms=+el('selwms').value;
  const on=+el('spo_'+n+'_on').value;
  const ticks=Math.max(1,Math.round(ms*2*f/1000));
  el('spo_'+n+'_off').value=on+ticks;
  log("width",n+": "+ms+"ms = "+ticks+" ticks @ freq "+f);
  await applySpo();
}
async function applyPol(){
  const lv={};OUTS.forEach(n=>lv[n]=+el('pol_'+n).value);
  await cmd({type:'set_setup',io_on_level:lv});
  ioOn=lv;
}
function fmtSpeed(freq){
  if(!meta.ppr||!freq)return '';
  const rpm=freq/meta.ppr*60;
  let s=' = '+rpm.toFixed(2)+' rpm';
  if(meta.dia>0)s+=' = '+(Math.PI*meta.dia*rpm/60).toFixed(1)+' mm/s rim';
  return s;
}
async function arSet(on){
  const body={on,m:+el('arm').value,n:+el('arn').value,o:+el('aro').value,
    cat_ok:+el('arok').value,cat_ng:+el('arng').value,cat_na:+el('arna').value};
  const r=await fetch('/autoreport',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(body)});
  const j=await r.json();
  log("autorep",j);
  arOn=!!(j&&j.autorep);
  if(!arOn)el('arstat').textContent='off';
}
async function applyMeta(){
  await cmd({type:'set_setup',pulses_per_rev:+el('ppr').value,plate_diameter_mm:+el('pdia').value});
  meta.ppr=+el('ppr').value;meta.dia=+el('pdia').value;
}
function log(tag,obj){
  const d=el('log');
  d.textContent+=new Date().toTimeString().slice(0,8)+" ["+tag+"] "+
    (typeof obj==='string'?obj:JSON.stringify(obj))+"\n";
  if(d.textContent.length>60000)d.textContent=d.textContent.slice(-40000);
  if(el('logscroll').checked)d.scrollTop=d.scrollHeight;
}
async function cmd(c,nowait){
  log("tx",c);
  try{
    const r=await fetch('/cmd',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({cmd:c,nowait:!!nowait})});
    const j=await r.json();
    log("rx",j.reply??j);
    return j.reply;
  }catch(e){log("err",String(e));}
}
async function pulsePin(pin,ms){
  await setOut(pin,true);
  setTimeout(()=>setOut(pin,false),Math.max(10,ms));
}
function sendRaw(){
  try{cmd(JSON.parse(el('raw').value));}
  catch(e){log("err","bad JSON: "+e);}
}
function paintPins(pins,vals){
  const t=el('pintab');
  t.innerHTML="<tr><th>pin</th><th>name</th><th>level</th></tr>";
  pins.forEach((p,i)=>{
    const v=vals[i];
    const n=PINS[p]||'';
    const logi=(n in ioOn)?(v===ioOn[n]?' · ON':' · off'):'';
    t.innerHTML+=`<tr><td>${p}</td><td>${n}</td>`+
      `<td class="${v?'hi':'lo'}">${v?'HIGH':'LOW'}${logi}</td></tr>`;
    if(p===27){
      if(gateLast!==null&&gateLast!==v)gateEdges++;
      gateLast=v;
      el('gedges').textContent=gateEdges;
      const g=el('gate');
      if(v){g.className='gclear';g.textContent='CLEAR (HIGH)';}
      else{g.className='gblock';g.textContent='BLOCKED (LOW)';}
    }
  });
}
const es=new EventSource('/events');
es.onmessage=e=>{
  const m=JSON.parse(e.data);
  if(m.panel==='pins')paintPins(m.pins,m.vals);
  else if(m.panel==='stat'){
    el('linkdot').className='dot up';
    el('stateline').textContent='state: '+m.state_name+' ('+m.state+')';
    lastFreq=m.plate_freq||lastFreq;
    el('freqline').textContent='plate_freq: '+m.plate_freq+fmtSpeed(m.plate_freq);
    el('reconline').textContent=m.reconnects?('reconnects: '+m.reconnects):'';
    if(m.count)el('reconline').textContent+='  sorted: '+JSON.stringify(m.count);
    arOn=!!m.autorep;
    if(m.autorep)el('arstat').textContent='reported ok:'+m.autorep.ok+' ng:'+m.autorep.ng+' NA:'+m.autorep.na;
    else el('arstat').textContent='off';
    el('errbanner').style.display=(m.state===112)?'':'none';
    if(m.pipe){
      el('ss_reg').textContent=m.pipe.registered;
      el('ss_wait').textContent=m.pipe.waiting;
      const h=m.pipe.heading||{};
      el('ss_head').textContent=`${h.SEL1??0} / ${h.SEL2??0} / ${h.SEL3??0} / ${h.NA??0} / ${h.SKIP??0}`;
    }
    if(m.count)el('ss_done').textContent=`${m.count.SEL1??0} / ${m.count.SEL2??0} / ${m.count.SEL3??0} / ${m.count.NA??0}`;
    if(m.report_latency){
      const L=m.report_latency;
      el('ss_lat').textContent=L.n?`${(L.avg_us/1000).toFixed(1)} ms / ${(L.max_us/1000).toFixed(1)} ms  (n=${L.n})`:'no reports yet';
    }
  }
  else if(m.panel==='link'){
    el('linkdot').className='dot '+(m.up?'up':'down');
    if(!m.up)el('stateline').textContent='state: link down';
  }
  else if(m.panel==='pins_unsupported'){el('pinwarn').style.display='';el('gate').className='gna';el('gate').textContent='PIN_READ n/a';}
  else if(m.panel==='diag')log("diag",m.text);
  else if(m.panel==='async')log("dev",m.msg);
};
es.onerror=()=>{el('linkdot').className='dot down';};
paintPol();paintSpo(null);  // defaults until the board answers
// Reflect the board's current stepper polarity in the selectors on load.
cmd({type:'get_setup'}).then(s=>{
  if(!s)return;
  if('stepper_en_active' in s)el('enact').value=String(s.stepper_en_active);
  if('stepper_dir' in s)el('dirlvl').value=String(s.stepper_dir);
  if(s.plate_freq){el('freq').value=s.plate_freq;el('freqslide').value=s.plate_freq;}
  if('plate_accel' in s)el('accel').value=s.plate_accel;
  if(s.pulses_per_rev){meta.ppr=s.pulses_per_rev;el('ppr').value=s.pulses_per_rev;}
  if(s.plate_diameter_mm){meta.dia=s.plate_diameter_mm;el('pdia').value=s.plate_diameter_mm;}
  if(s.io_on_level)ioOn=s.io_on_level;
  if('pulse_min_width' in s){
    el('pf_min').value=s.pulse_min_width; el('pf_max').value=s.pulse_max_width;
    el('pf_dr').value=s.gate_debounce_rise; el('pf_df').value=s.gate_debounce_fall;
    el('pf_sep').value=s.min_detect_sep_us;
    pfHint();
  }
  paintPol();
  paintSpo(s.stage_pulse_offset);
  if(meta.ppr&&meta.dia>0)
    el('spounit').textContent='1 pulse-count ≈ '+(Math.PI*meta.dia/(2*meta.ppr)).toFixed(4)+' mm at rim';
});
</script></body></html>
"""


def main():
    global PANEL
    ap = argparse.ArgumentParser(description="uInspESP32 hardware bring-up panel")
    ap.add_argument("--port", required=True, help="serial port, e.g. /dev/cu.usbserial-0001")
    ap.add_argument("--http-port", type=int, default=8765)
    args = ap.parse_args()

    PANEL = Panel(args.port)
    srv = ThreadingHTTPServer(("127.0.0.1", args.http_port), Handler)
    print("panel:  http://127.0.0.1:%d   (serial %s)" % (args.http_port, args.port))
    print("ctrl-c to quit")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
