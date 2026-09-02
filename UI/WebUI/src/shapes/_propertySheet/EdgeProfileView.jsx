// Set the edge-strength floor by looking at the edges, not by typing a number.
//
// edge.min_strength is in raw gradient units. Nothing on screen has ever said
// what those units are worth on THIS primitive, under THIS lighting, so the
// field has been filled with whatever the default was: the WebUI seeds 10, the
// defs in the field carry 30, one search point carries 0 -- against real edges
// measuring ~110 through a caliper and ~420 through a search point, because the
// two use different gradient operators. One field name, two scales, and no way
// to see either.
//
// The core sends what the selector actually saw, ungated: the peaks BELOW the
// current setting are in it too. That is the point. A threshold can only be
// lowered onto evidence that exists below it, and what decides where it belongs
// is the gap between the noise and the real edge -- visible here and nowhere
// else. Ungated also means the slider is arithmetic in the browser: dragging is
// instant and costs the machine nothing.
//
// TWO PLOTS, because there are two selectors.
//
//   caliper (line/arc)  averages along the edge and picks a peak out of one
//                       across-edge profile. The evidence is that curve, and
//                       the question is which calipers still find an edge.
//   search point        finds a peak per row and takes the one NEAREST the
//                       origin. The evidence is the candidate set, and the
//                       question is which candidate ends up nearest -- so the
//                       axis is distance along the search, not across the edge.
//
// Same slider, same commit, same 自動設定; different picture, and a different
// sentence underneath it.
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

const W = 320, H = 132, PADL = 30, PADB = 16, PADT = 8;

