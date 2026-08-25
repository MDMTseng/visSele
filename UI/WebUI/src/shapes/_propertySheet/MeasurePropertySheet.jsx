// Property sheet for a `measure` shape — direct React component, sibling of
// LinePropertySheet/ArcPropertySheet/SearchPointPropertySheet.
//
// Measure has 5 subtypes (distance, angle, radius, circle_info, calc) and
// a long shared field list (value + USL/LSL/UCL/LCL ± back-side copies,
// importance/width/quality_essential/orientation_essential/NGasNA/NAasNG,
// value_A/B/X/Y calibration, ref_baseLine + 3 ref slots). Limit-coupling
// onChange handlers from shapes/measure/index.js are reused so byte-identical
// def serialization is preserved — we just dispatch them inline at commit.
//
// Calc subtype: the bespoke `calc_f` editor (postfix-expression builder)
// remains as Measure_Calc_Editor in DefConfUI.js; we embed it directly here
// instead of redoing it as primitives.
import React from 'react';
import { useSelector } from 'react-redux';
import { SHAPE_TYPE } from 'REDUX_STORE_SRC/actions/UIAct';
import { fields as measureFields } from '../measure';
import { BACK_SIDE_LIMITS_ENABLED } from 'UTIL/backSideLimits';
import { Measure_Calc_Editor } from 'JSSRCROOT/DefConfUI';
import {
  Row, Section, NumberField, TextField, SwitchField, DropdownField,
  RefSlot, StepButton, translate,
} from './primitives.jsx';

const SUBTYPES = Object.values(SHAPE_TYPE.measure_subtype).filter(v => v !== 'NA');
const CIRCLE_INFO_TYPES = Object.values(SHAPE_TYPE._circle_info_type);

// Resolve a field's onChange (from the measure module's `fields` decl) and
// apply it after writing the new value. Mirrors what DefConfUI's jsonChange
// would do via applyFieldChange — keeps coupling (value→UCL/LCL/USL/LSL etc.)
// working from the per-shape component too.
function commitField(shape, key, newVal, onUpdate) {
  const next = { ...shape, [key]: newVal };
  const field = measureFields[key];
  if (field && typeof field.onChange === 'function') {
    field.onChange(next, shape[key]);
  }
  onUpdate(next);
}

const wrap360 = (v) => ((Number(v) % 360) + 360) % 360;

