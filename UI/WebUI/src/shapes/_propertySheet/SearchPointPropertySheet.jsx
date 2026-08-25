// Property sheet for a `search_point` shape — direct React component.
// See LinePropertySheet for the contract + rationale.
//
// search_point specifics:
//   - `angleDeg` numeric with ±30 / ±5 / 0 nudge buttons (legacy AngleRangeSetup
//     tweaks), input wraps to [0, 360).
//   - `search_far` switch (with legacy `search_style: 0|1` migration handled
//     in shapes/search_point.js's `derive` — by the time we render, the
//     applyDefaults pass has already produced a boolean).
//   - `locating_anchor` switch — independent of `locating` (different concept).
//   - `locating` dropdown only gates `edge` sub-group (no caliper count/width;
//     core runs ONE caliper along the search vector — see search_point.js).
//   - `ref[0]` slot — references one line shape; clicking enters ref-pick mode
//     via onTracePick(["ref", "0"]).
import React from 'react';
import {
  Row, Section, NumberField, TextField, SwitchField, DropdownField,
  RefSlot, StepButton, NumberTweakActions, translate,
} from './primitives.jsx';

const EDGE_METHODS    = ['strongest', 'first', 'last', 'middle', 'nth'];
const EDGE_POLARITIES = ['any', 'rising', 'falling'];

const wrap360 = (v) => ((Number(v) % 360) + 360) % 360;

export function SearchPointPropertySheet({
  shape, shapeList, onUpdate, onTracePick, dict, dictTheme = 'search_point',
}) {
  const update = (patch) => onUpdate({ ...shape, ...patch });
  const updateSub = (key, patch) => onUpdate({
    ...shape,
    [key]: { ...(shape[key] || {}), ...patch },
  });

  const flipLocating = (next) => {
    const patch = { locating: next };
    if (next === 'caliper' && !shape.edge) {
      // Same seed as a freshly created search_point (see
      // EverCheckCanvasComponent's create block). Switching an existing point
      // to caliper is the other way to arrive at caliper for the first time,
      // and arriving there with min_strength 0 / include_range 0 is what makes
      // caliper look unreliable on first contact.
      patch.edge = {
        method: 'first', polarity: 'falling', nth: 0,
        min_strength: 60, include_range: 0.01, manual_offset: 0,
      };
    }
    update(patch);
  };

  const t = (key) => translate(dict, dictTheme, key);
  const defaultTweak = { mul: [1.5], add: [0.1] };

  // Angle quick-actions: ±30, ±5, reset-0. Mirrors legacy AngleRangeSetup.
  const setAngle = (v) => update({ angleDeg: wrap360(v) });
  const angleActions = <>
    <StepButton onClick={() => setAngle((shape.angleDeg || 0) + 30)} title="+30">+30</StepButton>
    <StepButton onClick={() => setAngle((shape.angleDeg || 0) - 30)} title="−30">−30</StepButton>
    <StepButton onClick={() => setAngle((shape.angleDeg || 0) + 5)}  title="+5">+5</StepButton>
    <StepButton onClick={() => setAngle((shape.angleDeg || 0) - 5)}  title="−5">−5</StepButton>
    <StepButton onClick={() => setAngle(0)}                          title="0">0</StepButton>
  </>;

  const refEntry = shape.ref && shape.ref[0];

  return <div>
    <Row label={t('type')}><span style={{ fontSize: 12 }}>{t('search_point')}</span></Row>
    <TextField label={t('name')} value={shape.name}
      onCommit={(name) => update({ name })} />
    <NumberField label={t('margin')} value={shape.margin}
      onCommit={(margin) => update({ margin })}
      tweak={defaultTweak} />
    <NumberField label={t('width')} value={shape.width}
      onCommit={(width) => update({ width })}
      tweak={defaultTweak} />
    <NumberField label={t('angleDeg')} value={shape.angleDeg}
      onCommit={(v) => update({ angleDeg: wrap360(v) })}
      quickActions={angleActions} />
    <SwitchField label={t('search_far')}
      checked={!!shape.search_far}
      onChange={(v) => update({ search_far: v })} />
    <DropdownField label={t('locating')} value={shape.locating || 'contour'}
      options={['contour', 'caliper']} onChange={flipLocating} />
    <SwitchField label={t('locating_anchor')}
      checked={!!shape.locating_anchor}
      onChange={(v) => update({ locating_anchor: v })} />
    {shape.locating_anchor && <SwitchField label={t('anchor_corner')}
      checked={!!shape.anchor_corner}
      onChange={(v) => update({ anchor_corner: v })} />}

    {shape.locating === 'caliper' && <Section label="edge">
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
      <NumberField label="include_range" value={shape.edge?.include_range}
        onCommit={(include_range) => updateSub('edge', { include_range })}
        tweak={defaultTweak} />
      <NumberField label="manual_offset" value={shape.edge?.manual_offset}
        onCommit={(manual_offset) => updateSub('edge', { manual_offset })}
        tweak={defaultTweak} />
    </Section>}

    <Section label={t('ref') || 'ref'}>
      <Row label={t('0') || '0'}>
        <RefSlot refEntry={refEntry} shapeList={shapeList}
          onPick={() => onTracePick && onTracePick(['ref', '0'])} />
      </Row>
    </Section>
  </div>;
}