export function EdgeProfileView({ profile, minStrength, polarity = 'falling',
                                  onChange, onProbe, busy, note }) {
  const [hover, setHover] = useState(null);
  // The dragged value lives here as well as on the shape.
  //
  // Committing upward is the point of the control, but committing changes the
  // shape, and a shape change drops the inspection report -- so writing on
  // every change made the hits on the canvas vanish the moment the thumb moved,
  // taking away the reference the operator was comparing against. Losing the
  // "before" is losing the point of a slider. So the value stays local until
  // the drag ends, and `pending` is cleared when the shape's own value arrives:
  // what is shown is either what the shape says or what is on its way there.
  const [pending, setPending] = useState(null);
  const committed = Math.max(0, Number(minStrength) || 0);
  const [seen, setSeen] = useState(committed);
  if (committed !== seen) { setSeen(committed); setPending(null); }
  const thr = pending == null ? committed : pending;

  // Set while a drag has moved the value and not yet been acted on. A drag
  // fires onChange continuously; only its END is worth an inspection.
  const dirty = useRef(false);
  // The dragged value, in a ref as well as in state. dragEnd runs from an event
  // handler that may be in the SAME tick as the setState that produced the
  // value -- reading it from the render closure would commit the previous one.
  const latest = useRef(null);

  // LET GO AND SEE WHAT IT DID.
  //
  // The recolouring under the thumb is arithmetic on the payload in hand: it
  // says which peaks clear the floor, and nothing more. What the threshold
  // actually changes is which edge gets PICKED -- and so where the hits sit and
  // where the fit lands -- and none of that is derivable here. So the end of a
  // drag commits once and runs one inspection, and the canvas snaps to it.
  //
  // At the END, not during: a drag emits a change per pixel of travel. mouseup,
  // touchend, pointerup, mouseleave and keyup all funnel here and collapse to a
  // single run because `dirty` is cleared by whichever fires first -- mouseleave
  // is there for the drag that ends outside the control.
  const dragEnd = () => {
    if (!dirty.current || busy) return;
    dirty.current = false;
    const v = latest.current;
    if (v != null && v !== committed) onChange(v);
    if (onProbe) onProbe();
  };

  // SEARCH POINT: candidates by distance along the search, strength up.
  const peaks = useMemo(() => {
    if (!profile || profile.kind !== 'peaks' || !profile.p || !profile.p.length) return null;
    const pts = profile.p.map((pos, i) => ({ pos, str: profile.s[i] }));
    let peak = 0;
    for (const q of pts) if (q.str > peak) peak = q.str;
    // WHAT THE CORE ALREADY DOES, WRITTEN DOWN.
    //
    // search_point_cv keeps candidates within 0.40 of the strongest anywhere in
    // the window and takes the nearest survivor. That rule is invisible, is
    // absolute nowhere, and moves whenever something stronger enters the
    // window -- a neighbouring part or a burr raises the bar and the measured
    // point can jump to another edge with nothing said. Measured on test1:
    // three of nine search points are held on their edge by that rule alone,
    // one of them 13.8px (192um) from the nearest candidate.
    //
    // So the suggestion is that same number as a fixed floor. Applying it
    // changes nothing today and makes the rule visible, which is what has to
    // happen before it can stop being a hidden one.
    return { pts, peak: peak || 1, span: profile.span || 1,
             suggest: Math.round(0.40 * peak) };
  }, [profile]);

  // CALIPER (line/arc): every caliper's across-edge gradient, on one scale.
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
    // The suggestion is the geometric mean: scale-free, so it does not drift
    // when the lighting or the lens changes the units, and it sits in
    // proportion rather than at a fixed offset from either side.
    const live = best.filter((b) => b > 0);
    const signal = live.length ? Math.min(...live) : 0;
    const noise = runner.length ? Math.max(...runner) : 0;
    const clean = signal > 0 && signal > noise * 1.25;
    let suggest = clean ? Math.sqrt(signal * Math.max(noise, 1)) : signal * 0.5;
    // Never above the weakest real edge: a suggestion that drops a caliper the
    // moment it is applied is not a suggestion.
    if (signal > 0) suggest = Math.min(suggest, signal * 0.85);
    return { g, peak: peak || 1, n, best, signal, noise, clean,
             suggest: Math.max(0, Math.round(suggest)) };
  }, [profile, polarity]);

  // The controls are identical for both plots, so they are written once.
  const controls = (maxV, suggestV, footer) => <>
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 6 }}>
      <input type="range" min={0} max={maxV} step={1} value={thr}
             data-testid="edge-profile-slider"
             onChange={(e) => {
               const v = Number(e.target.value);
               latest.current = v; setPending(v); dirty.current = true;
             }}
             onMouseUp={dragEnd} onTouchEnd={dragEnd}
             onPointerUp={dragEnd} onMouseLeave={dragEnd} onKeyUp={dragEnd}
             style={{ flex: 1 }} />
      <span style={{ fontVariantNumeric: 'tabular-nums', minWidth: 34, textAlign: 'right' }}>
        {Math.round(thr)}
      </span>
    </div>
    {footer}
    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 6 }}>
      <button type="button" data-testid="edge-profile-auto" data-suggest={suggestV}
        onClick={() => { latest.current = suggestV; setPending(suggestV);
                         dirty.current = true; dragEnd(); }}
        style={{ padding: '3px 10px', cursor: 'pointer' }}>
        自動設定 {suggestV}
      </button>
      <button type="button" onClick={onProbe} disabled={busy}
        data-testid="edge-profile-recheck"
        style={{ padding: '3px 10px', cursor: busy ? 'default' : 'pointer' }}>
        {busy ? '檢查中…' : '重新檢查'}
      </button>
    </div>
    {note && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 4 }}>{note}</div>}
  </>;

  if (!model && !peaks) {
    return <div style={{ padding: '6px 0' }}>
      <button type="button" onClick={onProbe} disabled={busy}
        data-testid="edge-profile-check"
        style={{ padding: '4px 12px', cursor: busy ? 'default' : 'pointer' }}>
        {busy ? '檢查中…' : '檢查邊緣強度'}
      </button>
      {note && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 4 }}>{note}</div>}
      <div style={{ fontSize: 11.5, opacity: 0.65, marginTop: 4 }}>
        跑一張影像，把實際量到的邊緣梯度畫出來，門檻就照著圖設。
      </div>
    </div>;
  }

  if (peaks) {
    const { pts, peak, span, suggest } = peaks;
    const px = (d) => PADL + (d / span) * (W - PADL - 6);
    const py = (v) => PADT + (1 - v / peak) * (H - PADT - PADB);
    const over = pts.filter((q) => q.str >= thr);
    // The answer: the nearest candidate that clears the floor. Everything
    // nearer and weaker is what the floor is holding back, which is the only
    // reason to look at this plot.
    const chosen = over.length ? over.reduce((a, b) => (b.pos < a.pos ? b : a)) : null;
    const held = chosen ? pts.filter((q) => q.pos < chosen.pos).length : 0;
    return <div style={{ padding: '4px 0' }}>
      <svg width={W} height={H} style={{ background: 'rgba(127,127,127,.06)', borderRadius: 4 }}
           data-testid="edge-profile-plot" data-kind="peaks"
           data-cands={pts.length} data-pass={over.length}
           data-first={chosen ? chosen.pos.toFixed(1) : ''}>
        <rect x={PADL} y={py(thr)} width={W - PADL - 6} height={Math.max(0, py(0) - py(thr))}
              fill="rgba(255,120,117,.10)" />
        <line x1={PADL} x2={W - 6} y1={py(thr)} y2={py(thr)}
              stroke="#ff7875" strokeWidth="1.5" strokeDasharray="4 3" />
        {chosen && <line x1={px(chosen.pos)} x2={px(chosen.pos)} y1={PADT} y2={py(0)}
                         stroke="#1890ff" strokeWidth="1" strokeDasharray="3 3" />}
        {pts.map((q, k) => {
          const isFirst = chosen && q === chosen;
          return <circle key={k} cx={px(q.pos)} cy={py(q.str)} r={isFirst ? 4 : 2.2}
                   fill={isFirst ? '#1890ff'
                       : q.str >= thr ? 'rgba(82,196,26,.75)' : 'rgba(255,120,117,.6)'} />;
        })}
        <line x1={PADL} x2={W - 6} y1={py(0)} y2={py(0)} stroke="rgba(127,127,127,.5)" />
        <text x={4} y={py(thr) + 4} fontSize="10" fill="#ff7875">{Math.round(thr)}</text>
        <text x={4} y={py(peak) + 9} fontSize="10" fill="rgba(127,127,127,.8)">{Math.round(peak)}</text>
        <text x={PADL} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">近 0px</text>
        <text x={W - 46} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">遠 {span.toFixed(0)}px</text>
      </svg>
      <div style={{ fontSize: 11.5, marginTop: 4, color: chosen ? 'inherit' : '#ff7875' }}>
        {chosen
          ? <>首擊在 {chosen.pos.toFixed(1)}px，強度 {Math.round(chosen.str)}
              {held > 0 && <span style={{ color: '#ff7875' }}>
                　門檻擋下了 {held} 個更近的候選</span>}</>
          : '這個門檻下沒有任何候選，這個 search point 會是 NA'}
      </div>
      {controls(Math.ceil(peak * 1.1), suggest,
        <div style={{ fontSize: 11.5, opacity: 0.65, marginTop: 4 }}>
          藍點是被採用的首擊，綠點過了門檻但更遠，紅點被門檻擋下。
          建議值是核心目前內部規則（最強候選的 40%）寫成的固定門檻。
        </div>)}
    </div>;
  }

  const { g, peak, n, best, signal, noise, clean, suggest } = model;
  const x = (i) => PADL + (i / (n - 1)) * (W - PADL - 6);
  const y = (v) => PADT + (1 - v / peak) * (H - PADT - PADB);   // 0 at the bottom
  const pass = best.filter((b) => b >= thr).length;
  // The headroom the setting has: how far the floor could move before it starts
  // dropping calipers that currently find their edge.
  const weakest = best.length ? Math.min(...best.filter((b) => b > 0)) : 0;

  return <div style={{ padding: '4px 0' }}>
    <svg width={W} height={H} style={{ background: 'rgba(127,127,127,.06)', borderRadius: 4 }}
         data-testid="edge-profile-plot" data-kind="profile"
         data-calipers={g.length} data-pass={pass}>
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
      <text x={PADL} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">−{profile.L.toFixed(0)}px</text>
      <text x={W - 34} y={H - 3} fontSize="10" fill="rgba(127,127,127,.8)">+{profile.L.toFixed(0)}px</text>
    </svg>

    {/* Says what the setting DOES, in calipers, because that is the thing that
        goes NA -- a fit needs min_inliers of them, not a good-looking graph. */}
    <div style={{ fontSize: 11.5, marginTop: 4,
                  color: pass === g.length ? 'inherit' : '#ff7875' }}>
      {pass}/{g.length} 個 caliper 在這個門檻下找得到邊
      {pass === g.length && weakest > 0 &&
        <span style={{ opacity: 0.65 }}>　（最弱的一個是 {Math.round(weakest)}）</span>}
    </div>
    {controls(Math.ceil(peak * 1.1), suggest, <>
      <div style={{ fontSize: 11.5, opacity: 0.65, marginTop: 4 }}>
        綠線是會被採用的 caliper，紅線是被門檻濾掉的。粉紅區域以下都不算邊。
        最弱的邊 {Math.round(signal)}、最強的雜訊峰 {Math.round(noise)}。
      </div>
      {!clean && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 2 }}>
        雜訊峰和真實邊沒有分開，沒有一個門檻分得掉它們。建議值只保證每個 caliper
        還找得到邊，不保證找到的是對的那條 —— 先看看 caliper 位置或寬度。
      </div>}
    </>)}
  </div>;
}
