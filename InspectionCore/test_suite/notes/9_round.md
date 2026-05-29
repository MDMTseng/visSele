# Test loop — round note 9 (9 rounds remaining)

## What this round did
- Fixed `rc_str` label bug (was `SIGSIGABRT`).
- Added 16 corner cases: arc/line/search_point missing required fields, measure missing/dangling refs, type-confusion (id/margin as string, pt1 as number).

## Results: 23/23 PASS
Highlights:
- All missing-required-field feature parses => controlled SIGABRT (no SIGSEGV). Hardening confirmed across arc/line/search_point.
- Type-confusion (`id`="not_a_number", `margin`="wide", `pt1`=5) => controlled SIGABRT (JSON_GET_NUM rejects non-number, my guards catch it).
- **measure path is GRACEFUL**: `measure_missing_ref`, `measure_missing_value`, `measure_ref_nonexistent` all => exit0 (engine returns NA, no crash). Different robustness posture than feature-parse (which rejects).

## Findings
- The required-field hardening covers arc/line/search_point uniformly.
- Measure/judge evaluation tolerates dangling refs (good — won't crash on partially-edited defs).
- No memory-unsafe crash in any of 23 cases.

## New cases to add next round (note 8)
- Totally invalid JSON def (cJSON_Parse fails) — must not SIGSEGV.
- Empty featureSet (group present, zero features).
- Duplicate feature ids (two lines same id) — TreeExecution find-by-id.
- Cyclic ref (aux_point referencing chain that loops) — exercises TreeExecution STATUS_NA cycle guard (caveat E9).
- **Caliper-variant determinism**: patch all line/circle -> locating:caliper, run twice, assert identical (determinism of the most-reworked path).
- Search-point caliper-variant determinism.
- Inverted limits (USL<LSL), margin=0, negative coords.
