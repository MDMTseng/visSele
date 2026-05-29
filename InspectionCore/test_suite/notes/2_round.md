# Test loop — round note 2 (2 rounds remaining)

## Added (6 cases): degenerate determinism, long CALC postfix (~200 ops), calc_f not object,
## wrong-schema JSON, search_point ref nonexistent, aux_point missing ref.

## Results: 75/75 PASS
- determinism_degenerate (coincident arc) => identical across runs (NaN path still deterministic).
- calc_long_postexp (201 tokens) => exit0 (no overflow of evaluator stack / parse loop).
- calc_f_not_object => exit0 (path-fetch fails -> empty post_exp -> NA).
- wrong_schema_def ({"hello":"world"}) => exit4 (graceful reject).
- sp_ref_nonexistent => exit0 ; aux_missing_ref => controlled SIGABRT.

## Findings
- Even degenerate (NaN-producing) outputs are deterministic — the determinism guarantee is robust.
- 75 cases, zero SIGSEGV.

## New cases to add final round (note 1)
- Output sanity (schema kind): golden output is valid re-parseable JSON, has 'reports',
  and contains NO literal 'nan'/'inf' tokens (validates the confidence/uninit fixes at output level).
- Combined caliper + a CALC measure together -> no crash + deterministic.
- Caliper variant on alternate image -> deterministic.
- Final full-suite regression gate.
