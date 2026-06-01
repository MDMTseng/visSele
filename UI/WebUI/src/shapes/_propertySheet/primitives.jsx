// Compact, primitive building blocks for per-shape PropertySheet components.
//
// Design intent: each shape has its own explicit React component that lays
// out its fields with these primitives — NO schema-driven dispatcher, NO
// renderLib indirection. Field changes go through a single onUpdate
// callback the parent supplies (which typically dispatches Shape_Set via
// ec_canvas.SetShape).
//
// Why this exists: the previous JsonEditBlock-driven path coupled schema
// declarations to a generic dispatcher to per-shape behavior in a way that
// surprised at every edit. Direct components trade verbosity for clarity:
// reading a shape's PropertySheet tells you exactly what the UI does.

import React, { useState, useEffect, useRef } from 'react';
import Switch from 'antd/lib/switch';
import Menu from 'antd/lib/menu';
import Dropdown from 'antd/lib/dropdown';
import AntButton from 'antd/lib/button';
import Popover from 'antd/lib/popover';
import { CaretDownOutlined } from '@ant-design/icons';
import { GetObjElement } from 'UTIL/MISC_Util';

// ───── Style constants ───────────────────────────────────────────────────
export const ROW_STYLE = {
  display: 'flex', alignItems: 'center', minHeight: 24, gap: 6,
  padding: '1px 4px', fontSize: 12,
  borderBottom: '1px solid rgba(0,0,0,0.05)',
};
export const LABEL_STYLE = {
  flex: '0 0 88px', color: '#333', overflow: 'hidden',
  textOverflow: 'ellipsis', whiteSpace: 'nowrap',
};
export const VALUE_STYLE = {
  flex: 1, minWidth: 0, display: 'flex', alignItems: 'center', gap: 3,
};
export const INPUT_STYLE = {
  width: '100%', height: 22, fontSize: 12, padding: '0 4px',
  border: '1px solid #ccc', borderRadius: 3, background: 'white',
  color: '#222', boxSizing: 'border-box',
};
export const STEP_BTN_STYLE = {
  flex: '0 0 auto', height: 20, padding: '0 5px', fontSize: 11,
  border: '1px solid #ccc', borderRadius: 3, background: '#f5f5f5',
  color: '#333', cursor: 'pointer', lineHeight: '18px', minWidth: 24,
};
export const SECTION_HEADER_STYLE = {
  fontSize: 11, color: '#666', letterSpacing: '0.5px', fontWeight: 500,
  padding: '6px 4px 3px', borderBottom: '1px solid rgba(0,0,0,0.08)',
  textTransform: 'uppercase',
};
export const SECTION_BODY_STYLE = {
  paddingLeft: 8, borderLeft: '2px solid rgba(0,0,0,0.06)',
  margin: '0 0 4px 4px',
};

// ───── i18n helper ───────────────────────────────────────────────────────
// dict: nested {theme: {key: 'label'}, _: {key: 'fallback-label'}}.
// Used by Row/Section to translate field keys to displayed labels.
export function translate(dict, theme, key) {
  if (!dict) return key;
  const a = GetObjElement(dict, [theme, key]);
  if (a !== undefined) return a;
  const b = GetObjElement(dict, ['_', key]);
  return (b !== undefined) ? b : key;
}

// ───── Number rounding ───────────────────────────────────────────────────
// Property-sheet convention: numbers display + commit rounded to 4 decimals.
export function toFixed4(v) {
  if (v === undefined || v === null || v === '') return '';
  const n = (typeof v === 'string') ? parseFloat(v) : v;
  if (!Number.isFinite(n)) return '';
  return '' + parseFloat(n.toFixed(4));
}

