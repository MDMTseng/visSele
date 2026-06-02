// Property sheet for an `arc` shape — direct React component, sibling of
// LinePropertySheet. See LinePropertySheet for the contract + rationale.
//
// Arc-specific bits vs line:
//   - `direction` switch (stored as ±1, not bool — coerce in the handler).
//   - caliper width auto-default = arc length / count (not chord length).
import React from 'react';
import { threePointToArc } from 'UTIL/MathTools';
import {
  Row, Section, NumberField, TextField, SwitchField, DropdownField,
  translate,
} from './primitives.jsx';

const EDGE_METHODS    = ['strongest', 'first', 'last', 'middle', 'nth'];
const EDGE_POLARITIES = ['any', 'rising', 'falling'];

// Arc length pt1→pt3 in def-mm. Used to seed caliper width on first
// flip to caliper mode so the boxes tile cleanly along the arc.
function arcLengthOf(shape) {
  if (!(shape.pt1 && shape.pt2 && shape.pt3)) return 0;
  const a = threePointToArc(shape.pt1, shape.pt2, shape.pt3);
  if (!(a.r > 0)) return 0;
  const a0 = Math.atan2(shape.pt1.y - a.y, shape.pt1.x - a.x);
  const a2 = Math.atan2(shape.pt3.y - a.y, shape.pt3.x - a.x);
  let span = a2 - a0;
  while (span < 0) span += 2 * Math.PI;
  return a.r * span;
}

function defaultCaliperWidth(shape, count) {
  const L = arcLengthOf(shape);
  return (L > 0 && count > 0) ? L / count : 0.1;
}

export function ArcPropertySheet({ shape, onUpdate, dict, dictTheme = 'arc' }) {
  const update = (patch) => onUpdate({ ...shape, ...patch });
  const updateSub = (key, patch) => onUpdate({
    ...shape,
    [key]: { ...(shape[key] || {}), ...patch },
  });

  const flipLocating = (next) => {
    const patch = { locating: next };
    if (next === 'caliper') {
      const count = 10;
      if (!shape.caliper) patch.caliper = {
        count, width: defaultCaliperWidth(shape, count),
        min_inliers: 0, max_error: 0,
      };
      if (!shape.edge) patch.edge = {
        method: 'strongest', polarity: 'falling', nth: 0, min_strength: 0,
      };
    }
    update(patch);
  };

  const t = (key) => translate(dict, dictTheme, key);
  const defaultTweak = { mul: [1.5], add: [0.1] };

  return <div>
    <Row label={t('type')}><span style={{ fontSize: 12 }}>{t('arc')}</span></Row>
    <TextField label={t('name')} value={shape.name}
      onCommit={(name) => update({ name })} />
    <NumberField label={t('margin')} value={shape.margin}
      onCommit={(margin) => update({ margin })}
      tweak={defaultTweak} />
    {/* direction is stored as ±1; checked = -1, unchecked = +1 */}
    <SwitchField label={t('direction')}
      checked={shape.direction === -1}
      onChange={(v) => update({ direction: v ? -1 : 1 })} />
    <DropdownField label={t('locating')} value={shape.locating || 'contour'}
      options={['contour', 'caliper']} onChange={flipLocating} />
    <DropdownField label={t('fit_mode') || 'fit_mode'} value={shape.fit_mode || 'ls'}
      options={['ls', 'outer', 'inner']}
      onChange={(fit_mode) => update({ fit_mode })} />

    {shape.locating === 'caliper' && <>
      <Section label="caliper">
        <NumberField label="count" value={shape.caliper?.count} step={1}
          onCommit={(count) => updateSub('caliper', { count })}
          tweak={{ add: [1] }} />
        <NumberField label={t('width')} value={shape.caliper?.width}
          onCommit={(width) => updateSub('caliper', { width })}
          tweak={defaultTweak} />
        <NumberField label="min_inliers" value={shape.caliper?.min_inliers} step={1}
          onCommit={(min_inliers) => updateSub('caliper', { min_inliers })}
          tweak={{ add: [1] }} />
        <NumberField label="max_error" value={shape.caliper?.max_error}
          onCommit={(max_error) => updateSub('caliper', { max_error })}
          tweak={defaultTweak} />
      </Section>
      <Section label="edge">
        <DropdownField label="method" value={shape.edge?.method}
          options={EDGE_METHODS}
          onChange={(method) => updateSub('edge', { method })} />
        <DropdownField label="polarity" value={shape.edge?.polarity}
          options={EDGE_POLARITIES}
          onChange={(polarity) => updateSub('edge', { polarity })} />
        <NumberField label="nth" value={shape.edge?.nth} step={1}
          onCommit={(nth) => updateSub('edge', { nth })}
          tweak={{ add: [1] }} />
        <NumberField label="min_strength" value={shape.edge?.min_strength}
          onCommit={(min_strength) => updateSub('edge', { min_strength })}
          tweak={defaultTweak} />
      </Section>
    </>}
  </div>;
}
