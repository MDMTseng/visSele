// Property sheet for an `arc` shape — direct React component, sibling of
// LinePropertySheet. See LinePropertySheet for the contract + rationale.
//
// Arc-specific bits vs line:
//   - `direction` switch (stored as ±1, not bool — coerce in the handler).
//   - caliper width auto-default = arc length / count (not chord length).
import { caliperConfigProblem, CALIPER_MIN_COUNT_ARC } from '../_caliperFields';
import { ARC_POLARITY, EDGE_MIN_STRENGTH } from '../_caliperSeed';
import React, { useEffect } from 'react';
import { arcSweep } from 'UTIL/MathTools';
import {
  Row, Section, NumberField, TextField, SwitchField, DropdownField,
  translate,
} from './primitives.jsx';

const EDGE_METHODS    = ['strongest', 'first', 'last', 'middle', 'nth'];
const EDGE_POLARITIES = ['any', 'rising', 'falling'];

// Arc length pt1→pt3 in def-mm. Used to seed caliper width on first
// flip to caliper mode so the boxes tile cleanly along the arc.
function arcLengthOf(shape) {
  // arcSweep, not a pt1->pt3 span. This was a third copy of a calculation that
  // never looked at pt2, and so returned the COMPLEMENT of the arc whenever it
  // was drawn the other way round.
  if (!(shape.pt1 && shape.pt2 && shape.pt3)) return 0;
  return arcSweep(shape.pt1, shape.pt2, shape.pt3).length;
}

function defaultCaliperWidth(shape, count) {
  const L = arcLengthOf(shape);
  return (L > 0 && count > 0) ? L / count : 0.1;
}

export function ArcPropertySheet({ shape, onUpdate, dict, dictTheme = 'arc', lockCaliper = false }) {
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
        min_inliers: 5, max_error: 0.1,
      };
      if (!shape.edge) patch.edge = {
        // ARC_POLARITY: see arc.js -- falling costs -0.11mm on an inner radius.
        method: 'strongest', polarity: ARC_POLARITY, nth: 0, min_strength: EDGE_MIN_STRENGTH,
      };
    }
    update(patch);
  };

  const t = (key) => translate(dict, dictTheme, key);
  const defaultTweak = { mul: [1.5], add: [0.1] };

  // shape_based defs are caliper-only: force caliper on open, hide the selector.
  useEffect(() => {
    if (lockCaliper && shape.locating !== 'caliper') flipLocating('caliper');
    // eslint-disable-next-line
  }, [lockCaliper, shape.id]);
  const isCaliper = lockCaliper || shape.locating === 'caliper';

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
    {!lockCaliper &&
      <DropdownField label={t('locating')} value={shape.locating || 'contour'}
        options={['contour', 'caliper']} onChange={flipLocating} />}
    <DropdownField label={t('fit_mode') || 'fit_mode'} value={shape.fit_mode || 'ls'}
      options={['ls', 'outer', 'inner']}
      onChange={(fit_mode) => update({ fit_mode })} />

    {isCaliper && <>
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
        {/* Same as the line sheet: said beside the fields that cause it,
            because the overlay draws green inlier crosses on a shape this
            configuration can never let succeed. */}
        {caliperConfigProblem(shape.caliper, CALIPER_MIN_COUNT_ARC) && (
          <div style={{ gridColumn: '1 / -1', color: '#c33', fontSize: 12,
                        lineHeight: 1.5, padding: '2px 0' }}>
            ⚠ {caliperConfigProblem(shape.caliper, CALIPER_MIN_COUNT_ARC)}
          </div>
        )}
      </Section>
      <Section label="edge">
        <DropdownField label="method" value={shape.edge?.method}
          options={EDGE_METHODS}
          onChange={(method) => updateSub('edge', { method })} />
        <DropdownField label="polarity" value={shape.edge?.polarity}
          options={EDGE_POLARITIES}
          onChange={(polarity) => updateSub('edge', { polarity })} />
        {shape.edge?.method === 'nth' &&
          <NumberField label="nth" value={shape.edge?.nth} step={1}
            onCommit={(nth) => updateSub('edge', { nth })}
            tweak={{ add: [1] }} />}
        <NumberField label="min_strength" value={shape.edge?.min_strength}
          onCommit={(min_strength) => updateSub('edge', { min_strength })}
          tweak={defaultTweak} />
      </Section>
    </>}
  </div>;
}
