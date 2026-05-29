"""Shared QA harness for Core0_1 `visSele --insp` black-box tests.

Import in a qa module:
    from qalib import *
    CASES = []
    CASES.append(case("my_test", "robust", make=mut_del("line","id")))
    if __name__ == "__main__":
        import sys; sys.exit(run_module("qa_<viewpoint>", CASES))

Kinds:
  robust      : run a (possibly malformed) def, PASS = no memory-unsafe crash
                (no SIGSEGV/SIGBUS/SIGILL/SIGFPE) and no timeout. Controlled
                SIGABRT / exit3 / exit4 are acceptable.
  determinism : run the def N (default 2) times, PASS = all exit0 AND byte-identical.
  schema      : run golden, PASS = output is valid JSON, has 'reports', no nan/inf.
  expect_rc   : run the def, PASS = returncode == c['rc'].
  custom      : c['fn'](run_insp) -> (ok:bool, detail:str)
"""
import json, os, subprocess, signal, copy

ROOT  = "/Users/mdm/workspace/visSele/InspectionCore"
BUILD = ROOT + "/build/mac-arm64"
CORE  = ROOT + "/Core0_1"
VIS   = BUILD + "/visSele"
TEST  = "/Users/mdm/workspace/HY_sync/DEV/test"
IMG   = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.png"
ALT_IMG = TEST + "/10221 BOS-LT12BH4211 SORTING.png"
GDEF  = TEST + "/10221 BOS-LT12BH4211 SORTING_bk.hydef"
TMP   = "/tmp/qa"
os.makedirs(TMP, exist_ok=True)

SIGCRASH = {-signal.SIGSEGV, -signal.SIGBUS, -signal.SIGILL, -signal.SIGFPE}

def run_insp(def_path, out_path=None, img=IMG, timeout=120):
    """def_path may be a path (str) or a dict (written to tmp). Returns (rc, out_bytes|None)."""
    if isinstance(def_path, dict):
        p = f"{TMP}/_def_{abs(hash(json.dumps(def_path,sort_keys=True)))%10**8}.hydef"
        json.dump(def_path, open(p, "w")); def_path = p
    if out_path is None:
        out_path = f"{TMP}/_out.json"
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    try:
        r = subprocess.run([VIS, "--insp", img, def_path, out_path],
                           cwd=CORE, env=env, capture_output=True, timeout=timeout)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return ("TIMEOUT", None)
    out = open(out_path, "rb").read() if os.path.exists(out_path) else None
    return (rc, out)

def golden():
    return json.load(open(GDEF))

def first_of(o, ftype):
    if isinstance(o, dict):
        if o.get("type") == ftype: return o
        for v in o.values():
            r = first_of(v, ftype)
            if r is not None: return r
    elif isinstance(o, list):
        for v in o:
            r = first_of(v, ftype)
            if r is not None: return r
    return None

def all_of(o, ftype, acc=None):
    if acc is None: acc = []
    if isinstance(o, dict):
        if o.get("type") == ftype: acc.append(o)
        for v in o.values(): all_of(v, ftype, acc)
    elif isinstance(o, list):
        for v in o: all_of(v, ftype, acc)
    return acc

def mut_del(ftype, key):
    def f():
        d = golden(); t = first_of(d, ftype)
        if t: t.pop(key, None)
        return d
    return f

def mut_set(ftype, key, val):
    def f():
        d = golden(); t = first_of(d, ftype)
        if t: t[key] = val
        return d
    return f

def mut_many(ftype, kv):
    def f():
        d = golden(); t = first_of(d, ftype)
        if t:
            for k, v in kv.items(): t[k] = v
        return d
    return f

import re as _re
def mut_calc(post_exp):
    """Convert first measure to subtype=calc. NOTE: the engine gates calc evaluation
    on a non-empty `ref` listing referenced judge ids, so we derive ref from the
    [id] tokens in post_exp (falling back to a dummy ref to id 12 if none), mirroring
    real-world calc defs (which carry both calc_f.post_exp AND ref)."""
    def f():
        d = golden(); m = first_of(d, "measure")
        ids = []
        for tok in post_exp:
            mt = _re.match(r"^\[(\d+)\]$", str(tok))
            if mt: ids.append(int(mt.group(1)))
        if not ids: ids = [12]                      # dummy ref so the calc judge is evaluated
        m["subtype"] = "calc"
        m["ref"] = [{"id": i} for i in ids]
        m["calc_f"] = {"exp": "", "post_exp": list(post_exp)}
        return d
    return f

def case(name, kind, **kw):
    return dict(name=name, kind=kind, **kw)

def rc_str(rc):
    if rc == "TIMEOUT": return "TIMEOUT"
    if isinstance(rc, int) and rc < 0:
        try: return signal.Signals(-rc).name
        except Exception: return f"signal{-rc}"
    return f"exit{rc}"

def _eval(c):
    kind = c["kind"]
    if kind == "robust":
        dp = c["raw_path"] if "raw_path" in c else (c["make"]() if "make" in c else GDEF)
        if "raw" in c:
            p = f"{TMP}/raw_{c['name']}.hydef"; open(p, "w").write(c["raw"]); dp = p
        rc, out = run_insp(dp, img=c.get("img", IMG))
        ok = (rc not in SIGCRASH and rc != "TIMEOUT")
        return ok, rc_str(rc) + (" (controlled-reject)" if rc == -signal.SIGABRT else "")
    if kind == "determinism":
        dp = c["make"]() if "make" in c else GDEF
        runs = c.get("runs", 2); outs, rcs = [], []
        for k in range(runs):
            rc, o = run_insp(dp, f"{TMP}/det_{c['name']}_{k}.json", img=c.get("img", IMG))
            rcs.append(rc); outs.append(o)
        ident = all(o is not None and o == outs[0] for o in outs)
        ok = all(r == 0 for r in rcs) and outs[0] is not None and ident
        return ok, f"{runs}x all-exit0={all(r==0 for r in rcs)} identical={ident}"
    if kind == "schema":
        rc, out = run_insp(GDEF, img=c.get("img", IMG))
        if rc != 0 or out is None: return False, rc_str(rc)
        try: json.loads(out)
        except Exception as e: return False, f"invalid JSON: {e}"
        low = out.lower()
        return (b"reports" in out and b"nan" not in low and b"inf" not in low), \
               f"validJSON no_nan/inf={b'nan' not in low and b'inf' not in low}"
    if kind == "expect_rc":
        dp = c["make"]() if "make" in c else (c.get("defpath", GDEF))
        rc, out = run_insp(dp, img=c.get("img", IMG))
        return (rc == c["rc"]), f"{rc_str(rc)} (want exit{c['rc']})"
    if kind == "custom":
        return c["fn"](run_insp)
    return False, "unknown kind"

def run_module(modname, cases):
    print(f"==== {modname}: {len(cases)} cases ====")
    fails = 0
    for c in cases:
        ok, detail = _eval(c)
        print(f"  {c['name']:<34} {'PASS' if ok else 'FAIL'}  {detail}")
        if not ok: fails += 1
    print(f"  -> {len(cases)-fails}/{len(cases)} pass")
    return fails
