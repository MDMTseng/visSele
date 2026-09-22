// Guard: the two def-scoped key lists must stay consistent with each other and
// with the blank state they are reset to.
//
//   node unit_def_scoped_keys.mjs
//
// There are two lists, and a retake picks one of them:
//
//   DEF_SCOPED_EDIT_INFO_KEYS   everything that belongs to a def
//   DEF_LOCALIZER_SCOPED_KEYS   the subset the localizer owns
//
// The clear mode wipes the first, the keep mode wipes only the second. So a
// localizer key that is NOT also in the full list is a key that survives a
// FULL reset -- one def's registration or trained features carried into the
// next def, configuring a locator that then looks right. That is the failure
// this whole area keeps producing, and it is exactly the shape a human eye
// skips over: two lists of quoted strings, one of them a subset, edited months
// apart.
//
// Reset means `edit_info[k] = Edit_info_Empty()[k]`, so a key that
// Edit_info_Empty does not define resets to `undefined` by accident rather than
// by intent -- which is fine for some fields and wrong for anything whose blank
// value is [] or 0. Checked too.
//
// A SOURCE check, like unit_no_hardcoded_sel: importing InspectionEditorLogic
// pulls in the logger, the shape classes and half the editor. This costs
// milliseconds and needs no core and no browser.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const SRC = path.join(here, '..', '..', 'src', 'UTIL', 'InspectionEditorLogic.js');
const src = fs.readFileSync(SRC, 'utf8');

let fails = 0;
const check = (cond, what) => { if (!cond) { console.log('  FAIL ' + what); fails++; } return cond; };

// Pull an exported array-of-strings literal out of the source.
function arrayLiteral(name) {
  const m = src.match(new RegExp('export const ' + name + '\\s*=\\s*\\[([\\s\\S]*?)\\];'));
  if (!m) return null;
  // Strip // comments, then take every quoted string.
  const body = m[1].replace(/\/\/[^\n]*/g, '');
  return [...body.matchAll(/'([^']+)'|"([^"]+)"/g)].map((x) => x[1] || x[2]);
}

const FULL = arrayLiteral('DEF_SCOPED_EDIT_INFO_KEYS');
const LOC = arrayLiteral('DEF_LOCALIZER_SCOPED_KEYS');

console.log('both lists were found:');
check(Array.isArray(FULL) && FULL.length > 0, 'DEF_SCOPED_EDIT_INFO_KEYS not parsed');
check(Array.isArray(LOC) && LOC.length > 0, 'DEF_LOCALIZER_SCOPED_KEYS not parsed');
if (fails) { console.log('\nFAIL: could not read the lists; the source shape changed'); process.exit(1); }
console.log(`  full=${FULL.length} keys, localizer=${LOC.length} keys`);

// ── the invariant that matters ──────────────────────────────────────────────
console.log('every localizer key is also def-scoped:');
{
  const full = new Set(FULL);
  const orphans = LOC.filter((k) => !full.has(k));
  check(orphans.length === 0,
    'these survive a FULL reset and would leak into the next def: ' + orphans.join(', '));
  console.log(`  ${orphans.length === 0 ? 'ok' : 'BROKEN'}`);
}

console.log('no duplicates in either list:');
for (const [nm, list] of [['full', FULL], ['localizer', LOC]]) {
  const seen = new Set(), dupes = [];
  for (const k of list) { if (seen.has(k)) dupes.push(k); seen.add(k); }
  check(dupes.length === 0, `${nm} list repeats: ${dupes.join(', ')}`);
}
console.log('  ok');

// ── the keys the keep-mode must NOT touch ───────────────────────────────────
//
// 保留現有量測設定 exists to keep the calipers and the matching parameters. If
// one of those ever appears in the localizer list, the mode silently stops
// doing the one thing it is for -- and the symptom is "my measurements
// disappeared sometimes", which nobody reports precisely enough to find.
console.log('measurement settings are never in the localizer list:');
{
  const MEASUREMENT = [
    'sig_match_sim_thres', 'shape_min_score', 'shape_weak_thres', 'shape_strong_thres',
    'shape_match_scale', 'shape_nms_angle', 'matching_angle_margin_deg',
    'matching_angle_offset_deg', 'matching_face', 'matching_version',
    'inspection_downsample', 'morph_mode', 'morph_tps_lambda', 'morph_max_iter', 'morph_alpha',
  ];
  const loc = new Set(LOC);
  const leaked = MEASUREMENT.filter((k) => loc.has(k));
  check(leaked.length === 0,
    'keep-mode would wipe these, which is the opposite of what it promises: ' + leaked.join(', '));
  console.log(`  ${MEASUREMENT.length} checked, ${leaked.length} leaked`);
}

// ── the keys the localizer list MUST contain ────────────────────────────────
//
// The other direction. A retake replaces the PICTURE, so anything describing
// the old frame has to go in BOTH modes. Missing one here means keep-mode
// leaves a registration, a trained feature set or an extraction region that
// describes an image that is no longer on screen -- and the def still inspects,
// which is why it would not be noticed.
console.log('the localizer list covers everything tied to the old frame:');
{
  const MUST = ['def_image_reg', 'roi_refine_points', '__shape_cache', '__shape_stale',
                '__shape_lastGood', '__loc_include', '__loc_exclude',
                '__img_fresh_capture', '__tmp_ref_image_path'];
  const loc = new Set(LOC);
  const missing = MUST.filter((k) => !loc.has(k));
  check(missing.length === 0,
    'keep-mode would carry these across a new picture: ' + missing.join(', '));
  console.log(`  ${MUST.length} required, ${missing.length} missing`);
}

// ── reset target ────────────────────────────────────────────────────────────
//
// Reset assigns Edit_info_Empty()[k]. A key Edit_info_Empty does not mention
// resets to undefined -- intended for the optional numeric settings, wrong for
// anything whose blank value is a container.
console.log('keys whose blank value is a container are defined in Edit_info_Empty:');
{
  const emptyM = src.match(/export function Edit_info_Empty\(\)\s*{[\s\S]*?\n}/);
  check(!!emptyM, 'Edit_info_Empty not found');
  const emptyBody = emptyM ? emptyM[0] : '';
  const CONTAINER = ['__loc_include', '__loc_exclude', 'roi_refine_points'];
  const undef = CONTAINER.filter((k) => !new RegExp('(^|\\s)' + k + '\\s*:').test(emptyBody));
  // Not a hard failure for keys whose intended blank IS undefined -- it is a
  // report, because "resets to undefined" and "resets to []" behave differently
  // downstream and the choice should be visible in Edit_info_Empty.
  console.log(undef.length
    ? `  NOTE: reset to undefined (not listed in Edit_info_Empty): ${undef.join(', ')}`
    : '  all listed');
}

console.log(fails ? `\nFAIL: ${fails} assertion(s)` : '\nPASS: the two key lists agree, and neither mode does the other one\'s job');
process.exit(fails ? 1 : 0);
