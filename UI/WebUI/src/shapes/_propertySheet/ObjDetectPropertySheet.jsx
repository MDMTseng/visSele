// Property sheet for an `obj_detect` region: name, pose flags, and optional brightness/
// Sobel-edge bounds (mean & max). A blank bound = unbounded on that side. The core
// self-judges the region NG if any present bound is violated.
import React, { useState, useRef, useEffect } from 'react';
import AntButton from 'antd/lib/button';
import { Row, Section, NumberField, TextField, SwitchField, DropdownField, translate, toFixed4 } from './primitives.jsx';

// An optional-bound numeric input. Blank clears it (stored undefined => the core
// treats it as NAN/unbounded).
//
// LOCAL STATE, COMMITTED ON BLUR OR ENTER -- not a controlled input writing on
// every keystroke. Writing per keystroke dispatched a def update per character;
// each one re-rendered the sheet, and while a report was arriving the input
// could be rebuilt under the caret, which is what made it "keep unfocusing"
// while the machine was running. Same contract as NumberField.
function BoundInput({ value, onSet, width = 62, title }) {
  const [local, setLocal] = useState(() => toFixed4(value));
  const editing = useRef(false);
  useEffect(() => { if (!editing.current) setLocal(toFixed4(value)); }, [value]);
  const commit = () => {
    editing.current = false;
    const s = String(local).trim();
    if (s === '') { onSet(undefined); return; }
    const n = parseFloat(s);
    onSet(Number.isFinite(n) ? parseFloat(n.toFixed(4)) : undefined);
  };
  return <input type="number" title={title}
    style={{ width, fontSize: 12, padding: '0 3px' }}
    value={local}
    onFocus={() => { editing.current = true; }}
    onChange={(e) => { editing.current = true; setLocal(e.target.value); }}
    onBlur={commit}
    onKeyDown={(e) => { if (e.key === 'Enter') { commit(); e.target.blur(); } }} />;
}

// min and max on ONE row. Every bound in this sheet comes in pairs, and giving
// each half its own labelled row made the sheet twice as tall as it needed to
// be -- the reason it was hard to edit was mostly that you could not see it.
const PAIR_ROW = { display: 'flex', alignItems: 'center', gap: 4 };
const HINT = { fontSize: 10.5, color: '#999' };
function BoundPair({ label, title, minV, maxV, onMin, onMax, unit, extra }) {
  return <Row label={label} title={title}>
    <div style={PAIR_ROW}>
      <span style={HINT}>min</span>
      <BoundInput value={minV} onSet={onMin} title={title} />
      <span style={HINT}>max</span>
      <BoundInput value={maxV} onSet={onMax} title={title} />
      {unit ? <span style={HINT}>{unit}</span> : null}
      {extra}
    </div>
  </Row>;
}

// The button that fills a bound in from what the machine just measured.
// Typing a threshold for a dirty-area check means reading a number off the
// overlay and copying it by hand, which is both slow and how a decimal point
// goes missing. It only appears when there IS a measurement -- an empty region
// has nothing to offer and the button would be a lie.
function ApplyMeasured({ value, onApply, fmt, title }) {
  if (!Number.isFinite(value)) return null;
  return <AntButton size="small" type="link" title={title}
    style={{ fontSize: 11, padding: '0 4px', height: 18 }}
    onClick={() => onApply(parseFloat(value.toFixed(4)))}>
    ← {fmt ? fmt(value) : toFixed4(value)}
  </AntButton>;
}

// Defined at module scope, NOT inside the sheet. A component declared in the
// render body is a NEW component type on every render, so React unmounts the
// old subtree and mounts a fresh one -- every input inside it loses focus the
// moment anything re-renders the sheet. That was the other half of the
// unfocusing, and the half that no amount of input-level care could fix.
function StatBounds({ shape, update, keyBase, label, title }) {
  return <BoundPair label={label} title={title}
    minV={shape[keyBase + '_min']} maxV={shape[keyBase + '_max']}
    onMin={(v) => update({ [keyBase + '_min']: v })}
    onMax={(v) => update({ [keyBase + '_max']: v })} />;
}

