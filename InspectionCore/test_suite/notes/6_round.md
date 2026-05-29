# Test loop — round note 6 (6 rounds remaining)

## Added this round (10 cases): CALC evaluator battery + angle-deref probe + CALC determinism.

## Results: 49/49 PASS
- calc_sum/scalar/max/div_zero/ref_missing/malformed_postfix/huge_ref_id/empty_postexp => all exit0.
  The extracted JudgeCALC evaluator does NOT crash on: div-by-zero, stack-underflow (["$+$"]),
  ref to missing judge id, 2^31 ref id, empty post_exp. Robust.
- determinism_calc => identical (CALC path deterministic).
- measure_angle_missing_pt1 => controlled SIGABRT (NOT SIGSEGV). The unchecked `*JFetEx_NUMBER(
  judge_obj,"pt1.x")` in the angle branch does NOT cause a memory-unsafe crash in this scenario
  (def reload rejects first / JFetEx aborts controlled). Latent-but-not-exploited; noted.

## Findings
- JudgeCALC (the unit I extracted) is black-box robust to malformed expressions.
- No SIGSEGV anywhere in 49 cases.

## New cases to add next round (note 5)
- calc_self_ref (calc id references itself) -> CALC cycle guard.
- calc_mutual_cycle (two calc measures reference each other).
- calc_unsupported_fn (["[12]","sin$"]) -> functionExec_ returns -2 -> graceful NA.
- determinism_5x (run golden 5x, all identical — flakiness stress).
- duplicate_measure_ids.
- search_point caliper-variant determinism.
