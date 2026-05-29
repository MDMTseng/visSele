# Test loop — round note 1 (FINAL, 0 remaining)

## Added (4 cases): output sanity (schema), combined caliper+CALC, combined determinism, caliper-on-alt-image determinism.

## Results: 79/79 PASS (full suite)
- golden_output_sanity => valid re-parseable JSON, has 'reports', NO 'nan'/'inf' tokens
  (confidence/uninit-struct fixes verified at the output level).
- combined_caliper_calc => exit0 ; determinism_combined / determinism_caliper_alt_img => identical.

## ===== 10-ROUND LOOP SUMMARY =====
Built `test_suite/suite.py` — black-box harness over `visSele --insp`. 79 cases, 0 failures.

Coverage by kind:
- determinism (8): default, caliper(line/arc), search-point caliper, CALC, combined, alt-image,
  5x-stress, degenerate — ALL byte-identical. (Enabled by the uninit-confidence fix.)
- golden_regression (1): committed baseline `expected/10221.json`.
- schema (1): output is clean valid JSON, no nan/inf.
- robust (69): missing/extra/wrong-type fields across line/arc/search_point/measure/aux_point,
  invalid/empty/wrong-schema JSON, CALC battery (div0, stack-underflow, cycles, missing refs,
  huge ids, float/neg literals, deep chains, long postfix), degenerate geometry, image/def file
  errors, value/limit edges.

Robustness posture observed:
- Memory-unsafe crash (SIGSEGV/SIGBUS): **0 across all 79 cases.**
- Malformed feature parse -> controlled SIGABRT (reload reject). Invalid/missing files -> exit3/exit4.
- measure/CALC evaluation -> graceful NA on bad/dangling/cyclic refs (no crash).

Validated this session's fixes end-to-end: determinism (confidence), buffer-overflow (overlong
name => no crash), required-field hardening (missing id/pt/subtype => controlled reject not segv),
cycle guard (CALC + aux cycles => no hang).

Known limitations (future work):
- "robust" asserts no-crash, not value-correctness (no ground-truth assertions on degenerate => NA).
- ANGLE/AREA subtype pt1.x deref remains unguarded in source (didn't SIGSEGV in tests, but latent).
- Single golden sample + single machine path; test data lives outside repo (HY_sync/DEV/test).