// `measuredRegion` is the core's own stats for THIS region from the last
// inspection -- dark_area_mm2, dark_ratio, bright_*, edge_*. Read from the
// report, never from the shape: the shape carries what the def asks for, and
// the two must not be confused.
export function ObjDetectPropertySheet({ shape, onUpdate, dict, dictTheme = 'obj_detect',
                                         measuredRegion }) {
  const update = (patch) => onUpdate({ ...shape, ...patch });
  const t = (key) => translate(dict, dictTheme, key);
  const m = measuredRegion || {};

  return <div>
    <Row label="type"><span style={{ fontSize: 12 }}>obj_detect</span></Row>
    <TextField label="name" value={shape.name} onCommit={(name) => update({ name })} />
    <SwitchField label="忽視旋轉" checked={shape.ignore_rotation}
      onChange={(v) => update({ ignore_rotation: v })} />
    <SwitchField label="忽視位移(絕對位置)" checked={shape.ignore_translation}
      onChange={(v) => update({ ignore_translation: v })} />
    <NumberField label="降採樣 ÷" value={shape.downsample ?? 1} step={1}
      title="降採樣加速統計(平均);注意 max 會變小、Sobel 尺度會變。"
      onCommit={(v) => update({ downsample: Math.max(1, Math.round(v) || 1) })}
      tweak={{ add: [1] }} />

    {/* Dark-area check -- the "is this space clean" measurement.
        dark_thresh blank = the whole dark measurement is off. */}
    <Section label="暗區(淨空檢查)">
      <Row label="暗門檻" title="低於此灰階的像素算「暗」。留空 = 不做暗區量測。暗區在降採樣前以原解析度量,小顆雜物不會被抹掉。">
        <div style={PAIR_ROW}>
          <BoundInput value={shape.dark_thresh} onSet={(v) => update({ dark_thresh: v })} />
          <span style={HINT}>灰階</span>
        </div>
      </Row>
      <BoundPair label="暗面積" unit="mm²"
        title="低於暗門檻的像素總面積。"
        minV={shape.dark_area_min} maxV={shape.dark_area_max}
        onMin={(v) => update({ dark_area_min: v })}
        onMax={(v) => update({ dark_area_max: v })}
        extra={<ApplyMeasured value={m.dark_area_mm2}
                 title="把目前這一幀量到的暗面積填進 max"
                 fmt={(v) => v.toFixed(3)}
                 onApply={(v) => update({ dark_area_max: v })} />} />
      <BoundPair label="暗比例" unit="0~1"
        title="暗像素佔區域的比例。"
        minV={shape.dark_ratio_min} maxV={shape.dark_ratio_max}
        onMin={(v) => update({ dark_ratio_min: v })}
        onMax={(v) => update({ dark_ratio_max: v })}
        extra={<ApplyMeasured value={m.dark_ratio}
                 title="把目前這一幀量到的暗比例填進 max"
                 fmt={(v) => v.toFixed(4)}
                 onApply={(v) => update({ dark_ratio_max: v })} />} />
    </Section>

    {/* What a trip means for the PART. This is the field that decides whether a
        dirty region costs you a lap or a part. */}
    <DropdownField label="超出界限時"
      value={shape.on_fail === 'ng' ? 'NG(吹掉)' : 'NA(不判定,繞回)'}
      options={['NA(不判定,繞回)', 'NG(吹掉)']}
      onChange={(v) => update({ on_fail: v.startsWith('NG') ? 'ng' : 'na' })} />
    <div style={{ ...HINT, margin: '2px 0' }}>
      NA = 視野被污染,這顆量不準 → 繞一圈重測(預設)。NG = 這顆不良 → 走 SEL1 吹掉。
    </div>

    <Section label="亮度 / Sobel(界限留空 = 不限)">
      <StatBounds shape={shape} update={update} keyBase="bright_mean" label="亮度 mean" />
      <StatBounds shape={shape} update={update} keyBase="bright_max"  label="亮度 max" />
      <StatBounds shape={shape} update={update} keyBase="edge_mean"   label="Sobel mean" />
      <StatBounds shape={shape} update={update} keyBase="edge_max"    label="Sobel max" />
    </Section>

    {/* What the last inspection actually saw, so a bound can be judged against
        it without leaving the sheet. */}
    {measuredRegion ? <div style={{ ...HINT, margin: '2px 0', lineHeight: 1.5 }}>
      本次:
      {Number.isFinite(m.dark_area_mm2) ? ` 暗 ${m.dark_area_mm2.toFixed(3)}mm² (${(m.dark_ratio * 100).toFixed(2)}%)` : ''}
      {` B ${fmt1(m.bright_mean)}/${fmt1(m.bright_max)}`}
      {` E ${fmt1(m.edge_mean)}/${fmt1(m.edge_max)}`}
    </div> : null}
  </div>;
}

function fmt1(v) { return (typeof v === 'number') ? v.toFixed(1) : '—'; }
