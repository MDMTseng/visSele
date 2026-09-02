// Set the edge-strength floor by looking at the edges, not by typing a number.
//
// edge.min_strength is in raw gradient units. Nothing on screen has ever said
// what those units are worth on THIS primitive, under THIS lighting, so the
// field has been filled with whatever the default was: the WebUI seeds 10, the
// defs in the field carry 30, one search point carries 0 -- against real edges
// that measure ~110 through a caliper. The floor was effectively off.
//
// The core sends the whole across-edge gradient for every caliper on the
// primitive (extra.edge_profile), ungated: the peaks BELOW the current setting
// are in it too. That is the point. A threshold can only be lowered on evidence
// that exists below it, and what decides where it belongs is the gap between
// the noise and the real edge -- which is visible here and nowhere else.
//
// Everything the slider does is computed from that payload in the browser, so
// dragging it is instant and costs the machine nothing. One 檢查 per look.
import React, { useMemo, useRef, useState } from 'react';

// A peak is a local maximum of the signed gradient on the side the polarity
// selects -- the same rule edge_select applies, so what is counted here is what
// the core would pick, not an approximation of it.
function peaksOf(g, polarity) {
  const out = [];
  for (let i = 1; i < g.length - 1; i++) {
    const v = polarity === 'rising' ? g[i]
            : polarity === 'falling' ? -g[i]
            : Math.abs(g[i]);
    const a = polarity === 'rising' ? g[i - 1]
            : polarity === 'falling' ? -g[i - 1]
            : Math.abs(g[i - 1]);
    const b = polarity === 'rising' ? g[i + 1]
            : polarity === 'falling' ? -g[i + 1]
            : Math.abs(g[i + 1]);
    if (v > 0 && v >= a && v >= b) out.push({ i, v });
  }
  return out;
}

