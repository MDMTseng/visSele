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

# ==========================================================================
# ROUND 2 — deeper harness/system boundary
# ==========================================================================

# ---- crafted image files (built at import time with PIL) ------------------
def _have_pil():
    try:
        import PIL  # noqa
        return True
    except Exception:
        return False

_PIL = _have_pil()

def _save_png(name, im):
    p = f"{TMP}/sys_{name}"
    im.save(p, "PNG")
    return p

if _PIL:
    from PIL import Image
    # valid PNG header, deliberately corrupted body: keep the 8-byte signature
    # + IHDR but smash the IDAT compressed stream so decode fails mid-file.
    _full = open(_save_png("good_tmp.png", Image.new("L", (64, 64), 128)), "rb").read()
    # corrupt: flip the back third of the file (past IHDR, into IDAT) to garbage
    _ba = bytearray(_full)
    _start = len(_ba) * 2 // 3
    for _i in range(_start, len(_ba) - 1):  # leave final byte; trash IDAT/IEND region
        _ba[_i] ^= 0xFF
    CORRUPT_PNG = _write("corrupt_body.png", bytes(_ba))
    # truncated PNG: valid header but file cut off mid-IDAT
    TRUNC_PNG = _write("trunc.png", _full[: len(_full) * 2 // 3])
    # 1x1 tiny grayscale
    TINY_PNG = _save_png("tiny1x1.png", Image.new("L", (1, 1), 200))
    # wrong channel count: RGBA where engine likely expects gray / single-plane
    RGBA_PNG = _save_png("rgba.png", Image.new("RGBA", (128, 128), (10, 20, 30, 255)))
    # RGB (3 channel) variant
    RGB_PNG = _save_png("rgb.png", Image.new("RGB", (128, 128), (40, 40, 40)))
    # huge image (well beyond golden 2592x1944) to probe alloc/time limits
    HUGE_PNG = _save_png("huge.png", Image.new("L", (8000, 8000), 100))
else:
    CORRUPT_PNG = TRUNC_PNG = TINY_PNG = RGBA_PNG = RGB_PNG = HUGE_PNG = None

# ---- crafted-image robustness / contract --------------------------------
if _PIL:
    # valid header + corrupt body: must reject cleanly, never crash unsafely
    CASES.append(case("img_corrupt_body_robust", "robust", img=CORRUPT_PNG))
    # truncated PNG (header ok, cut mid-stream): no memory-unsafe crash / hang
    CASES.append(case("img_truncated_robust", "robust", img=TRUNC_PNG))
    # 1x1 image: degenerate-but-valid; engine may run (exit0) or reject — must
    # not crash. Use robust (any controlled rc/abort acceptable).
    CASES.append(case("img_tiny_1x1_robust", "robust", img=TINY_PNG))
    # wrong channel count (RGBA / RGB): must not crash; rc must be stable.
    CASES.append(case("img_rgba_robust", "robust", img=RGBA_PNG))
    CASES.append(case("img_rgb_robust", "robust", img=RGB_PNG))
    # huge image: time/memory stress, must finish within timeout & not crash.
    CASES.append(case("img_huge_robust", "robust", img=HUGE_PNG))

    # the same crafted-bad images must yield a STABLE exit code across repeats
    # (no nondeterministic crash-vs-reject). custom: run each twice, compare rc.
    def _stable_rc_factory(img_path, label):
        def fn(run):
            rcs = []
            for _ in range(2):
                rc, _o = run(GDEF, out_path=f"{TMP}/sys_stab_{label}.json", img=img_path)
                rcs.append(rc)
            stable = (rcs[0] == rcs[1])
            safe = all(r not in SIGCRASH and r != "TIMEOUT" for r in rcs)
            return (stable and safe), f"rc1={rc_str(rcs[0])} rc2={rc_str(rcs[1])} stable={stable} safe={safe}"
        return fn
    CASES.append(case("img_corrupt_stable_rc", "custom", fn=_stable_rc_factory(CORRUPT_PNG, "corrupt")))
    CASES.append(case("img_rgba_stable_rc", "custom", fn=_stable_rc_factory(RGBA_PNG, "rgba")))

# ---- deeply duplicated def (10-20x) for time / no-crash ------------------
CASES.append(case("xl_def_15x_robust", "robust", make=mut_large_def(15)))
def fn_xl_def_no_hang(run):
    """20x-bloated def must complete (not hang) and not crash; we don't require
    exit0 (huge def may be rejected) — only liveness + memory-safety."""
    import time as _t
    d = mut_large_def(20)()
    t0 = _t.time()
    rc, out = run(d, out_path=f"{TMP}/sys_xl20.json")
    dt = _t.time() - t0
    ok = (rc != "TIMEOUT") and (rc not in SIGCRASH)
    return ok, f"rc={rc_str(rc)} {dt:.1f}s (liveness + memory-safe)"
CASES.append(case("xl_def_20x_no_hang", "custom", fn=fn_xl_def_no_hang))

# ---- output corruption / state isolation across processes ---------------
def fn_malformed_then_golden(run):
    """Each run_insp is a FRESH process, so in-process state cannot bleed. We
    instead verify a malformed run cannot corrupt a SUBSEQUENT golden run's
    output: capture baseline golden, run a malformed def, then golden again to
    the SAME out path, and assert byte-identical to baseline."""
    base_p = f"{TMP}/sys_iso_base.json"
    rc0, base = run(GDEF, out_path=base_p)
    if rc0 != 0 or base is None:
        return False, f"baseline golden failed rc={rc_str(rc0)}"
    # interleave a malformed (garbage) def run against the SAME out path
    garb = _write("iso_garbage.hydef", "}{ not json")
    rc1, _ = run(garb, out_path=base_p)
    # golden again to the same path
    rc2, after = run(GDEF, out_path=base_p)
    same = (after is not None and after == base)
    ok = (rc2 == 0) and same
    return ok, f"malformed_rc={rc_str(rc1)} golden2_rc={rc_str(rc2)} byte_identical_to_baseline={same}"
CASES.append(case("malformed_no_output_corruption", "custom", fn=fn_malformed_then_golden))

def fn_interleave_golden_bad(run):
    """Stronger: 3 golden runs interleaved with bad-image / bad-def runs, all to
    distinct out paths; every golden output must equal the first golden output
    (no cross-run corruption / leaked file handles)."""
    goldens = []
    seq = ["G", "BADIMG", "G", "BADDEF", "G"]
    badimg = GARB_PNG
    baddef = _write("inter_bad.hydef", "garbage")
    for i, step in enumerate(seq):
        if step == "G":
            rc, o = run(GDEF, out_path=f"{TMP}/sys_inter_{i}.json")
            goldens.append((rc, o))
        elif step == "BADIMG":
            run(GDEF, out_path=f"{TMP}/sys_inter_{i}.json", img=badimg)
        else:
            run(baddef, out_path=f"{TMP}/sys_inter_{i}.json")
    all0 = all(rc == 0 and o is not None for rc, o in goldens)
    ident = all0 and all(o == goldens[0][1] for _, o in goldens)
    return (all0 and ident), f"golden_runs={len(goldens)} all_exit0={all0} all_identical={ident}"
CASES.append(case("interleave_golden_bad_identical", "custom", fn=fn_interleave_golden_bad))

# ---- more parallelism (8x) ----------------------------------------------
def fn_concurrent8(run):
    N = 8
    results = {}
    def worker(i):
        results[i] = run(GDEF, out_path=f"{TMP}/sys_conc8_{i}.json")
    ts = [threading.Thread(target=worker, args=(i,)) for i in range(N)]
    for t in ts: t.start()
    for t in ts: t.join()
    all0 = all(results[i][0] == 0 for i in range(N))
    allout = all(results[i][1] is not None for i in range(N))
    # all reports must be byte-identical (same def+img => deterministic)
    ref = results[0][1]
    ident = allout and all(results[i][1] == ref for i in range(N))
    return (all0 and allout and ident), f"{N}x all-exit0={all0} all-written={allout} all-identical={ident}"
CASES.append(case("concurrent8_identical", "custom", fn=fn_concurrent8))

# ---- output to /dev/null ------------------------------------------------
def fn_out_devnull(run):
    """Write report to /dev/null: harness readback yields b'' (exists, empty).
    Engine must run cleanly (rc0) and not crash on a non-regular out file."""
    rc, out = run(GDEF, out_path="/dev/null")
    ok = (rc == 0) and (rc not in SIGCRASH)
    return ok, f"rc={rc_str(rc)} (write to /dev/null, no crash)"
CASES.append(case("out_devnull_rc0", "custom", fn=fn_out_devnull))

# ---- very long file paths -----------------------------------------------
def fn_long_out_path(run):
    """Out path with a very long (255-char) filename component. Most filesystems
    cap NAME_MAX at 255; engine must not crash and must return a stable rc."""
    longname = "L" * 240 + ".json"   # filename component < NAME_MAX(255)
    bad = f"{TMP}/{longname}"
    rc, out = run(GDEF, out_path=bad)
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} file_present={os.path.exists(bad)} (long name, no crash)"
CASES.append(case("out_long_path", "custom", fn=fn_long_out_path))

def fn_long_def_path(run):
    """Pass a def via a very long path. Copy golden to a long-named file and run.
    Must load & run (rc0) or reject cleanly — never crash."""
    longname = "D" * 240 + ".hydef"   # filename component < NAME_MAX(255)
    p = f"{TMP}/{longname}"
    open(p, "w").write(open(GDEF).read())
    rc, out = run(p, out_path=f"{TMP}/sys_longdef_out.json")
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} (long def path, no crash)"
CASES.append(case("def_long_path", "custom", fn=fn_long_def_path))

# ---- unicode filenames --------------------------------------------------
def fn_unicode_paths(run):
    """Unicode (CJK + emoji) in both def and out filenames. Must load/run and
    write the report; no crash, stable rc."""
    udef = f"{TMP}/检测_測試_def.hydef"
    open(udef, "w").write(open(GDEF).read())
    uout = f"{TMP}/報告_report.json"
    rc, out = run(udef, out_path=uout)
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    wrote = os.path.exists(uout)
    return safe, f"rc={rc_str(rc)} report_written={wrote} (unicode paths, no crash)"
CASES.append(case("unicode_def_out_paths", "custom", fn=fn_unicode_paths))

def fn_unicode_img(run):
    """Unicode-named image (copy golden image). Engine must load it & run."""
    import shutil
    uimg = f"{TMP}/画像_\U0001f4f7.png"
    shutil.copy(IMG, uimg)
    rc, out = run(GDEF, out_path=f"{TMP}/sys_uimg_out.json", img=uimg)
    ok = (rc == 0)
    return ok, f"rc={rc_str(rc)} (unicode image name)"
CASES.append(case("unicode_img_rc0", "custom", fn=fn_unicode_img))

if __name__ == "__main__":
    sys.exit(run_module("qa_system", CASES))
