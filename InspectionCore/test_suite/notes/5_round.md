# Test loop — round note 5 (5 rounds remaining)

## Added (6 cases): CALC self/mutual cycles, unsupported fn, dup measure ids, 5x determinism, sp-caliper determinism.

## Results: 55/55 PASS
- calc_self_ref / calc_mutual_cycle => exit0 (cycle guard covers CALC ref cycles; no infinite recursion).
- calc_unsupported_fn (sin$) => exit0 (functionExec_ returns -2 -> NA, graceful).
- duplicate_measure_ids => exit0.
- determinism_5x => 5 runs byte-identical (no flakiness).
- determinism_sp_caliper => identical (search-point caliper path deterministic).

## Findings
- Determinism is robust under repetition (5x) and across all three measurement paths
  (default/contour, line/arc caliper, search-point caliper).
- No SIGSEGV in 55 cases total.

## New cases to add next round (note 4) — focus: IMAGE-side + ref-type robustness
- Missing image file -> graceful exit3.
- Missing/unreadable def file -> must not SIGSEGV.
- Alternate image (the non-_bk 10221 SORTING.png) + golden def -> no crash + deterministic.
- CALC referencing a non-measure feature id (e.g. a line id) -> NA (judge_CALC only scans judgeReports).
- CALC deep chain (calc -> calc -> distance), 3 levels.