// ───── Row (label + value flex container) ────────────────────────────────
// Children fill the value column; label sits on the left (88px column).
// `actions` (optional): JSX rendered inside a popover that pops out when
// the user clicks the label. Used for tweak buttons (×2, ±0.1, ...) that
// should stay hidden until requested — keeps the row visually quiet.
export function Row({ label, title, children, actions }) {
  const labelNode = <div style={LABEL_STYLE} title={title || label}>{label}</div>;
  return <div style={ROW_STYLE}>
    {actions
      ? <Popover
          content={<div style={{ display: 'flex', gap: 4, flexWrap: 'wrap', maxWidth: 240 }}>{actions}</div>}
          trigger="click" placement="leftTop"
        >
          <div style={{ ...LABEL_STYLE, cursor: 'pointer', textDecoration: 'underline dotted #999', textDecorationThickness: 1 }}
               title={title || label}>
            {label}
          </div>
        </Popover>
      : labelNode
    }
    <div style={VALUE_STYLE}>{children}</div>
  </div>;
}

// ───── Section (header + indented body for nested groups) ────────────────
// Used for caliper/edge/ref sub-groups. Header is a label; if the user
// passes `onClick`, it becomes a clickable trigger (ref-pick anchor).
export function Section({ label, onClick, children }) {
  return <div>
    <div style={SECTION_HEADER_STYLE}>
      {onClick
        ? <AntButton size="small" type="link"
            style={{ fontSize: 11, padding: 0, height: 'auto', color: '#1565c0' }}
            onClick={onClick}>{label}</AntButton>
        : <span>{label}</span>}
    </div>
    <div style={SECTION_BODY_STYLE}>{children}</div>
  </div>;
}

// ───── NumberField ───────────────────────────────────────────────────────
// Local-state input. Commits on blur or Enter, parses + rounds to 4
// decimals. Esc reverts. While editing, external value-prop changes are
// held off so a parallel redux update doesn't stomp the user's keystrokes.
//
// Tweak buttons (pop out from the label on click — kept hidden by default
// to keep rows visually quiet) come in two flavors:
//   tweak         — shortcut: `true` for defaults (×2 ÷2 ±0.1 ±0.01), or
//                   `{mul: [...], add: [...]}` for custom factors/deltas.
//                   Internally constructs <NumberTweakActions> using this
//                   field's own value + onCommit.
//   quickActions  — escape hatch: raw JSX for non-numeric tweak patterns
//                   (used by shape-specific button groups).
export function NumberField({ label, value, onCommit, step = 0.0001, quickActions, tweak }) {
  const [local, setLocal] = useState(() => toFixed4(value));
  const editing = useRef(false);
  useEffect(() => { if (!editing.current) setLocal(toFixed4(value)); }, [value]);

  const commit = () => {
    editing.current = false;
    const n = parseFloat(local);
    if (Number.isFinite(n)) {
      const rounded = parseFloat(n.toFixed(4));
      onCommit(rounded);
      setLocal('' + rounded);
    } else {
      setLocal(toFixed4(value));
    }
  };

  // Resolve actions: explicit JSX wins; else `tweak` builds the default
  // NumberTweakActions; else no popover.
  const actions = quickActions
    ?? (tweak
        ? <NumberTweakActions
            value={value} onCommit={onCommit}
            mul={typeof tweak === 'object' ? tweak.mul : undefined}
            add={typeof tweak === 'object' ? tweak.add : undefined}
          />
        : undefined);

  return <Row label={label} actions={actions}>
    <input
      type="number" step={step} style={INPUT_STYLE} value={local}
      onChange={(e) => { editing.current = true; setLocal(e.target.value); }}
      onBlur={commit}
      onKeyDown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        else if (e.key === 'Escape') {
          editing.current = false;
          setLocal(toFixed4(value));
          e.currentTarget.blur();
        }
      }}
    />
  </Row>;
}

// ───── TextField ─────────────────────────────────────────────────────────
export function TextField({ label, value, onCommit }) {
  const [local, setLocal] = useState(() => value ?? '');
  const editing = useRef(false);
  useEffect(() => { if (!editing.current) setLocal(value ?? ''); }, [value]);
  return <Row label={label}>
    <input
      type="text" style={INPUT_STYLE} value={local}
      onChange={(e) => { editing.current = true; setLocal(e.target.value); }}
      onBlur={() => { editing.current = false; onCommit(local); }}
      onKeyDown={(e) => {
        if (e.key === 'Enter') e.currentTarget.blur();
        else if (e.key === 'Escape') {
          editing.current = false; setLocal(value ?? ''); e.currentTarget.blur();
        }
      }}
    />
  </Row>;
}

