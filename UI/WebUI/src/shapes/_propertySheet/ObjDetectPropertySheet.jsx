// Property sheet for an `obj_detect` region: name, pose flags, and optional brightness/
// Sobel-edge bounds (mean & max). A blank bound = unbounded on that side. The core
// self-judges the region NG if any present bound is violated.
import React from 'react';
import { Row, Section, NumberField, TextField, SwitchField, DropdownField, translate } from './primitives.jsx';

// An optional-bound numeric field: blank clears it (stored undefined => core treats as
// NAN/unbounded). Shows the current value or empty.
function BoundField({ label, value, onSet }) {
  return <Row label={label}>
    <input type="number" style={{ width: 70, fontSize: 12 }}
      value={value === undefined || value === null || Number.isNaN(value) ? '' : value}
      onChange={(e) => {
        const s = e.target.value;
        onSet(s === '' ? undefined : parseFloat(s));
      }} />
  </Row>;
}

export function ObjDetectPropertySheet({ shape, onUpdate, dict, dictTheme = 'obj_detect' }) {
  const update = (patch) => onUpdate({ ...shape, ...patch });
  const t = (key) => translate(dict, dictTheme, key);

  const StatBounds = ({ keyBase, label }) => (
    <Section label={label}>
      <BoundField label="min" value={shape[keyBase + '_min']} onSet={(v) => update({ [keyBase + '_min']: v })} />
      <BoundField label="max" value={shape[keyBase + '_max']} onSet={(v) => update({ [keyBase + '_max']: v })} />
    </Section>
  );

  return <div>
    <Row label="type"><span style={{ fontSize: 12 }}>obj_detect</span></Row>
    <TextField label="name" value={shape.name} onCommit={(name) => update({ name })} />
    <SwitchField label="忽視旋轉" checked={shape.ignore_rotation}
      onChange={(v) => update({ ignore_rotation: v })} />
    <SwitchField label="忽視位移(絕對位置)" checked={shape.ignore_translation}
      onChange={(v) => update({ ignore_translation: v })} />
    <NumberField label="降採樣 ÷" value={shape.downsample ?? 1} step={1}
      onCommit={(v) => update({ downsample: Math.max(1, Math.round(v) || 1) })}
      tweak={{ add: [1] }} />
    <div style={{ fontSize: 11, color: '#999', margin: '2px 0' }}>
      降採樣加速統計(平均);注意 max 會變小、Sobel 尺度會變。界限留空 = 不限,任一超出 → NG。
    </div>

    {/* Dark-area check -- the "is this space clean" measurement.
        dark_thresh blank = the whole dark measurement is off. */}
    <Section label="暗區(淨空檢查)">
      <BoundField label="暗門檻 (灰階)" value={shape.dark_thresh}
        onSet={(v) => update({ dark_thresh: v })} />
      <BoundField label="暗面積 max (mm²)" value={shape.dark_area_max}
        onSet={(v) => update({ dark_area_max: v })} />
      <BoundField label="暗面積 min (mm²)" value={shape.dark_area_min}
        onSet={(v) => update({ dark_area_min: v })} />
      <BoundField label="暗比例 max (0~1)" value={shape.dark_ratio_max}
        onSet={(v) => update({ dark_ratio_max: v })} />
      <BoundField label="暗比例 min (0~1)" value={shape.dark_ratio_min}
        onSet={(v) => update({ dark_ratio_min: v })} />
    </Section>
    <div style={{ fontSize: 11, color: '#999', margin: '2px 0' }}>
      低於暗門檻的像素量。門檻留空 = 不做暗區量測。
      暗區在**降採樣前**以原解析度量,所以小顆雜物不會被降採樣抹掉。
    </div>

    {/* What a trip means for the PART. This is the field that decides whether a
        dirty region costs you a lap or a part. */}
    <DropdownField label="超出界限時"
      value={shape.on_fail === 'ng' ? 'NG(吹掉)' : 'NA(不判定,繞回)'}
      options={['NA(不判定,繞回)', 'NG(吹掉)']}
      onChange={(v) => update({ on_fail: v.startsWith('NG') ? 'ng' : 'na' })} />
    <div style={{ fontSize: 11, color: '#999', margin: '2px 0' }}>
      NA = 視野被污染,這顆量不準 → 不動作,繞一圈重測(預設)。
      NG = 這顆本身不良 → 走 SEL1 吹掉。
    </div>

    <StatBounds keyBase="bright_mean" label="亮度 mean" />
    <StatBounds keyBase="bright_max"  label="亮度 max" />
    <StatBounds keyBase="edge_mean"   label="Sobel mean" />
    <StatBounds keyBase="edge_max"    label="Sobel max" />
  </div>;
}
