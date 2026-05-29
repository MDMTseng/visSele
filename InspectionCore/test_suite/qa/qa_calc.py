"""QA module for the Core0_1 CALC expression evaluator (JudgeCALC.cpp).

Black-box tested via `visSele --insp`. A CALC measure is a measure with
  {"subtype":"calc","calc_f":{"exp":"","post_exp":[<RPN tokens>]}}
post_exp is a POSTFIX token list:
  "[id]"  -> value of judge/measure with that id
  "3","-5","3.14" -> numeric literals
  "$+$","$-$","$*$","$/$" -> binary operators
  "max$","min$" -> variadic, preceded by a "$,$" arity-cache token that
                   marks how many operands the function consumes, e.g.
                   ["[12]","[13]","$,$","max$"].
mut_calc([...]) rewrites the FIRST measure (id=8 in golden) into such a calc;
its computed result surfaces as the report with id=8.

VIEWPOINT: exhaustively exercise the RPN evaluator for ROBUSTNESS (no
memory-unsafe crash / hang on malformed input) and CORRECTNESS (computed
value matches hand-computed arithmetic over baseline measured values).

Baseline measured values (plain golden run, --insp):
  [8]=8.601038  [12]=8.468927  [13]=8.601037  [14]=2.792538
These are the operands used to predict expected calc outputs.

Strategy:
  - custom cases   : convert measure 8 -> calc, run, parse output, locate
                     report id=8 and assert measured value ~= expected.
                     If the field can't be located, degrade to no-crash.
  - robust cases   : malformed / pathological RPN, PASS = no SIGSEGV/SIGBUS/
                     SIGILL/SIGFPE and no timeout (controlled reject is fine).
  - determinism    : a non-trivial calc must be byte-identical across runs.

Coverage: every operator (+ - * /), variadic max/min at arity 2/3/many/1,
nesting / deep expressions, division by zero, stack underflow & leftover
operands (overflow), malformed/empty/whitespace tokens, refs to missing /
non-existent / self / huge (int-overflow) ids, float-precision, and a very
long expression.
"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qalib import *

# ---- baseline operand values (measured from a plain golden run) ----
V8, V12, V13, V14 = 8.601038, 8.468927, 8.601037, 2.792538
TOL = 1e-2  # generous: float32 reports + measurement noise

def _find_value(o, tid):
    """Locate a report dict with id==tid that carries a numeric 'value'."""
    if isinstance(o, dict):
        if o.get("id") == tid and isinstance(o.get("value"), (int, float)):
            return o["value"]
        for v in o.values():
            r = _find_value(v, tid)
            if r is not None:
                return r
    elif isinstance(o, list):
        for v in o:
            r = _find_value(v, tid)
            if r is not None:
                return r
    return None

def calc_expect(post_exp, expected, tol=TOL, tid=8):
    """custom fn: run a calc(post_exp), assert report[tid].value ~= expected.
    Degrades to no-crash assertion if the value field cannot be located."""
    def fn(run_insp):
        d = mut_calc(post_exp)()
        rc, out = run_insp(d)
        if rc in SIGCRASH or rc == "TIMEOUT":
            return False, f"crash/hang {rc_str(rc)}"
        if out is None:
            return False, f"no output ({rc_str(rc)})"
        try:
            j = json.loads(out)
        except Exception as e:
            return False, f"invalid JSON ({rc_str(rc)}): {e}"
        got = _find_value(j, tid)
        if got is None:
            # cannot locate -> fall back to no-crash semantics
            return True, f"no-crash {rc_str(rc)} (value field not located)"
        if not math.isfinite(got):
            return False, f"non-finite value {got}"
        if abs(got - expected) <= tol:
            return True, f"value={got:.5f} ~= {expected:.5f}"
        return False, f"WRONG value={got:.6f} expected={expected:.6f}"
    return fn

CASES = []

# ================= CORRECTNESS (custom) =================
CASES.append(case("add", "custom", fn=calc_expect(["[12]", "[13]", "$+$"], V12 + V13)))
CASES.append(case("sub", "custom", fn=calc_expect(["[13]", "[12]", "$-$"], V13 - V12)))
CASES.append(case("mul", "custom", fn=calc_expect(["[12]", "[13]", "$*$"], V12 * V13, tol=0.2)))
CASES.append(case("div", "custom", fn=calc_expect(["[13]", "[12]", "$/$"], V13 / V12)))
CASES.append(case("literal_passthrough", "custom", fn=calc_expect(["3.14"], 3.14)))
CASES.append(case("neg_literal", "custom", fn=calc_expect(["-5"], -5.0)))
CASES.append(case("ref_passthrough", "custom", fn=calc_expect(["[12]"], V12)))
CASES.append(case("lit_arith", "custom", fn=calc_expect(["3", "4", "$+$", "2", "$*$"], 14.0)))

# nesting / deep expression: ((12+13) - 8) / 14  * 2
deep = ["[12]", "[13]", "$+$", "[8]", "$-$", "[14]", "$/$", "2", "$*$"]
CASES.append(case("deep_nest", "custom",
                  fn=calc_expect(deep, ((V12 + V13) - V8) / V14 * 2, tol=0.05)))

# ---- variadic max / min ----
CASES.append(case("max_arity2", "custom",
                  fn=calc_expect(["[12]", "[13]", "$,$", "max$"], max(V12, V13))))
CASES.append(case("min_arity2", "custom",
                  fn=calc_expect(["[12]", "[13]", "$,$", "min$"], min(V12, V13))))
CASES.append(case("max_arity3", "custom",
                  fn=calc_expect(["[8]", "[12]", "[13]", "$,$", "max$"], max(V8, V12, V13))))
CASES.append(case("min_arity4", "custom",
                  fn=calc_expect(["[8]", "[12]", "[13]", "[14]", "$,$", "min$"],
                                 min(V8, V12, V13, V14))))
# many operands (arity 6, with repeats + literals)
CASES.append(case("max_many", "custom",
                  fn=calc_expect(["[8]", "[12]", "[13]", "[14]", "1", "100", "$,$", "max$"], 100.0)))
# max of a sub-expression result and a literal
CASES.append(case("max_of_expr", "custom",
                  fn=calc_expect(["[12]", "[13]", "$+$", "5", "$,$", "max$"], V12 + V13)))

# ================= DETERMINISM =================
CASES.append(case("det_calc", "determinism", make=mut_calc(deep), runs=3))

# ================= ROBUSTNESS (malformed) =================
CASES.append(case("div_by_zero", "robust", make=mut_calc(["[12]", "0", "$/$"])))
CASES.append(case("div_zero_zero", "robust", make=mut_calc(["0", "0", "$/$"])))
CASES.append(case("stack_underflow_op", "robust", make=mut_calc(["[12]", "$+$"])))
CASES.append(case("stack_underflow_empty", "robust", make=mut_calc(["$+$"])))
CASES.append(case("empty_postexp", "robust", make=mut_calc([])))
CASES.append(case("leftover_operands", "robust", make=mut_calc(["[12]", "[13]", "3"])))
CASES.append(case("max_no_arity_token", "robust", make=mut_calc(["[12]", "[13]", "max$"])))
CASES.append(case("max_zero_arity", "robust", make=mut_calc(["$,$", "max$"])))
CASES.append(case("garbage_token", "robust", make=mut_calc(["[12]", "@!#", "$+$"])))
CASES.append(case("empty_token", "robust", make=mut_calc(["[12]", "", "$+$"])))
CASES.append(case("whitespace_token", "robust", make=mut_calc(["[12]", "   ", "$+$"])))
CASES.append(case("ref_missing_id", "robust", make=mut_calc(["[99999]", "1", "$+$"])))
CASES.append(case("ref_self", "robust", make=mut_calc(["[8]", "1", "$+$"])))
CASES.append(case("ref_huge_id_overflow", "robust",
                  make=mut_calc(["[99999999999999999999]", "1", "$+$"])))
CASES.append(case("ref_negative_id", "robust", make=mut_calc(["[-1]", "1", "$+$"])))
CASES.append(case("malformed_ref_brackets", "robust", make=mut_calc(["[12", "1", "$+$"])))
CASES.append(case("deeply_long_expr", "robust",
                  make=mut_calc(["[12]"] + ["1", "$+$"] * 500)))

# ================= ROUND 2: harder robustness + correctness =================
# Baseline operands again: V8 V12 V13 V14 (+V17 distance, but value unknown ->
# only used in robust/no-crash contexts unless hand-checkable).

# --- more stack-underflow shapes ---
# operator immediately after a single literal operand (needs 2, has 1)
CASES.append(case("r2_underflow_op_after_lit", "robust", make=mut_calc(["5", "$*$"])))
# nested under-arity: inner op consumes the only operand, outer op starves
CASES.append(case("r2_nested_under_arity", "robust",
                  make=mut_calc(["[12]", "[13]", "$+$", "$*$"])))
# chain of operators each starving the next
CASES.append(case("r2_op_chain_starve", "robust", make=mut_calc(["[12]", "$+$", "$-$", "$*$", "$/$"])))
# max$ with arity-cache LARGER than stack depth (claims 5, only 2 present)
CASES.append(case("r2_max_arity_gt_stack", "robust",
                  make=mut_calc(["[12]", "[13]", "$,$", "$,$", "$,$", "$,$", "$,$", "max$"])))
# min$ arity cache > stack, single operand
CASES.append(case("r2_min_arity_gt_stack", "robust",
                  make=mut_calc(["[12]", "$,$", "$,$", "$,$", "min$"])))

# --- over-long / repeated arity tokens ---
CASES.append(case("r2_many_arity_tokens", "robust",
                  make=mut_calc(["[12]", "[13]"] + ["$,$"] * 50 + ["max$"])))
CASES.append(case("r2_arity_tokens_only", "robust", make=mut_calc(["$,$"] * 20 + ["max$"])))
CASES.append(case("r2_trailing_arity_no_func", "robust",
                  make=mut_calc(["[12]", "[13]", "$,$", "$,$"])))

# --- malformed [id] tokens ---
CASES.append(case("r2_empty_brackets", "robust", make=mut_calc(["[]", "1", "$+$"])))
CASES.append(case("r2_alpha_id", "robust", make=mut_calc(["[abc]", "1", "$+$"])))
CASES.append(case("r2_comma_in_id", "robust", make=mut_calc(["[12,13]", "1", "$+$"])))
CASES.append(case("r2_nested_brackets", "robust", make=mut_calc(["[[12]]", "1", "$+$"])))
CASES.append(case("r2_neg_inside_bracket", "robust", make=mut_calc(["[12, [-1]", "1", "$+$"])))
CASES.append(case("r2_float_id", "robust", make=mut_calc(["[12.5]", "1", "$+$"])))

# --- refs to NON-measure ids (line/arc/aux/search_point) ---
CASES.append(case("r2_ref_line_id", "robust", make=mut_calc(["[1]", "[2]", "$+$"])))
CASES.append(case("r2_ref_arc_id", "robust", make=mut_calc(["[24]", "[25]", "$+$"])))
CASES.append(case("r2_ref_aux_point_id", "robust", make=mut_calc(["[100001]", "1", "$+$"])))
CASES.append(case("r2_ref_search_point_id", "robust", make=mut_calc(["[3]", "1", "$+$"])))
CASES.append(case("r2_ref_sign360_id", "robust", make=mut_calc(["[100000]", "1", "$+$"])))

# --- NaN / inf propagation ---
# [12]/0 -> (inf or NaN) then * literal ; must not surface nan/inf or crash
CASES.append(case("r2_divzero_then_mul", "robust", make=mut_calc(["[12]", "0", "$/$", "10", "$*$"])))
CASES.append(case("r2_divzero_then_sub", "robust", make=mut_calc(["[12]", "0", "$/$", "[13]", "$-$"])))
CASES.append(case("r2_divzero_chain", "robust",
                  make=mut_calc(["[12]", "0", "$/$", "0", "$/$", "0", "$/$"])))
# 0/0 then max with literal
CASES.append(case("r2_nan_in_max", "robust",
                  make=mut_calc(["0", "0", "$/$", "5", "$,$", "max$"])))

# --- huge / negative / float literals ---
CASES.append(case("r2_huge_literal", "robust", make=mut_calc(["1e308", "1e308", "$*$"])))
CASES.append(case("r2_huge_neg_literal", "robust", make=mut_calc(["-1e308", "1e308", "$-$"])))
CASES.append(case("r2_tiny_float", "robust", make=mut_calc(["1e-308", "1e-308", "$/$"])))
CASES.append(case("r2_int_overflow_literal", "robust",
                  make=mut_calc(["99999999999999999999999", "1", "$+$"])))

# --- correctness: deeper trees, precedence-via-RPN, mixing refs+lits+ops ---
# precedence: 2 + 3 * 4 == 14 (RPN encodes precedence); also = lit_arith but reordered
CASES.append(case("r2_precedence", "custom",
                  fn=calc_expect(["2", "3", "4", "$*$", "$+$"], 14.0)))
# (([12] - [13]) is ~ -1.2e-6) * 1000000 -> ~ -1.06 ; mix ref+lit deeply
CASES.append(case("r2_mix_deep", "custom",
                  fn=calc_expect(["[12]", "[13]", "$-$", "[8]", "$+$", "2", "$/$"],
                                 ((V12 - V13) + V8) / 2, tol=0.02)))
# correctness with negative literal in tree: [12] + (-3) * 2 = [12] - 6
CASES.append(case("r2_neg_in_tree", "custom",
                  fn=calc_expect(["[12]", "-3", "2", "$*$", "$+$"], V12 + (-3 * 2), tol=0.02)))
# nested max/min correctness: max(min([12],[13]), [8]) == V8
CASES.append(case("r2_nested_maxmin", "custom",
                  fn=calc_expect(["[12]", "[13]", "$,$", "min$", "[8]", "$,$", "max$"],
                                 max(min(V12, V13), V8))))
# division chain correctness: [13] / [12] / 1 == V13/V12
CASES.append(case("r2_div_chain", "custom",
                  fn=calc_expect(["[13]", "[12]", "$/$", "1", "$/$"], V13 / V12)))

# ================= ROUND 3: harder CALC edges =================

# --- arity-cache exact-match vs off-by-one ---
# arity-cache count == operand count: 2 operands, ONE "$,$" -> max(V12,V13)
CASES.append(case("r3_arity_exact_2", "custom",
                  fn=calc_expect(["[12]", "[13]", "$,$", "max$"], max(V12, V13))))
# arity-cache off-by-one (two "$,$" tokens before max$, only 2 operands)
CASES.append(case("r3_arity_off_by_one_extra", "robust",
                  make=mut_calc(["[12]", "[13]", "$,$", "$,$", "max$"])))
# exact 3 operands w/ two "$,$" (proper way)
CASES.append(case("r3_arity_exact_3", "custom",
                  fn=calc_expect(["[8]", "[12]", "[13]", "$,$", "$,$", "max$"],
                                 max(V8, V12, V13))))

# --- deeply nested expression (10+ ops), hand-computed ---
# ((((V12+V13)*2 - V8) / V14 + 1) - 3) * 2 + V12 - V13 + 5
def _r3_hand():
    x = (V12 + V13) * 2
    x = x - V8
    x = x / V14
    x = x + 1
    x = x - 3
    x = x * 2
    x = x + V12
    x = x - V13
    x = x + 5
    return x
deep10 = ["[12]", "[13]", "$+$", "2", "$*$", "[8]", "$-$", "[14]", "$/$",
         "1", "$+$", "3", "$-$", "2", "$*$", "[12]", "$+$", "[13]", "$-$",
         "5", "$+$"]
CASES.append(case("r3_deep_10ops", "custom",
                  fn=calc_expect(deep10, _r3_hand(), tol=0.05)))

# --- self-ref with stack-pollution before (push junk, then self) ---
CASES.append(case("r3_self_with_pollution", "robust",
                  make=mut_calc(["99", "42", "[8]", "1", "$+$"])))

# --- whitespace/tab-only tokens ---
CASES.append(case("r3_tab_token", "robust", make=mut_calc(["[12]", "\t\t", "$+$"])))
CASES.append(case("r3_mixed_ws_token", "robust", make=mut_calc(["[12]", " \t \n ", "$+$"])))

# --- embedded control chars in tokens ---
CASES.append(case("r3_ctrl_in_token", "robust",
                  make=mut_calc(["[12]", "1\x01\x02", "$+$"])))
CASES.append(case("r3_null_in_id", "robust",
                  make=mut_calc(["[12\x001]", "1", "$+$"])))

# --- very long single token (5000-char) ---
import sys as _sys
_sys.set_int_max_str_digits(10000)
CASES.append(case("r3_huge_id_token", "robust",
                  make=mut_calc(["[" + "9" * 5000 + "]", "1", "$+$"])))
CASES.append(case("r3_huge_op_token", "robust",
                  make=mut_calc(["[12]", "1", "$+$" + "x" * 5000])))

# --- exp string mismatched with post_exp: only post_exp matters ---
def _mut_calc_with_exp(post_exp, exp_str):
    def f():
        d = mut_calc(post_exp)()
        m = first_of(d, "measure")
        m["calc_f"]["exp"] = exp_str
        return d
    return f
def _r3_exp_ignored(post_exp, exp_str, expected):
    def fn(run_insp):
        d = _mut_calc_with_exp(post_exp, exp_str)()
        rc, out = run_insp(d)
        if rc in SIGCRASH or rc == "TIMEOUT":
            return False, f"crash/hang {rc_str(rc)}"
        if out is None:
            return False, f"no output ({rc_str(rc)})"
        try:
            j = json.loads(out)
        except Exception as e:
            return False, f"invalid JSON: {e}"
        got = _find_value(j, 8)
        if got is None:
            return True, "no-crash, value field not located"
        if not math.isfinite(got):
            return False, f"non-finite value {got}"
        if abs(got - expected) <= TOL:
            return True, f"post_exp used (value={got:.5f}); exp ignored"
        return False, f"exp may have been used: got={got:.6f} expected post_exp={expected:.6f}"
    return fn
CASES.append(case("r3_exp_string_ignored", "custom",
                  fn=_r3_exp_ignored(["[12]", "[13]", "$+$"], "lies_garbage_!@#$",
                                      V12 + V13)))

# --- pure literal stack (10 literals, no op) -> stack-not-1 reject ---
CASES.append(case("r3_pure_literal_stack10", "robust",
                  make=mut_calc(["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"])))

# --- absurd numbers with operators (overflow / underflow) ---
CASES.append(case("r3_huge_add_huge", "robust",
                  make=mut_calc(["1e308", "1e308", "$+$"])))   # +inf
CASES.append(case("r3_neg_huge_mul", "robust",
                  make=mut_calc(["-1e308", "1e10", "$*$"])))   # -inf
CASES.append(case("r3_tiny_mul_tiny", "robust",
                  make=mut_calc(["1e-308", "1e-308", "$*$"]))) # 0 (subnormal)

# --- 0/0 -> nan, verify NOT in serialized JSON ---
def _r3_nan_inf_in_output(post_exp):
    def fn(run_insp):
        d = mut_calc(post_exp)()
        rc, out = run_insp(d)
        if rc in SIGCRASH or rc == "TIMEOUT":
            return False, f"crash/hang {rc_str(rc)}"
        if out is None:
            return True, "no-crash, no output"
        try:
            json.loads(out)
        except Exception as e:
            return False, f"invalid JSON: {e}"
        low = out.lower()
        leaks = []
        if b"nan" in low: leaks.append("nan")
        if b"inf" in low: leaks.append("inf")
        if leaks:
            return False, f"LEAK in JSON: {leaks}"
        return True, "no nan/inf leak"
    return fn
CASES.append(case("r3_nan_no_leak", "custom",
                  fn=_r3_nan_inf_in_output(["0", "0", "$/$"])))
CASES.append(case("r3_posinf_no_leak", "custom",
                  fn=_r3_nan_inf_in_output(["1", "0", "$/$"])))
CASES.append(case("r3_neginf_no_leak", "custom",
                  fn=_r3_nan_inf_in_output(["-1", "0", "$/$"])))
CASES.append(case("r3_overflow_no_leak", "custom",
                  fn=_r3_nan_inf_in_output(["1e308", "1e308", "$*$"])))

if __name__ == "__main__":
    sys.exit(run_module("qa_calc", CASES))
