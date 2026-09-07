// Kept-sample groups, configured on the inspection screen and remembered PER
// DEF FILE NAME in this browser's localStorage.
//
// The core holds the buffer and takes the group list over ST
// (INSP_SAMPLE_GROUPS); it does not remember it across defs, so the inspection
// screen pushes the stored list every time it starts inspecting -- an empty
// list when the def has none, which turns keeping OFF rather than leaving the
// previous def's rules running against a different recipe.
//
// localStorage, not machine_setting.json, on purpose: the groups are a
// question the operator is asking about THIS recipe right now ("show me ten
// where measure 10 failed"), not a machine property, and they should not ride
// along in a fleet-synced file.
//
// Shape of one group, exactly what the core parses (wiringPanel.cpp
// insp_sample_set_groups):
//   { name, cap, rotate, verdict: 'OK'|'NG'|'NA'|'*', measures: { "<judge id>": 'OK'|'NG'|'NA'|'*' } }
import { mkLog } from './logger';
const log = mkLog('ui.samplegroups');

const KEY_PREFIX = 'visSele.sampleGroups.v1:';
const keyOf = (defName) => KEY_PREFIX + String(defName || '');

export const SAMPLE_WANTS = ['OK', 'NG', 'NA', '*'];

export function loadSampleGroups(defName) {
  if (!defName) return [];
  try {
    const raw = window.localStorage.getItem(keyOf(defName));
    const v = raw ? JSON.parse(raw) : [];
    return Array.isArray(v) ? v.map(normaliseGroup) : [];
  } catch (e) { return []; }
}

export function saveSampleGroups(defName, groups) {
  if (!defName) return false;
  try {
    const clean = (groups || []).map(normaliseGroup);
    if (clean.length === 0) window.localStorage.removeItem(keyOf(defName));
    else window.localStorage.setItem(keyOf(defName), JSON.stringify(clean));
    return true;
  } catch (e) { log.warn('[samples] localStorage write failed', e); return false; }
}

export function normaliseGroup(g) {
  const o = g || {};
  const cap = Math.max(1, Math.min(200, Math.round(Number(o.cap) || 20)));
  const measures = {};
  Object.entries(o.measures || {}).forEach(([id, want]) => {
    if (SAMPLE_WANTS.includes(want) && want !== '*') measures[String(id)] = want;
  });
  return {
    name: String(o.name || '').trim() || 'group',
    cap,
    rotate: o.rotate === true,
    verdict: SAMPLE_WANTS.includes(o.verdict) ? o.verdict : '*',
    measures,
  };
}

// Send the list to the core. `sendBPG` is ACT_WS_SEND_CORE_BPG's signature.
// Returns a promise; a rejection is logged, not thrown -- the screen must not
// fail to start inspecting because a sample rule could not be delivered.
export function pushSampleGroups(sendBPG, groups) {
  const list = (groups || []).map(normaliseGroup);
  return new Promise((resolve) => {
    try {
      sendBPG('ST', 0, { INSP_SAMPLE_GROUPS: list }, undefined, {
        resolve: () => resolve(true),
        reject: (e) => { log.warn('[samples] ST INSP_SAMPLE_GROUPS rejected', e); resolve(false); },
      });
    } catch (e) { log.warn('[samples] ST INSP_SAMPLE_GROUPS failed', e); resolve(false); }
  });
}
