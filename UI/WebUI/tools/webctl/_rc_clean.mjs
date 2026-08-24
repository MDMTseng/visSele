// Force the station's clean regions dirty (and put them back) at RUNTIME.
//
// WHY A SEPARATE, DELIBERATELY MINIMAL CLIENT
//
// The question this serves is "does the WebUI still get the image, and no
// dimensions, when the clean area fails". Whatever connects to the core FIRST
// gets the image stream, so a probe that opens a CI session or subscribes with
// SB would take the stream off the browser and answer its own question wrongly.
// This sends ST commands and nothing else -- no CI, no SB, no stream.
//
// THE CONNECTION IS OPENED EARLY AND HELD. Connecting late -- after the browser
// is already attached and the machine is running -- was measured to fail: the
// ST never reached the core at all (dark_ratio never moved) and the ack timed
// out at 8s. Opened before the browser and kept alive answering HR, the same
// command works every time. So this is a long-lived peer, not a one-shot.
//
// THE COMMAND IS "ST", NOT "RC". MachineSetting is handled inside
// checkTL("ST") (wiringPanel.cpp:5595). The RC branch's targets are camera and
// calibration files only -- send MachineSetting there and RC answers ACK:true
// having ignored the key entirely, which is a green light for a change that
// never happened. Cost one full bench run to notice.
//
// ACK:false IS THE NORMAL ANSWER HERE. session_ACK is only set by keys like
// DoImageTransfer and InspAreaBypass; a MachineSetting-only ST leaves it false.
// Do not read it as failure -- verify the change in the DATA (dark_ratio), which
// is the only thing that actually proves the new thresholds are live.
//
// A bare ST also resets ImageCropX/Y/W/H to the whole frame unconditionally
// (5629). Harmless while downSampLevel==1, because that path sends capImg
// untouched and never reads the crop -- but it is a real side effect.
//
// setup_machine_setting() reloads inspection_region AND clean_regions, and
// treats an ABSENT key as "none configured" (wiringPanel.cpp:5874). Sending
// clean_regions alone would silently wipe the station box. Both keys, always.
//
// Runtime only: machine_setting.json on disk is never written, so a crash or a
// restart restores the real configuration by itself.
import WebSocket from 'ws';

const BPG_HDR = 9;
const enc = new TextEncoder();
function frame(t, p, g, o) {
  const b = enc.encode(o == null ? '' : JSON.stringify(o));
  const u = new Uint8Array(BPG_HDR + b.length + 1);
  u[0] = t.charCodeAt(0); u[1] = t.charCodeAt(1); u[2] = p;
  u[3] = g >> 8; u[4] = g & 255;
  const l = u.length - BPG_HDR;
  u[5] = l >>> 24; u[6] = (l >> 16) & 255; u[7] = (l >> 8) & 255; u[8] = l & 255;
  u.set(b, BPG_HDR);
  return u;
}

export async function openSettingLink(url = 'ws://127.0.0.1:4090') {
  const ws = new WebSocket(url);
  ws.binaryType = 'arraybuffer';
  let pg = 1;
  let waiter = null;
  ws.on('message', (d) => {
    if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
    const b = new Uint8Array(d);
    const t = String.fromCharCode(b[0], b[1]);
    // Answer the heartbeat for the whole life of the link, or the core drops
    // this peer between the open and the command that matters.
    if (t === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
    if (t !== 'SS' || !waiter) return;
    const s = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0+$/, '');
    if (s.includes('"cmd":"ST"')) { const w = waiter; waiter = null; w.resolve(s); }
  });
  await new Promise((res, rej) => {
    ws.once('open', res);
    ws.once('error', rej);
  });
  return {
    send(mset, timeoutMs = 15000) {
      return new Promise((resolve, reject) => {
        const timer = setTimeout(() => { waiter = null; reject(new Error('no ST ack in ' + timeoutMs + 'ms')); }, timeoutMs);
        waiter = { resolve: (v) => { clearTimeout(timer); resolve(v); } };
        ws.send(frame('ST', 0, pg++, { MachineSetting: mset }));
      });
    },
    close() { try { ws.close(); } catch {} },
  };
}

// 255, not "near white". eval_clean_regions uses THRESH_BINARY_INV, so a pixel
// counts as dark when NOT (src > thresh). At 255 nothing can exceed it, so the
// whole region is dark, ratio is 1.0 and the region trips on every frame no
// matter what is under it.
//
// 250 does NOT work here and the reason matters: this is a BACKLIT station, so
// the bright field is saturated at 255, and 255 > 250 leaves dark_ratio at
// exactly 0. A threshold picked as "nearly white" quietly measures nothing.
export const dirtied = (mset, thresh = 255) => ({
  ...mset,
  clean_regions: (mset.clean_regions || []).map((c) => ({ ...c, dark_thresh: thresh })),
});
