# Test loop — round note 8 (8 rounds remaining)

## Added this round (9 cases): invalid/empty JSON, empty featureSet, duplicate ids, cyclic ref, inverted limits, zero margin, negative coords, caliper-variant determinism.

## Results: 32/32 PASS
Key findings:
- invalid_json / empty_string_def => exit4 (harness "null report", graceful — no SIGSEGV).
- empty_featureSet => controlled SIGABRT (reload rejects).
- duplicate_line_ids => exit0 (TreeExecution find-by-id takes first; no crash).
- **cyclic_aux_ref => exit0** — STATUS_NA in-progress cycle guard (caveat E9) holds; no infinite recursion/hang.
- inverted limits / zero margin / negative coords => exit0 (graceful pass/NA).
- **determinism_caliper => identical** — the most-reworked path (caliper line/arc) is byte-deterministic.

## Findings
- No SIGSEGV/SIGBUS/timeout across 32 cases. Robustness posture is solid.
- Cycle guard + dup-id handling confirmed by black box.
- Determinism confirmed on BOTH default (contour) and caliper paths.

## New cases to add next round (note 7) — focus: NUMERICAL degeneracy (wave-5 flagged div-by-zero in pure geometry)
- Degenerate arc: pt1==pt2==pt3 (coincident) -> convert3Pts2ArcData / CrossProduct_norm zero-vector; must not crash.
- Degenerate line: pt1==pt2 (zero line_vec -> acvVecNormalize /0); must not crash.
- measure(angle) with both refs = same line id.
- Wrong-type ref (measure ref id->line but type says search_point).
- Huge id (2147483648, int overflow).
- Forward-compat: unknown extra field on a line + unknown top-level key -> must not crash (ideally output unchanged).
