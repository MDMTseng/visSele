# Test loop — round note 10 (10 rounds remaining)

## What this round did
- Built the black-box suite (`test_suite/suite.py`) driving `visSele --insp` from `Core0_1/`.
- 3 test kinds: `determinism`, `golden_regression` (committed baseline `expected/10221.json`), `robust` (malformed def must not memory-crash).
- Initial 7 cases.

## Results: 7/7 PASS
| case | result | detail |
|---|---|---|
| determinism_golden | PASS | identical across 2 runs (confidence fix holds) |
| golden_regression | PASS | baseline created |
| line_missing_id | PASS | controlled SIGABRT (parse reject), no SIGSEGV |
| measure_missing_name | PASS | controlled SIGABRT |
| line_overlong_name (200 chars) | PASS | exit0 — buffer-overflow fix works (truncated) |
| measure_missing_subtype | PASS | controlled SIGABRT |
| line_missing_pt1 | PASS | controlled SIGABRT |

## Findings
- Determinism + buffer + required-field hardening from this session are all confirmed by black-box tests.
- Malformed defs => controlled SIGABRT (engine throws on reload failure; `--insp` doesn't catch). Acceptable (no memory corruption), but a truly graceful exit-code path would be nicer (engine-level, not test concern).
- Cosmetic bug in suite: `rc_str` prints `SIGSIGABRT` (double prefix) — fix next round.

## New cases to add next round (note 9)
- Fix `rc_str` label.
- arc: missing pt1/pt2/pt3/direction/margin.
- line: missing pt2; missing margin.
- search_point: missing angleDeg / ref / pt1 / width.
- measure: missing ref; missing value; ref to a non-existent feature id.
- Type-confusion: `id` as string, `margin` as string, `pt1` as number not object.
