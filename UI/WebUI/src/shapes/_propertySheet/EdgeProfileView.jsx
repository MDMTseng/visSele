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
import React, { useMemo, useState } from 'react';

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
  const W = 320, H = 132, PADL = 30, PADB = 16, PADT = 8;

  const model = useMemo(() => {
    if (!profile || !profile.g || !profile.g.length) return null;
    const g = profile.g;
    // One scale for every caliper: comparing them is most of the value, and a
    // per-caliper autoscale would make a weak edge look like a strong one.
    let peak = 0;
    for (const one of g) for (const v of one) if (Math.abs(v) > peak) peak = Math.abs(v);
    const n = g[0].length;
    // Per-caliper: the strongest peak of the selected polarity. This is the
    // number the floor is compared against, so it is what the readout counts.
    const best = g.map((one) => {
      const p = peaksOf(one, polarity);
      return p.length ? Math.max(...p.map((q) => q.v)) : 0;
    });
    return { g, peak: peak || 1, n, best };
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

  const { g, peak, n, best } = model;
  const x = (i) => PADL + (i / (n - 1)) * (W - PADL - 6);
  const y = (v) => PADT + (1 - v / peak) * (H - PADT - PADB);   // 0 at the bottom
  const thr = Math.max(0, Number(minStrength) || 0);
  const pass = best.filter((b) => b >= thr).length;
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
             onChange={(e) => onChange(Number(e.target.value))}
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
    <button type="button" onClick={onProbe} disabled={busy}
      data-testid="edge-profile-recheck"
      style={{ marginTop: 6, padding: '3px 10px', cursor: busy ? 'default' : 'pointer' }}>
      {busy ? '檢查中…' : '重新檢查'}
    </button>
    {note && <div style={{ fontSize: 11.5, color: '#ff7875', marginTop: 4 }}>{note}</div>}
  </div>;
}