// ───── SwitchField ───────────────────────────────────────────────────────
export function SwitchField({ label, checked, onChange }) {
  return <Row label={label}>
    <Switch size="small" checked={!!checked} onChange={onChange} />
  </Row>;
}

// ───── DropdownField ─────────────────────────────────────────────────────
// Single-select list. `value` is the current selection; clicking shows a
// menu of `options`; onChange is called with the new value.
export function DropdownField({ label, value, options, onChange }) {
  const menu = (
    <Menu onClick={(ev) => onChange(options[ev.key])}>
      {options.map((opt, idx) => <Menu.Item key={idx}>{opt}</Menu.Item>)}
    </Menu>
  );
  return <Row label={label}>
    <Dropdown overlay={menu} trigger={['click']}>
      <a href="#" onClick={(e) => e.preventDefault()} style={{
        display: 'inline-flex', alignItems: 'center',
        justifyContent: 'space-between',
        height: 22, padding: '0 8px', fontSize: 12,
        background: '#1565c0', color: 'white', borderRadius: 3,
        minWidth: 96, gap: 6,
      }}>
        <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
          {value ?? '—'}
        </span>
        <CaretDownOutlined />
      </a>
    </Dropdown>
  </Row>;
}

// ───── StepButton ────────────────────────────────────────────────────────
// Small button intended for tweak-button clusters (NumberTweakActions and
// shape-specific button groups). Inline style; no business logic.
export function StepButton({ onClick, title, children }) {
  return <button type="button" style={STEP_BTN_STYLE} onClick={onClick} title={title}>
    {children}
  </button>;
}

// ───── NumberTweakActions ────────────────────────────────────────────────
// The ×N / ÷N / +M / −M button cluster used in NumberField popovers (and
// anywhere else you need quick numeric nudging). Configurable:
//   mul  — array of multiplicative factors (renders ×N and ÷N buttons)
//   add  — array of additive deltas    (renders +M and −M buttons)
// Default pattern (`tweak: true` shorthand) covers the common "double/halve
// + nudge by 0.1 / 0.01" use case for size-like fields.
const DEFAULT_TWEAK = { mul: [2], add: [0.1, 0.01] };
export function NumberTweakActions({ value, onCommit, mul, add }) {
  const v = Number(value);
  const cleanV = Number.isFinite(v) ? v : 0;
  const m = mul ?? DEFAULT_TWEAK.mul;
  const a = add ?? DEFAULT_TWEAK.add;
  return <>
    {m.map((f) => <StepButton key={`x${f}`}
      onClick={() => onCommit(cleanV * f)} title={`× ${f}`}>×{f}</StepButton>)}
    {m.map((f) => <StepButton key={`/${f}`}
      onClick={() => onCommit(cleanV / f)} title={`÷ ${f}`}>÷{f}</StepButton>)}
    {a.map((d) => <StepButton key={`+${d}`}
      onClick={() => onCommit(cleanV + d)} title={`+ ${d}`}>+{d}</StepButton>)}
    {a.map((d) => <StepButton key={`-${d}`}
      onClick={() => onCommit(cleanV - d)} title={`− ${d}`}>−{d}</StepButton>)}
  </>;
}

// ───── RefSlot ───────────────────────────────────────────────────────────
// One reference-pick button: shows the current target shape's name/id and
// clicks call `onPick()` which the parent routes to ref-pick mode.
// `refEntry` is the {id, element} object from shape.ref[slotIdx].
// `shapeList` is consulted to resolve the referenced shape's display name.
export function RefSlot({ refEntry, shapeList, onPick }) {
  const resolved = refEntry && shapeList && shapeList.find((s) => s.id === refEntry.id);
  const label = resolved
    ? (resolved.name || `id ${resolved.id}`)
    : (refEntry && refEntry.id !== undefined ? `id ${refEntry.id}` : '(pick)');
  return <AntButton size="small" onClick={onPick} style={{
    fontSize: 11, height: 22, padding: '0 8px',
    maxWidth: '100%', overflow: 'hidden', textOverflow: 'ellipsis',
  }}>{label}</AntButton>;
}
