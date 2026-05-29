# Test loop — round note 4 (4 rounds remaining)

## Added (6 cases): missing image, missing def, alt image, CALC ref-to-line, CALC deep chain, alt-image determinism.

## Results: 61/61 PASS
- img_missing => exit3 (graceful "cannot load image").
- def_missing => exit4 (graceful null report; no crash on unreadable def).
- img_alt_nonbk (non-_bk image) => exit0 + deterministic across runs.
- calc_ref_to_line_id ([1]=a line) => exit0 (judge_CALC scans only judgeReports -> NA).
- calc_deep_chain (calc->calc->distance) => exit0.

## Findings
- Image/def file errors handled with clean exit codes (3/4), no crash.
- Determinism holds on a second (alternate) image too.
- CALC multi-level chains and cross-type refs are safe.

## New cases to add next round (note 3) — focus: value/literal/limit edges
- CALC float literal (["[12]","3.14","$*$"]) and negative literal (["[12]","-5","$+$"]).
- Empty name ("") ; name with embedded quotes/backslash (JSON-escape robustness).
- measure value as non-numeric string ("abc") -> JfetchStrNUM NaN path.
- USL==LSL (zero tolerance band).
- search_point width=0 ; negative margin.
