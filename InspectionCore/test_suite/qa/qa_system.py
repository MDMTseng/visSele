"""QA module: SYSTEM / IO / CLI-CONTRACT / RESOURCE ROBUSTNESS for `visSele --insp`.

VIEWPOINT
  This module owns the *harness boundary* — everything around the def-schema:
  the image file, the def file, the output path, repeated/concurrent invocation,
  and the documented exit-code contract. Def-schema mutation is owned by another
  agent; here we only ever pass either the pristine golden def or deliberately
  broken *files* (zero-byte, garbage, wrong-format, missing).

DOCUMENTED EXIT-CODE CONTRACT (probed empirically against the real binary)
  exit0  success (report written)
  exit3  image cannot be loaded   (missing / zero-byte / wrong-format image)
  exit4  def cannot be parsed     (missing / zero-byte / garbage def)
  NOTE   an UNWRITABLE output path (e.g. /nonexistent_dir/out.json) still
         returns exit0 — the engine does NOT fail the run when it cannot
         persist the report. We assert the *observed* behavior (rc==0, no
         file produced), not an idealized one. See `out_unwritable_rc0`.

PLAN / CASE MAP
  Image IO (expect_rc / robust):
    - img_missing_rc3         missing image path -> exit3, stable
    - img_zerobyte_rc3        zero-byte .png     -> exit3
    - img_wrongfmt_rc3        a .hydef passed as the image -> exit3 (no crash)
    - img_garbage_robust      random bytes as image -> no memory-unsafe crash
    - img_alt_rc0             ALT_IMG (the non-_bk variant) loads & runs -> exit0
  Def IO (expect_rc / robust):
    - def_missing_rc4         missing def path -> exit4
    - def_zerobyte_rc4        zero-byte def    -> exit4
    - def_garbage_rc4         non-JSON text def -> exit4 (controlled, no crash)
    - def_truncated_robust    half a golden def -> no memory-unsafe crash
  Output path:
    - out_unwritable_rc0      out under nonexistent dir -> rc0, file absent (custom)
    - out_to_existing_dir     out path that is a directory -> no crash (custom)
  Resource / stability:
    - golden_schema           pristine golden -> valid JSON report (schema)
    - golden_determinism      golden run 3x -> all exit0 & byte-identical
    - alt_determinism         ALT_IMG run 2x -> stable
    - large_def_robust        golden featureSet duplicated ~6x -> no crash/hang
    - concurrent_distinct_out 4 parallel runs to distinct out paths -> all exit0,
                              all reports present, no file-handle clobber (custom)
"""
import sys, os, json, copy, subprocess, threading
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qalib import *

CASES = []

# ---- helpers -------------------------------------------------------------
def _write(name, data):
    p = f"{TMP}/sys_{name}"
    if isinstance(data, (bytes, bytearray)):
        open(p, "wb").write(data)
    else:
        open(p, "w").write(data)
    return p

def _zerobyte(name):
    p = f"{TMP}/sys_{name}"; open(p, "wb").close(); return p

# zero-byte / garbage image files (created at import time)
ZERO_PNG  = _zerobyte("zero.png")
GARB_PNG  = _write("garbage.png", os.urandom(4096))
TRUNC_DEF = _write("trunc.hydef", open(GDEF).read()[: max(1, os.path.getsize(GDEF)//2)])

def mut_large_def(factor=6):
    """Duplicate the golden featureSet `factor` times to stress time/memory."""
    def f():
        d = golden()
        # find a list of features to multiply; fall back to whole-doc dup of arrays
        def bloat(o):
            if isinstance(o, dict):
                for k, v in list(o.items()):
                    if isinstance(v, list) and v and isinstance(v[0], dict):
                        o[k] = v * factor
                    else:
                        bloat(v)
            elif isinstance(o, list):
                for v in o: bloat(v)
        bloat(d)
        return d
    return f

# ---- image IO ------------------------------------------------------------
CASES.append(case("img_missing_rc3", "expect_rc", rc=3, img=f"{TMP}/__no_such_img__.png"))
CASES.append(case("img_zerobyte_rc3", "expect_rc", rc=3, img=ZERO_PNG))
CASES.append(case("img_wrongfmt_rc3", "expect_rc", rc=3, img=GDEF))
CASES.append(case("img_garbage_robust", "robust", img=GARB_PNG))
CASES.append(case("img_alt_rc0", "expect_rc", rc=0, img=ALT_IMG))

# ---- def IO --------------------------------------------------------------
CASES.append(case("def_missing_rc4", "expect_rc", rc=4, defpath=f"{TMP}/__no_such_def__.hydef"))
CASES.append(case("def_zerobyte_rc4", "expect_rc", rc=4, defpath=_zerobyte("zero.hydef")))
CASES.append(case("def_garbage_rc4", "expect_rc", rc=4, defpath=_write("garbage.hydef", "not json {{{ )))")))
CASES.append(case("def_truncated_robust", "robust", raw_path=TRUNC_DEF))

# ---- output path ---------------------------------------------------------
def fn_out_unwritable(run):
    bad = "/nonexistent_dir/qa_out.json"
    if os.path.exists(bad): os.remove(bad)
    rc, out = run(GDEF, out_path=bad)
    # harness reads back the file; absent dir -> out is None. Engine returns 0.
    ok = (rc == 0) and (out is None) and (not os.path.exists(bad))
    return ok, f"rc={rc_str(rc)} file_present={os.path.exists(bad)}"
CASES.append(case("out_unwritable_rc0", "custom", fn=fn_out_unwritable))

def fn_out_under_file(run):
    # parent of out path is a regular file, not a dir -> ENOTDIR on write.
    # (avoids the harness readback IsADirectoryError that a dir out_path triggers)
    f = f"{TMP}/sys_notadir"; open(f, "w").write("x")
    bad = f + "/qa_out.json"
    rc, out = run(GDEF, out_path=bad)
    ok = (rc not in SIGCRASH and rc != "TIMEOUT") and out is None
    return ok, f"rc={rc_str(rc)} file_present={os.path.exists(bad)} (no memory-unsafe crash)"
CASES.append(case("out_parent_not_dir", "custom", fn=fn_out_under_file))

# ---- resource / stability -----------------------------------------------
CASES.append(case("golden_schema", "schema"))
CASES.append(case("golden_determinism", "determinism", runs=3))
CASES.append(case("alt_determinism", "determinism", runs=2, img=ALT_IMG))
CASES.append(case("large_def_robust", "robust", make=mut_large_def(6)))

def fn_concurrent(run):
    N = 4
    results = {}
    def worker(i):
        rc, out = run(GDEF, out_path=f"{TMP}/sys_conc_{i}.json")
        results[i] = (rc, out)
    ts = [threading.Thread(target=worker, args=(i,)) for i in range(N)]
    for t in ts: t.start()
    for t in ts: t.join()
    all0 = all(results[i][0] == 0 for i in range(N))
    allout = all(results[i][1] is not None for i in range(N))
    # each run should produce a valid, non-clobbered JSON report
    valid = True
    for i in range(N):
        try: json.loads(results[i][1])
        except Exception: valid = False
    ok = all0 and allout and valid
    return ok, f"{N}x parallel all-exit0={all0} all-written={allout} all-validJSON={valid}"
CASES.append(case("concurrent_distinct_out", "custom", fn=fn_concurrent))

if __name__ == "__main__":
    sys.exit(run_module("qa_system", CASES))