export function EdgeProfileView({ profile, minStrength, polarity = 'falling',
                                  onChange, onProbe, busy, note }) {
  const [hover, setHover] = useState(null);
  // The dragged value lives here as well as on the shape.
  //
  // Committing upward is the point of the control, but the commit is a round
  // trip through the canvas and the store, and a slider that waits for that
  // before it redraws feels broken while being dragged. So the drag updates
  // locally and immediately -- the curve recolours and the caliper count moves
  // under the thumb -- and the same value goes up. `pending` is cleared
  // whenever the shape's own value arrives, so the two cannot drift: what is
  // shown is either what the shape says or what is on its way there.
  const [pending, setPending] = useState(null);
  // Set while a drag has moved the value and not yet been acted on. A drag
  // fires onChange continuously; only its END is worth an inspection.
  const dirty = useRef(false);
  const committed = Math.max(0, Number(minStrength) || 0);
  const [seen, setSeen] = useState(committed);
  if (committed !== seen) { setSeen(committed); setPending(null); }
  const thr = pending == null ? committed : pending;
  const W = 320, H = 132, PADL = 30, PADB = 16, PADT = 8;

  const model = useMemo(() => {
    if (!profile || !profile.g || !profile.g.length) return null;
    const g = profile.g;
    // One scale for every caliper: comparing them is most of the value, and a
    // per-caliper autoscale would make a weak edge look like a strong one.
    let peak = 0;
    for (const one of g) for (const v of one) if (Math.abs(v) > peak) peak = Math.abs(v);
    const n = g[0].length;
    // Per caliper: the strongest peak of the selected polarity -- the one the
    // core would pick -- and the strongest of the REST, which is what a floor
    // set too low would let win instead.
    const best = [], runner = [];
    for (const one of g) {
      const p = peaksOf(one, polarity).map((q) => q.v).sort((a, b) => b - a);
      best.push(p.length ? p[0] : 0);
      runner.push(p.length > 1 ? p[1] : 0);
    }
    // WHERE THE FLOOR BELONGS, from the two numbers that bound it.
    //
    //   signal = the WEAKEST edge the calipers actually found. Go above this
    //            and a caliper stops finding its edge -- the floor may not.
    //   noise  = the STRONGEST competing peak anywhere in the windows. Stay
    //            below this and that peak can be picked instead, which is not a
    //            missing measurement but a wrong one.
    //
    // The gap between them is the whole reason this panel exists, and the
    // suggestion is the geometric mean of the two: scale-free, so it does not
    // drift when the lighting or the lens changes the units, and it sits in
    // proportion rather than at a fixed offset from either side.
    //
    // No gap means the answer is not "pick better" -- there is no threshold
    // that separates them, and the honest suggestion is a floor that keeps
    // every caliper working while the panel says the separation is poor.
    const live = best.filter((b) => b > 0);
    const signal = live.length ? Math.min(...live) : 0;
    const noise = runner.length ? Math.max(...runner) : 0;
    const clean = signal > 0 && signal > noise * 1.25;
    let suggest = clean
      ? Math.sqrt(signal * Math.max(noise, 1))
      : signal * 0.5;
    // Never above the weakest real edge: a suggestion that drops a caliper the
    // moment it is applied is not a suggestion.
    if (signal > 0) suggest = Math.min(suggest, signal * 0.85);
    suggest = Math.max(0, Math.round(suggest));
    return { g, peak: peak || 1, n, best, signal, noise, clean, suggest };
  }, [profile, polarity]);

  if (!model) {
    return <div style={{ padding: '6px 0' }}>
      <button type="button" onClick={onProbe} disabled={busy}
        data-testid="edge-profile-check"
        style={{ padding: '4px 12px', cursor: busy ? 'default' : 'pointer' }}>
        {busy ? '檢查中…' : '檢查邊緣強度'}
      </button>
      {note && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 4 }}>{note}</div>}
      <div style={{ fontSize: 11.5, opacity: 0.65, marginTop: 4 }}>
        跑一張影像，把每個 caliper 實際量到的邊緣梯度畫出來，門檻就照著圖設。
      </div>
    </div>;
  }

  const { g, peak, n, best, signal, noise, clean, suggest } = model;
  const x = (i) => PADL + (i / (n - 1)) * (W - PADL - 6);
  const y = (v) => PADT + (1 - v / peak) * (H - PADT - PADB);   // 0 at the bottom
  const pass = best.filter((b) => b >= thr).length;
  const dragEnd = () => {
    if (!dirty.current || busy) return;
    dirty.current = false;
    if (onProbe) onProbe();
  };
  // The headroom the setting has: how far the floor could move before it starts
  // dropping calipers that currently find their edge.
  const weakest = best.length ? Math.min(...best.filter((b) => b > 0)) : 0;

  return <div style={{ padding: '4px 0' }}>
    <svg width={W} height={H} style={{ background: 'rgba(127,127,127,.06)', borderRadius: 4 }}
         data-testid="edge-profile-plot" data-calipers={g.length} data-pass={pass}>
      {/* the dead zone: anything in here is not an edge at the current floor */}
      <rect x={PADL} y={y(thr)} width={W - PADL - 6} height={Math.max(0, y(0) - y(thr))}
            fill="rgba(255,120,117,.10)" />
      <line x1={PADL} x2={W - 6} y1={y(thr)} y2={y(thr)}
            stroke="#ff7875" strokeWidth="1.5" strokeDasharray="4 3" />
      {g.map((one, k) => {
        const d = one.map((v, i) => {
          const s = polarity === 'rising' ? v : polarity === 'falling' ? -v : Math.abs(v);
          return (i ? 'L' : 'M') + x(i).toFixed(1) + ' ' + y(Math.max(0, s)).toFixed(1);
        }).join(' ');
        const on = best[k] >= thr;
        return <path key={k} d={d} fill="none" strokeWidth={hover === k ? 2 : 1}
                     stroke={on ? 'rgba(82,196,26,.75)' : 'rgba(255,120,117,.75)'}
                     onMouseEnter={() => setHover(k)} onMouseLeave={() => setHover(null)} />;
      })}
      <line x1={PADL} x2={W - 6} y1={y(0)} y2={y(0)} stroke="rgba(127,127,127,.5)" />
      <text x={4} y={y(thr) + 4} fontSize="10" fill="#ff7875">{Math.round(thr)}</text>
      <text x={4} y={y(peak) + 9} fontSize="10" fill="rgba(127,127,127,.8)">{Math.round(peak)}</text>
      <text x={PADL} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">
        −{profile.L.toFixed(0)}px
      </text>
      <text x={W - 34} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">
        +{profile.L.toFixed(0)}px
      </text>
    </svg>

    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 6 }}>
      <input type="range" min={0} max={Math.ceil(peak * 1.1)} step={1} value={thr}
             data-testid="edge-profile-slider"
             onChange={(e) => {
               const v = Number(e.target.value);
               setPending(v); onChange(v); dirty.current = true;
             }}
             // LET GO AND SEE WHAT IT DID.
             //
             // The recolouring under the thumb is arithmetic on the payload in
             // hand -- it says which peaks pass, and nothing more. What the
             // threshold actually changes is which edge each caliper PICKS, and
             // therefore where the hits sit and where the line fits, and none of
             // that is derivable here. So the end of a drag runs one inspection
             // and the canvas snaps to the answer.
             //
             // At the END, not during: a drag emits a change per pixel of
             // travel, and an inspection per pixel would make the control
             // unusable and the machine busy. mouseup covers the mouse,
             // touchend and pointerup the rest, and mouseleave the drag that
             // ends outside the control -- all four collapse to one because
             // `dirty` is cleared by the first that fires.
             onMouseUp={dragEnd} onTouchEnd={dragEnd}
             onPointerUp={dragEnd} onMouseLeave={dragEnd}
             onKeyUp={dragEnd}
             style={{ flex: 1 }} />
      <span style={{ fontVariantNumeric: 'tabular-nums', minWidth: 34, textAlign: 'right' }}>
        {Math.round(thr)}
      </span>
    </div>

    {/* Says what the setting DOES, in calipers, because that is the thing that
        goes NA -- a fit needs min_inliers of them, not a good-looking graph. */}
    <div style={{ fontSize: 11.5, marginTop: 4,
                  color: pass === g.length ? 'inherit' : '#ff7875' }}>
      {pass}/{g.length} 個 caliper 在這個門檻下找得到邊
      {pass === g.length && weakest > 0 &&
        <span style={{ opacity: 0.65 }}>　（最弱的一個是 {Math.round(weakest)}）</span>}
    </div>
    <div style={{ fontSize: 11.5, opacity: 0.65, marginTop: 2 }}>
      綠線是會被採用的 caliper，紅線是被門檻濾掉的。粉紅區域以下都不算邊。
    </div>
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 6 }}>
      <button type="button" data-testid="edge-profile-auto"
        data-suggest={suggest} data-clean={clean ? '1' : '0'}
        onClick={() => { setPending(suggest); onChange(suggest); dirty.current = true; dragEnd(); }}
        style={{ padding: '3px 10px', cursor: 'pointer' }}>
        自動設定 {suggest}
      </button>
      <span style={{ fontSize: 11.5, opacity: 0.65 }}>
        最弱的邊 {Math.round(signal)}、最強的雜訊峰 {Math.round(noise)}
      </span>
    </div>
    {!clean && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 2 }}>
      雜訊峰和真實邊沒有分開，沒有一個門檻分得掉它們。建議值只保證每個 caliper
      還找得到邊，不保證找到的是對的那條 —— 先看看 caliper 位置或寬度。
    </div>}
    <button type="button" onClick={onProbe} disabled={busy}
      data-testid="edge-profile-recheck"
      style={{ marginTop: 6, padding: '3px 10px', cursor: busy ? 'default' : 'pointer' }}>
      {busy ? '檢查中…' : '重新檢查'}
    </button>
    {note && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 4 }}>{note}</div>}
  </div>;
}
