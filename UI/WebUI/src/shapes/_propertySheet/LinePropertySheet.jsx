// Property sheet for a `line` shape — direct React component, no schema
// dispatcher. Reads the current shape, lays out the fields explicitly,
// dispatches changes through onUpdate(modifiedShape). Caliper/edge nested
// groups appear only when locating == 'caliper'; their defaults are seeded
// here when the user first flips locating (the legacy onChange behavior).
//
// Contract (set by DefConfUI):
//   shape         — current line shape (already Shape_Attr_Fill'd)
//   onUpdate(s)   — persist the updated shape (Shape_Set via ec_canvas)
//   onTracePick   — switch into ref-pick mode for the given keyTrace
//   shapeList     — for ref resolution (search_point/aux_* only — line
//                   doesn't reference other shapes itself, so unused here)
//   dict / theme  — i18n
import { caliperConfigProblem, CALIPER_MIN_COUNT_LINE } from '../_caliperFields';
import React, { useEffect } from 'react';
import {
  Row, Section, NumberField, TextField, SwitchField, DropdownField,
  translate,
} from './primitives.jsx';

const EDGE_METHODS    = ['strongest', 'first', 'last', 'middle', 'nth'];
const EDGE_POLARITIES = ['any', 'rising', 'falling'];

// Compute auto-default width when first switching to caliper mode:
// width = line length / count, so the boxes tile cleanly.
function defaultCaliperWidth(shape, count) {
  if (!shape.pt1 || !shape.pt2 || !(count > 0)) return 0.1;
  const len = Math.hypot(shape.pt2.x - shape.pt1.x, shape.pt2.y - shape.pt1.y);
  return len > 0 ? len / count : 0.1;
}

export function LinePropertySheet({ shape, onUpdate, dict, dictTheme = 'line', lockCaliper = false }) {
  // Single-key updater: spreads a partial patch onto the shape and pushes
  // through onUpdate. Sub-object updates use updateSub('caliper', patch).
  const update = (patch) => onUpdate({ ...shape, ...patch });
  const updateSub = (key, patch) => onUpdate({
    ...shape,
    [key]: { ...(shape[key] || {}), ...patch },
  });

  // Locating mode is the gate that decides whether caliper/edge groups show.
  // First flip to caliper seeds defaults on the stored shape so the canvas
  // overlay matches what the property sheet shows.
  const flipLocating = (next) => {
    const patch = { locating: next };
    if (next === 'caliper') {
      const count = 10;
      if (!shape.caliper) patch.caliper = {
        count, width: defaultCaliperWidth(shape, count),
        min_inliers: 5, max_error: 0.1,
      };
      if (!shape.edge) patch.edge = {
        method: 'strongest', polarity: 'falling', nth: 0, min_strength: 60,
      };
    }
    update(patch);
  };

  const t = (key) => translate(dict, dictTheme, key);

  // New-version (shape_based) defs are caliper-only: force caliper on open and hide the
  // contour/caliper selector (the raw-gray path has no contour for contour mode).
  useEffect(() => {
    if (lockCaliper && shape.locating !== 'caliper') flipLocating('caliper');
    // eslint-disable-next-line
  }, [lockCaliper, shape.id]);
  const isCaliper = lockCaliper || shape.locating === 'caliper';

  const defaultTweak = { mul: [1.5], add: [0.1] };
  return <div>
    <Row label={t('type')}><span style={{ fontSize: 12 }}>{t('line')}</span></Row>
    <TextField label={t('name')} value={shape.name}
      onCommit={(name) => update({ name })} />
    <NumberField label={t('margin')} value={shape.margin}
      onCommit={(margin) => update({ margin })}
      tweak={defaultTweak} />  {/* default: ×2 ÷2 ±0.1 ±0.01 */}
    <SwitchField label={t('vertex_touch_searching')}
      checked={shape.vertex_touch_searching}
      onChange={(v) => update({ vertex_touch_searching: v })} />
    {!lockCaliper &&
      <DropdownField label={t('locating')} value={shape.locating || 'contour'}
        options={['contour', 'caliper']} onChange={flipLocating} />}

    {isCaliper && <>
      <Section label="caliper">
        <NumberField label="count" value={shape.caliper?.count} step={1}
          onCommit={(count) => updateSub('caliper', { count })}
          tweak={{ add: [1] }} />
        <NumberField label={t('width')} value={shape.caliper?.width}
          onCommit={(width) => updateSub('caliper', { width })}
          tweak ={defaultTweak}/>
        <NumberField label="min_inliers" value={shape.caliper?.min_inliers} step={1}
          onCommit={(min_inliers) => updateSub('caliper', { min_inliers })}
          tweak={{  add: [1] }} />
        <NumberField label="max_error" value={shape.caliper?.max_error}
          onCommit={(max_error) => updateSub('caliper', { max_error })}
          tweak ={defaultTweak}/>
        {/* Said HERE, beside the fields that cause it. The overlay draws
            `count` green inlier crosses on a shape this configuration can never
            let succeed, so the picture actively argues against the NA -- see
            caliperConfigProblem. */}
        {caliperConfigProblem(shape.caliper, CALIPER_MIN_COUNT_LINE) && (
          <div style={{ gridColumn: '1 / -1', color: '#c33', fontSize: 12,
                        lineHeight: 1.5, padding: '2px 0' }}>
            ⚠ {caliperConfigProblem(shape.caliper, CALIPER_MIN_COUNT_LINE)}
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
          tweak ={defaultTweak}/>
      </Section>
    </>}
  </div>;
}