export function MeasurePropertySheet({
  shape, shapeList, onUpdate, onTracePick, dict, dictTheme = 'measure',
}) {
  const t = (key) => translate(dict, dictTheme, key);
  const set = (key) => (v) => commitField(shape, key, v, onUpdate);

  const limitTweak  = { add: [0.1, 0.01] };
  const sizeTweak   = { mul: [1.5], add: [0.1] };
  const angleActs   = (key, currentVal) => <>
    <StepButton onClick={() => commitField(shape, key, wrap360((currentVal||0)+30), onUpdate)}>+30</StepButton>
    <StepButton onClick={() => commitField(shape, key, wrap360((currentVal||0)-30), onUpdate)}>−30</StepButton>
    <StepButton onClick={() => commitField(shape, key, wrap360((currentVal||0)+5), onUpdate)}>+5</StepButton>
    <StepButton onClick={() => commitField(shape, key, wrap360((currentVal||0)-5), onUpdate)}>−5</StepButton>
    <StepButton onClick={() => commitField(shape, key, 0, onUpdate)}>0</StepButton>
  </>;

  const isCalc       = shape.subtype === SHAPE_TYPE.measure_subtype.calc;
  const isCircleInfo = shape.subtype === SHAPE_TYPE.measure_subtype.circle_info;
  const refCount     = isCalc ? 0 : 3;

  return <div>
    <Row label={t('type')}><span style={{ fontSize: 12 }}>{t('measure')}</span></Row>
    <TextField label={t('name')} value={shape.name}
      onCommit={(name) => onUpdate({ ...shape, name })} />
    <DropdownField label={t('subtype')} value={shape.subtype}
      options={SUBTYPES}
      onChange={(subtype) => onUpdate({ ...shape, subtype })} />

    {/* Target + control/spec limits */}
    <Section label="target">
      {shape.angleDeg !== undefined &&
        <NumberField label={t('angleDeg')} value={shape.angleDeg}
          onCommit={(v) => commitField(shape, 'angleDeg', wrap360(v), onUpdate)}
          quickActions={angleActs('angleDeg', shape.angleDeg)} />}
      <NumberField label={t('value')} value={shape.value}
        onCommit={set('value')} tweak={limitTweak} />
      <NumberField label="USL" value={shape.USL}
        onCommit={set('USL')} tweak={limitTweak} />
      <NumberField label="LSL" value={shape.LSL}
        onCommit={set('LSL')} tweak={limitTweak} />
      <NumberField label="UCL" value={shape.UCL}
        onCommit={set('UCL')} tweak={limitTweak} />
      <NumberField label="LCL" value={shape.LCL}
        onCommit={set('LCL')} tweak={limitTweak} />
    </Section>

    {/* Back-side limit set (toggle + nested copy) */}
    {/* Back-side limits are disabled (UTIL/backSideLimits.js). The switch is
        hidden rather than merely ignored: leaving a control that configures
        nothing is how someone spends an afternoon setting numbers the machine
        will not use -- which is most of what the audit found. A def that
        already carries _b values keeps them; they are stripped from the wire
        def, so the core stops applying them too. */}
    {BACK_SIDE_LIMITS_ENABLED && <SwitchField label={t('back_value_setup')}
      checked={!!shape.back_value_setup}
      onChange={(v) => commitField(shape, 'back_value_setup', v, onUpdate)} />}
    {BACK_SIDE_LIMITS_ENABLED && shape.back_value_setup && <Section label="back">
      <NumberField label="value_b" value={shape.value_b}
        onCommit={set('value_b')} tweak={limitTweak} />
      <NumberField label="USL_b" value={shape.USL_b}
        onCommit={set('USL_b')} tweak={limitTweak} />
      <NumberField label="LSL_b" value={shape.LSL_b}
        onCommit={set('LSL_b')} tweak={limitTweak} />
      <NumberField label="UCL_b" value={shape.UCL_b}
        onCommit={set('UCL_b')} tweak={limitTweak} />
      <NumberField label="LCL_b" value={shape.LCL_b}
        onCommit={set('LCL_b')} tweak={limitTweak} />
    </Section>}

    {/* Behavior */}
    <Section label="behavior">
      <NumberField label={t('importance')} value={shape.importance} step={1}
        onCommit={set('importance')} tweak={{ add: [1] }} />
      {/* <NumberField label={t('width')} value={shape.width}
        onCommit={set('width')} tweak={sizeTweak} /> */}
      <SwitchField label={t('quality_essential')}
        checked={shape.quality_essential !== false}
        onChange={(v) => commitField(shape, 'quality_essential', v, onUpdate)} />
      <SwitchField label={t('orientation_essential')}
        checked={!!shape.orientation_essential}
        onChange={(v) => commitField(shape, 'orientation_essential', v, onUpdate)} />
      <SwitchField label="NGasNA" checked={!!shape.NGasNA}
        onChange={(v) => commitField(shape, 'NGasNA', v, onUpdate)} />
      <SwitchField label="NAasNG" checked={!!shape.NAasNG}
        onChange={(v) => commitField(shape, 'NAasNG', v, onUpdate)} />
    </Section>

    {/* Calibration mapping value_A/B → value_X/Y */}
    <Section label="value mapping">
      <NumberField label="value_A" value={shape.value_A} onCommit={set('value_A')} tweak={sizeTweak} />
      <NumberField label="value_B" value={shape.value_B} onCommit={set('value_B')} tweak={sizeTweak} />
      <NumberField label="value_X" value={shape.value_X} onCommit={set('value_X')} tweak={sizeTweak} />
      <NumberField label="value_Y" value={shape.value_Y} onCommit={set('value_Y')} tweak={sizeTweak} />
    </Section>

    {/* circle_info subtype: which scalar to extract */}
    {isCircleInfo && <DropdownField label={t('info_type')}
      value={shape.info_type}
      options={CIRCLE_INFO_TYPES}
      onChange={(info_type) => onUpdate({ ...shape, info_type })} />}

    {/* References */}
    <Section label={t('ref_baseLine') || 'ref_baseLine'}>
      <Row label={t('baseLine') || '0'}>
        <RefSlot refEntry={shape.ref_baseLine} shapeList={shapeList}
          onPick={() => onTracePick && onTracePick(['ref_baseLine'])} />
      </Row>
    </Section>

    {refCount > 0 && <Section label={t('ref') || 'ref'}>
      {Array.from({ length: refCount }).map((_, i) => (
        <Row key={i} label={String(i)}>
          <RefSlot refEntry={shape.ref && shape.ref[i]} shapeList={shapeList}
            onPick={() => onTracePick && onTracePick(['ref', String(i)])} />
        </Row>
      ))}
    </Section>}

    {/* calc subtype: postfix-expression builder. Delegates to the legacy
        Measure_Calc_Editor (DefConfUI export) — bespoke widget that's not
        worth re-implementing as primitives yet. */}
    {isCalc && <CalcFEditor shape={shape} shapeList={shapeList}
      onUpdate={onUpdate} onTracePick={onTracePick} />}
  </div>;
}

// Wraps Measure_Calc_Editor in a JsonEditBlock-style { target, onChange,
// renderContext } adapter so the legacy component drops in unchanged.
function CalcFEditor({ shape, shapeList, onUpdate, onTracePick }) {
  // measure_list: every other measure (Measure_Calc_Editor filters internally
  // for loop avoidance via refChainHasLoop, but we pre-filter by type at least).
  const measure_list = (shapeList || []).filter(s => s.type === SHAPE_TYPE.measure);
  const target = { obj: shape, keyTrace: ['calc_f'] };
  const onChange = (_tar, _type, evt) => {
    // Measure_Calc_Editor's onChange signature: (target, "input", { target: { value: { exp, post_exp } } })
    const next = evt && evt.target && evt.target.value;
    if (!next) return;
    onUpdate({ ...shape, calc_f: next });
  };
  return <Section label="calc_f">
    <Measure_Calc_Editor target={target} onChange={onChange} className=""
      renderContext={{
        measure_list,
        ref_keyTrace_callback: (kt) => onTracePick && onTracePick(kt),
        ref: shape.ref || [],
      }} />
  </Section>;
}
