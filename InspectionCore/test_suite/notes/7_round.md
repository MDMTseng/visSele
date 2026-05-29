# Test loop — round note 7 (7 rounds remaining)

## Added this round (7 cases): numerical degeneracy + forward-compat.

## Results: 39/39 PASS
- arc_coincident_pts / arc_collinear_pts / line_zero_length / measure_angle_same_line => exit0.
  Degenerate geometry hits div-by-zero in pure helpers (acvVecNormalize, CrossProduct_norm,
  convert3Pts2ArcData) but produces NaN (no SIGFPE/crash on this platform). NaN flows to NA.
- line_huge_id (2^31) / line_unknown_field / unknown_toplevel_key => exit0 (forward-compat: ignored).

## Findings
- No crash on degenerate geometry; div-by-zero -> NaN -> NA (graceful, not a trap).
  LIMITATION: suite only asserts "no crash", not "degenerate => NA (not a bogus finite value)".
  A ground-truth assertion would need expected values.
- Forward-compat holds: unknown fields/keys ignored.

## Discovered gap (to probe next round)
- parse_judgeData ANGLE subtype derefs `*JFetEx_NUMBER(judge_obj,"pt1.x/.y")` UNCHECKED
  (line ~1747). My earlier hardening covered name/id/subtype but NOT per-subtype pt1.x.
  => a measure subtype=angle missing pt1 may SIGSEGV. Will test next round; if it crashes, FIX it.

## New cases to add next round (note 6) — focus: CALC (the extracted evaluator) + the angle gap
CALC schema confirmed: measure{subtype:"calc", calc_f:{exp:"", post_exp:["[12]","[13]","$+$"]}}.
- calc_sum [12]+[13]; calc_scalar [12]*2; calc_max via "$,$"+"max$".
- calc_div_zero [12]/0 ; calc_ref_missing [99999]+1 ; calc_malformed_postfix ["$+$"] (stack underflow!).
- calc_huge_ref_id [2147483647].
- measure_angle_missing_pt1 (probe the unhardened deref).
