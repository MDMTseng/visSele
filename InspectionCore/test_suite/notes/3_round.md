# Test loop — round note 3 (3 rounds remaining)

## Added (8 cases): CALC float/negative literals, empty name, name with quotes/backslash/tab,
## non-numeric value, USL==LSL, search_point width=0, negative margin.

## Results: 69/69 PASS
- All => exit0. CALC handles float ("3.14") and negative ("-5") literals.
- JSON-escape-heavy name (a"b\c\t) parses fine (cJSON handles escaping).
- Non-numeric value / zero tolerance band / zero width / negative margin => no crash.

## Findings
- Value/literal/limit edge inputs are all non-crashing.
- 69 cases total, still zero SIGSEGV/SIGBUS/timeout.

## New cases to add next round (note 2)
- determinism on degenerate geometry (arc coincident) — ensure NaN-producing path is still deterministic.
- CALC very long post_exp (~100 ops) — no crash, parser loop cap (1000) respected.
- calc_f not an object (string) — graceful.
- Valid JSON but wrong schema ({"hello":"world"}) — graceful reject.
- search_point ref to non-existent target id.
- aux_point missing ref.
