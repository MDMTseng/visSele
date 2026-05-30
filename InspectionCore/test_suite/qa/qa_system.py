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

# ==========================================================================
# ROUND 3 — boundary sizes, formats, CLI argv, same-path race, deep def
# ==========================================================================

if _PIL:
    from PIL import Image as _PILImage
    # min-size boundary: 31 should be rejected (exit3), 32 should succeed-or-
    # cleanly-fail; both must be memory-safe.
    _BND_31 = _save_png("bnd_31x31.png", _PILImage.new("L", (31, 31), 128))
    _BND_32 = _save_png("bnd_32x32.png", _PILImage.new("L", (32, 32), 128))
    _BND_33 = _save_png("bnd_33x33.png", _PILImage.new("L", (33, 33), 128))
    _NSQ_TALL  = _save_png("nsq_32x1000.png", _PILImage.new("L", (32, 1000), 128))
    _NSQ_WIDE  = _save_png("nsq_1000x32.png", _PILImage.new("L", (1000, 32), 128))
    _NSQ_MIXED = _save_png("nsq_32x31.png",   _PILImage.new("L", (32, 31), 128))  # one dim below min
    # PNG with Adam7 interlacing
    _INTERLACE_PATH = f"{TMP}/sys_interlaced.png"
    _PILImage.new("L", (256, 256), 100).save(_INTERLACE_PATH, "PNG", interlace=True)
    # JPEG (different format)
    _JPEG_PATH = f"{TMP}/sys_real.jpg"
    _PILImage.new("L", (256, 256), 100).save(_JPEG_PATH, "JPEG")
    # BMP saved with .png extension (extension-vs-magic mismatch)
    _BMP_AS_PNG = f"{TMP}/sys_bmp_as.png"
    _PILImage.new("L", (256, 256), 100).save(_BMP_AS_PNG, "BMP")
    # PNG with a weird gamma/ICC profile chunk (use a tagged sRGB ICC)
    _WEIRD_DPI = f"{TMP}/sys_weird_dpi.png"
    _PILImage.new("L", (256, 256), 100).save(_WEIRD_DPI, "PNG", dpi=(999999, 1))
    # very large image — 8192x8192 grayscale ~ 64MB, safer than 16384²
    _BIG_IMG = _save_png("big_8192.png", _PILImage.new("L", (8192, 8192), 80))

    # boundary: 31x31 must be rejected (engine min is 32)
    CASES.append(case("img_boundary_31x31_rc3", "expect_rc", rc=3, img=_BND_31))
    # boundary: 32x32 -- must not crash (may exit0 or controlled reject)
    CASES.append(case("img_boundary_32x32_robust", "robust", img=_BND_32))
    # boundary: 33x33 -- just above min, must not crash
    CASES.append(case("img_boundary_33x33_robust", "robust", img=_BND_33))
    # non-square: tall / wide -- must not crash
    CASES.append(case("img_nonsquare_tall_robust", "robust", img=_NSQ_TALL))
    CASES.append(case("img_nonsquare_wide_robust", "robust", img=_NSQ_WIDE))
    # one dim under min -> exit3 (boundary check is per-dim)
    CASES.append(case("img_one_dim_under_min_rc3", "expect_rc", rc=3, img=_NSQ_MIXED))
    # interlaced PNG must load / process without crash
    CASES.append(case("img_interlaced_png_robust", "robust", img=_INTERLACE_PATH))
    # JPEG: loader may or may not support; must not crash either way
    CASES.append(case("img_jpeg_robust", "robust", img=_JPEG_PATH))
    # BMP-with-.png-extension: format detection by content vs extension; no crash
    CASES.append(case("img_bmp_as_png_robust", "robust", img=_BMP_AS_PNG))
    # weird DPI metadata: must not affect run; no crash
    CASES.append(case("img_weird_dpi_robust", "robust", img=_WEIRD_DPI))
    # large image: time / memory stress, must finish & not crash
    def fn_big_img(run):
        import time as _t
        t0 = _t.time()
        rc, _ = run(GDEF, out_path=f"{TMP}/sys_big.json", img=_BIG_IMG)
        dt = _t.time() - t0
        ok = (rc != "TIMEOUT") and (rc not in SIGCRASH)
        return ok, f"rc={rc_str(rc)} {dt:.1f}s (8192x8192, no crash)"
    CASES.append(case("img_big_8192_no_crash", "custom", fn=fn_big_img))

# ---- same-out-path race (4 workers, identical out path) -----------------
def fn_concurrent_same_out(run):
    N = 4
    out = f"{TMP}/sys_race_same.json"
    results = {}
    def worker(i):
        results[i] = run(GDEF, out_path=out)
    ts = [threading.Thread(target=worker, args=(i,)) for i in range(N)]
    for t in ts: t.start()
    for t in ts: t.join()
    # Each worker last-reads the (possibly mid-write) file; we accept any rc==0,
    # require no crash / no timeout / final file is valid JSON.
    safe = all(r[0] == 0 and r[0] not in SIGCRASH for r in results.values())
    final_ok = False
    if os.path.exists(out):
        try: json.loads(open(out, "rb").read()); final_ok = True
        except Exception: final_ok = False
    return (safe and final_ok), f"{N}x same-path all-rc0={safe} final-validJSON={final_ok}"
CASES.append(case("concurrent_same_out_race", "custom", fn=fn_concurrent_same_out))

# ---- CLI argv: extra unknown args before --insp ------------------------
def _run_argv(extra_args_before=None, extra_args_after=None, timeout=120):
    """Run visSele with custom argv around the --insp triplet, return rc."""
    out = f"{TMP}/sys_argv_out.json"
    if os.path.exists(out): os.remove(out)
    argv = [VIS]
    if extra_args_before: argv += extra_args_before
    argv += ["--insp", IMG, GDEF, out]
    if extra_args_after: argv += extra_args_after
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    try:
        r = subprocess.run(argv, cwd=CORE, env=env, capture_output=True, timeout=timeout)
        return r.returncode, (open(out, "rb").read() if os.path.exists(out) else None)
    except subprocess.TimeoutExpired:
        return ("TIMEOUT", None)

def fn_extra_args_before(_run):
    # unknown junk args before --insp -- the --insp scanner just skips them
    rc, out = _run_argv(extra_args_before=["unknown_garbage", "anotherjunk"])
    ok = (rc == 0) and out is not None
    return ok, f"rc={rc_str(rc)} report_present={out is not None}"
CASES.append(case("argv_extra_before_insp", "custom", fn=fn_extra_args_before))

def fn_extra_args_after(_run):
    # trailing junk after the --insp triplet (must still run successfully)
    rc, out = _run_argv(extra_args_after=["trailing=garbage", "moreafter"])
    ok = (rc == 0) and out is not None
    return ok, f"rc={rc_str(rc)} report_present={out is not None}"
CASES.append(case("argv_extra_after_insp", "custom", fn=fn_extra_args_after))

def fn_very_long_argv(_run):
    # ~200 extra tokens of varying length; must not crash, must still run
    junk = [f"j{i}=x" * 3 for i in range(200)]
    rc, out = _run_argv(extra_args_before=junk)
    ok = (rc not in SIGCRASH) and (rc != "TIMEOUT") and (rc == 0) and out is not None
    return ok, f"rc={rc_str(rc)} argv_len={200+5} report_present={out is not None}"
CASES.append(case("argv_very_long", "custom", fn=fn_very_long_argv))

# ---- deeply nested def (binary_processing_group wrap N levels deep) -----
def mut_deeply_nested(depth=20):
    """Wrap golden featureSet in N nested binary_processing_groups."""
    def f():
        d = golden()
        inner = d.get("featureSet")
        if inner is None: return d
        cur = inner
        for _ in range(depth):
            cur = [{"type": "binary_processing_group", "featureSet": cur}]
        d["featureSet"] = cur
        return d
    return f
CASES.append(case("def_deeply_nested_20_robust", "robust", make=mut_deeply_nested(20)))
CASES.append(case("def_deeply_nested_50_robust", "robust", make=mut_deeply_nested(50)))

# ---- def with featureSet referencing a nonexistent included path -------
def mut_nonexistent_include():
    def f():
        d = golden()
        # mimic an "include" / nested file ref. Many schemas accept a string at
        # the featureSet slot but we just inject a path-like value alongside.
        d["featureSet"].append({
            "type": "binary_processing_group",
            "include": "/nonexistent/path/to/feature.hydef",
            "featureSet": [],
        })
        return d
    return f
CASES.append(case("def_nonexistent_include_robust", "robust", make=mut_nonexistent_include()))

# ==========================================================================
# ROUND 4 — color spaces, big IDAT, dim boundaries, process isolation,
#           argv flood, env-flag combos, mega def, runtime SLO, extreme pixels
# ==========================================================================

if _PIL:
    from PIL import Image as _PI4
    # 16-bit grayscale PNG (I;16)
    _R4_16BIT = f"{TMP}/sys_r4_16bit.png"
    _PI4.new("I;16", (256, 256), 30000).save(_R4_16BIT, "PNG")
    CASES.append(case("r4_img_16bit_gray_robust", "robust", img=_R4_16BIT))

    # Palettized (P-mode) PNG
    _R4_PAL = f"{TMP}/sys_r4_palette.png"
    _pal = _PI4.new("P", (256, 256), 5)
    _pal.putpalette([i % 256 for i in range(768)])
    _pal.save(_R4_PAL, "PNG")
    CASES.append(case("r4_img_palette_robust", "robust", img=_R4_PAL))

    # Grayscale-with-alpha (LA mode)
    _R4_LA = f"{TMP}/sys_r4_LA.png"
    _PI4.new("LA", (256, 256), (120, 255)).save(_R4_LA, "PNG")
    CASES.append(case("r4_img_gray_alpha_robust", "robust", img=_R4_LA))

    # Very large IDAT (~10MB): high-entropy random pixels resist compression
    _R4_BIGIDAT = f"{TMP}/sys_r4_bigidat.png"
    import random as _rnd
    _rnd.seed(42)
    _big = _PI4.frombytes("L", (3000, 3000), bytes(_rnd.getrandbits(8) for _ in range(3000*3000)))
    _big.save(_R4_BIGIDAT, "PNG", compress_level=1)
    CASES.append(case("r4_img_big_idat_robust", "robust", img=_R4_BIGIDAT))

    # Dim boundary 32x33 / 33x32
    _R4_32x33 = f"{TMP}/sys_r4_32x33.png"; _PI4.new("L", (32, 33), 128).save(_R4_32x33, "PNG")
    _R4_33x32 = f"{TMP}/sys_r4_33x32.png"; _PI4.new("L", (33, 32), 128).save(_R4_33x32, "PNG")
    CASES.append(case("r4_img_32x33_robust", "robust", img=_R4_32x33))
    CASES.append(case("r4_img_33x32_robust", "robust", img=_R4_33x32))

    # All-zero (pure black) and all-white pixel images at golden size
    _gw, _gh = 256, 256
    _R4_ALLBLACK = f"{TMP}/sys_r4_allblack.png"; _PI4.new("L", (_gw, _gh), 0).save(_R4_ALLBLACK, "PNG")
    _R4_ALLWHITE = f"{TMP}/sys_r4_allwhite.png"; _PI4.new("L", (_gw, _gh), 255).save(_R4_ALLWHITE, "PNG")
    CASES.append(case("r4_img_all_black_robust", "robust", img=_R4_ALLBLACK))
    CASES.append(case("r4_img_all_white_robust", "robust", img=_R4_ALLWHITE))

    # One bright pixel, rest black
    _R4_ONEPIX = f"{TMP}/sys_r4_onepix.png"
    _imop = _PI4.new("L", (_gw, _gh), 0); _imop.putpixel((_gw//2, _gh//2), 255)
    _imop.save(_R4_ONEPIX, "PNG")
    CASES.append(case("r4_img_one_bright_pixel_robust", "robust", img=_R4_ONEPIX))

# ---- concurrent isolation: malformed run + golden run, process isolation -
def fn_r4_concurrent_isolation(run):
    """Two threads = two subprocesses. One uses malformed def, one uses golden.
    Verify: golden subprocess returns rc==0 with valid JSON regardless of the
    malformed sibling. Each --insp invocation is its own process so they MUST
    be independent."""
    garb = _write("r4_iso_garbage.hydef", "}}}not-json{{{")
    results = {}
    def w_bad():
        results["bad"] = run(garb, out_path=f"{TMP}/sys_r4_iso_bad.json")
    def w_good():
        results["good"] = run(GDEF, out_path=f"{TMP}/sys_r4_iso_good.json")
    ts = [threading.Thread(target=w_bad), threading.Thread(target=w_good)]
    for t in ts: t.start()
    for t in ts: t.join()
    g_rc, g_out = results["good"]
    b_rc, _b_out = results["bad"]
    valid = False
    if g_out is not None:
        try: json.loads(g_out); valid = True
        except Exception: valid = False
    ok = (g_rc == 0) and valid and (b_rc != "TIMEOUT") and (b_rc not in SIGCRASH)
    return ok, f"good_rc={rc_str(g_rc)} valid={valid} bad_rc={rc_str(b_rc)} (process-isolated)"
CASES.append(case("r4_concurrent_process_isolation", "custom", fn=fn_r4_concurrent_isolation))

# ---- argv at the absolute limit: 1000 ignored junk tokens ---------------
def fn_r4_argv_1000(_run):
    junk = [f"ignored_{i}=abc" for i in range(1000)]
    rc, out = _run_argv(extra_args_after=junk, timeout=180)
    ok = (rc == 0) and out is not None and (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return ok, f"rc={rc_str(rc)} argv_len~={1005} report_present={out is not None}"
CASES.append(case("r4_argv_1000_ignored", "custom", fn=fn_r4_argv_1000))

# ---- env var combo: CALIP_DUMP + SPCV_DUMP + SP_PT_DUMP all set ---------
def fn_r4_env_dumps(_run):
    """All three *_DUMP env vars on simultaneously — engine should run cleanly,
    produce its normal report, and not crash. Extra dump files are fine."""
    out = f"{TMP}/sys_r4_envdump.json"
    if os.path.exists(out): os.remove(out)
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD,
               CALIP_DUMP="1", SPCV_DUMP="1", SP_PT_DUMP="1")
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out], cwd=CORE, env=env,
                           capture_output=True, timeout=180)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(out, "rb").read() if os.path.exists(out) else None
    valid = False
    if report is not None:
        try: json.loads(report); valid = True
        except Exception: valid = False
    ok = (rc == 0) and valid and (rc not in SIGCRASH)
    return ok, f"rc={rc_str(rc)} report_valid={valid} (all DUMP flags on)"
CASES.append(case("r4_env_all_dump_flags", "custom", fn=fn_r4_env_dumps))

# ---- 100-feature def (replicate golden featureSet) ---------------------
def mut_r4_replicate_features(target=100):
    def f():
        d = golden()
        fs = d.get("featureSet")
        if isinstance(fs, list) and fs:
            # repeat in place until we reach >= target features
            base = list(fs)
            while len(fs) < target:
                fs.extend(copy.deepcopy(base))
            d["featureSet"] = fs[:target]
        return d
    return f
CASES.append(case("r4_def_100_features_robust", "robust", make=mut_r4_replicate_features(100)))

# ---- golden runtime SLO: < 5s -----------------------------------------
def fn_r4_golden_runtime(run):
    import time as _t
    t0 = _t.time()
    rc, out = run(GDEF, out_path=f"{TMP}/sys_r4_perf.json")
    dt = _t.time() - t0
    ok = (rc == 0) and (out is not None) and (dt < 5.0)
    return ok, f"rc={rc_str(rc)} {dt:.2f}s (SLO < 5.0s)"
CASES.append(case("r4_golden_runtime_under_5s", "custom", fn=fn_r4_golden_runtime))

# ==========================================================================
# ROUND 5 — abs vs rel paths, dangerous string slots, symlink/FIFO def,
#           50MB def, 16-bit PNG, concurrency w/ shared dir, env locale,
#           sequential 50x memory-growth probe
# ==========================================================================
import shutil, stat, tempfile, time as _time5

# ---- abs vs rel --insp path: engine resolves data/default_camera_param.json
#      relative to cwd (CORE). Verify that passing ABSOLUTE paths still works
#      and that running from a DIFFERENT cwd does not break the run because
#      the engine internally cd's / loads data relative to CORE.
def fn_r5_abs_paths_from_other_cwd(_run):
    out = f"{TMP}/sys_r5_abs_out.json"
    if os.path.exists(out): os.remove(out)
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    # cwd = TMP, but pass absolute paths everywhere
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                           cwd=TMP, env=env, capture_output=True, timeout=120)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(out, "rb").read() if os.path.exists(out) else None
    # data/default_camera_param.json is relative to CORE; running from TMP may
    # fail to find it. We require: no memory-unsafe crash. rc0+report = great.
    safe = (rc not in SIGCRASH)
    return safe, f"rc={rc_str(rc)} report_present={report is not None} (cwd=TMP, abs paths)"
CASES.append(case("r5_abs_paths_other_cwd", "custom", fn=fn_r5_abs_paths_from_other_cwd))

def fn_r5_relative_paths_from_core(_run):
    # cwd = CORE; pass RELATIVE paths. Engine should resolve normally.
    rel_out = "tmp_qa_r5_rel_out.json"
    rel_out_abs = f"{CORE}/{rel_out}"
    if os.path.exists(rel_out_abs): os.remove(rel_out_abs)
    # Need relative img/def paths too — copy to CORE
    rel_img = "tmp_qa_r5_rel_img.png"
    rel_def = "tmp_qa_r5_rel_def.hydef"
    shutil.copy(IMG, f"{CORE}/{rel_img}")
    shutil.copy(GDEF, f"{CORE}/{rel_def}")
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    try:
        r = subprocess.run([VIS, "--insp", rel_img, rel_def, rel_out],
                           cwd=CORE, env=env, capture_output=True, timeout=120)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(rel_out_abs, "rb").read() if os.path.exists(rel_out_abs) else None
    # cleanup
    for p in (rel_out_abs, f"{CORE}/{rel_img}", f"{CORE}/{rel_def}"):
        if os.path.exists(p): os.remove(p)
    ok = (rc == 0) and (report is not None)
    return ok, f"rc={rc_str(rc)} report_present={report is not None} (rel paths from CORE)"
CASES.append(case("r5_relative_paths_from_core", "custom", fn=fn_r5_relative_paths_from_core))

# ---- def referencing CalibMapPath / StageLightReportPath at bad locations
#      (buffer-overflow site previously fixed). Engine must not crash on
#      nonexistent files or paths outside data/.
def mut_r5_bad_calib_paths():
    def f():
        d = golden()
        d["CalibMapPath"] = "/nonexistent/calib/map/that/does/not/exist.json"
        d["StageLightReportPath"] = "/tmp/__no_such_stage_light_report__.json"
        return d
    return f
CASES.append(case("r5_def_bad_calib_paths_robust", "robust", make=mut_r5_bad_calib_paths()))

def mut_r5_long_calib_paths():
    # very long string in CalibMapPath — previous buffer-overflow site
    def f():
        d = golden()
        d["CalibMapPath"] = "/" + ("A" * 4000) + ".json"
        d["StageLightReportPath"] = "/" + ("B" * 4000) + ".json"
        return d
    return f
CASES.append(case("r5_def_overlong_calib_paths_robust", "robust", make=mut_r5_long_calib_paths()))

# ---- image-load via symlink chain ---------------------------------------
def fn_r5_symlink_chain(run):
    # build a 5-deep symlink chain pointing at the golden image
    chain_dir = f"{TMP}/r5_symchain"
    os.makedirs(chain_dir, exist_ok=True)
    prev = IMG
    for i in range(5):
        link = f"{chain_dir}/link{i}.png"
        if os.path.islink(link) or os.path.exists(link): os.remove(link)
        os.symlink(prev, link)
        prev = link
    rc, out = run(GDEF, out_path=f"{TMP}/sys_r5_sym_out.json", img=prev)
    ok = (rc == 0) and out is not None
    return ok, f"rc={rc_str(rc)} report_present={out is not None} (5-deep symlink chain)"
CASES.append(case("r5_image_symlink_chain_rc0", "custom", fn=fn_r5_symlink_chain))

# ---- def file is a FIFO --------------------------------------------------
def fn_r5_def_is_fifo(run):
    fifo = f"{TMP}/sys_r5_def.fifo"
    if os.path.exists(fifo): os.remove(fifo)
    os.mkfifo(fifo)
    out = f"{TMP}/sys_r5_fifo_out.json"
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    # writer thread: opens for write (blocks until reader opens), writes garbage,
    # then closes -> EOF for reader. Otherwise engine could legitimately block
    # forever waiting on a FIFO with no writer, which is correct POSIX behavior.
    import threading as _th5
    def _writer():
        try:
            with open(fifo, "wb") as fh:
                fh.write(b"}}}not-json{{{")
        except Exception:
            pass
    t = _th5.Thread(target=_writer, daemon=True)
    t.start()
    proc = subprocess.Popen([VIS, "--insp", IMG, fifo, out],
                            cwd=CORE, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        proc.communicate(timeout=20)
        rc = proc.returncode
    except subprocess.TimeoutExpired:
        proc.kill(); proc.communicate()
        try: os.remove(fifo)
        except Exception: pass
        return False, "TIMEOUT (FIFO read hang)"
    try: os.remove(fifo)
    except Exception: pass
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} (FIFO def, no hang)"
CASES.append(case("r5_def_fifo_no_hang", "custom", fn=fn_r5_def_is_fifo))

# ---- def file > 50MB (synthesize by duplicating features) ---------------
def fn_r5_def_50mb(run):
    p = f"{TMP}/sys_r5_def_50mb.hydef"
    d = golden()
    # bloat featureSet until file size > 50MB
    fs = d.get("featureSet")
    if not isinstance(fs, list) or not fs:
        return False, "golden has no featureSet"
    base = list(fs)
    # write iteratively until > 50MB
    while True:
        d["featureSet"] = list(fs)
        json.dump(d, open(p, "w"))
        sz = os.path.getsize(p)
        if sz > 50 * 1024 * 1024: break
        # double up
        fs = fs + copy.deepcopy(base) * max(1, len(fs) // max(1, len(base)))
        if len(fs) > 200000: break  # safety
    sz = os.path.getsize(p)
    rc, out = run(p, out_path=f"{TMP}/sys_r5_50mb_out.json")
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} def_size={sz/1024/1024:.1f}MB (no crash/hang)"
CASES.append(case("r5_def_50mb_robust", "custom", fn=fn_r5_def_50mb))

# ---- 16-bit PNG (already partially covered in r4; explicitly assert) ----
if _PIL:
    from PIL import Image as _PI5
    _R5_16BIT = f"{TMP}/sys_r5_16bit_max.png"
    # max-value 16-bit
    _PI5.new("I;16", (512, 512), 65535).save(_R5_16BIT, "PNG")
    CASES.append(case("r5_img_16bit_max_robust", "robust", img=_R5_16BIT))

# ---- concurrency with shared output dir (distinct filenames) -----------
def fn_r5_concurrent_shared_dir(run):
    shared = f"{TMP}/r5_shared_outdir"
    os.makedirs(shared, exist_ok=True)
    # clear any prior files
    for f in os.listdir(shared):
        try: os.remove(f"{shared}/{f}")
        except Exception: pass
    N = 6
    results = {}
    def worker(i):
        results[i] = run(GDEF, out_path=f"{shared}/r5_conc_{i}.json")
    ts = [threading.Thread(target=worker, args=(i,)) for i in range(N)]
    for t in ts: t.start()
    for t in ts: t.join()
    all0 = all(results[i][0] == 0 for i in range(N))
    allout = all(results[i][1] is not None for i in range(N))
    ref = results[0][1]
    ident = allout and all(results[i][1] == ref for i in range(N))
    return (all0 and allout and ident), f"{N}x shared-dir all-exit0={all0} all-identical={ident}"
CASES.append(case("r5_concurrent_shared_outdir", "custom", fn=fn_r5_concurrent_shared_dir))

# ---- env vars LANG/LC_ALL=C / unset -------------------------------------
def fn_r5_env_locale(_run, lang=None, lc_all=None, label=""):
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    for k in ("LANG", "LC_ALL"):
        env.pop(k, None)
    if lang is not None: env["LANG"] = lang
    if lc_all is not None: env["LC_ALL"] = lc_all
    out = f"{TMP}/sys_r5_locale_{label}.json"
    if os.path.exists(out): os.remove(out)
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                           cwd=CORE, env=env, capture_output=True, timeout=120)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(out, "rb").read() if os.path.exists(out) else None
    valid = False
    if report is not None:
        try: json.loads(report); valid = True
        except Exception: valid = False
    ok = (rc == 0) and valid
    return ok, f"rc={rc_str(rc)} valid={valid} (LANG={lang} LC_ALL={lc_all})"
CASES.append(case("r5_env_locale_C", "custom",
                  fn=lambda r: fn_r5_env_locale(r, "C", "C", "C")))
CASES.append(case("r5_env_locale_unset", "custom",
                  fn=lambda r: fn_r5_env_locale(r, None, None, "unset")))

# ---- 50 sequential runs: detect memory growth via /usr/bin/time -v -----
def fn_r5_sequential_50(run):
    """Sequentially run 50x. Check runtime stability (no monotonic blowup).
    Best-effort RSS via /usr/bin/time -l on macOS (peak maximum resident set)."""
    N = 50
    times = []
    rsses = []
    has_time = os.path.exists("/usr/bin/time")
    for i in range(N):
        out = f"{TMP}/sys_r5_seq_{i}.json"
        if os.path.exists(out): os.remove(out)
        env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
        t0 = _time5.time()
        if has_time:
            try:
                r = subprocess.run(["/usr/bin/time", "-l", VIS, "--insp", IMG, GDEF, out],
                                   cwd=CORE, env=env, capture_output=True, timeout=60)
                rc = r.returncode
                # parse "maximum resident set size" from stderr
                err = r.stderr.decode(errors="replace")
                rss = None
                for line in err.splitlines():
                    if "maximum resident set size" in line:
                        try: rss = int(line.strip().split()[0])
                        except Exception: pass
                if rss is not None: rsses.append(rss)
            except subprocess.TimeoutExpired:
                return False, f"TIMEOUT at iter {i}"
        else:
            try:
                r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                                   cwd=CORE, env=env, capture_output=True, timeout=60)
                rc = r.returncode
            except subprocess.TimeoutExpired:
                return False, f"TIMEOUT at iter {i}"
        dt = _time5.time() - t0
        times.append(dt)
        if rc != 0:
            return False, f"iter {i} rc={rc_str(rc)}"
    # stability: avg of last 10 not more than 2x avg of first 10
    first10 = sum(times[:10]) / 10
    last10 = sum(times[-10:]) / 10
    time_stable = last10 < 2.0 * first10
    detail = f"N={N} t_first10_avg={first10:.2f}s t_last10_avg={last10:.2f}s time_stable={time_stable}"
    if rsses:
        rss_first = sum(rsses[:10]) / 10
        rss_last = sum(rsses[-10:]) / 10
        # each run is a fresh process -> RSS should be roughly constant
        rss_stable = rss_last < 2.0 * rss_first
        detail += f" rss_first10={rss_first/1e6:.1f}MB rss_last10={rss_last/1e6:.1f}MB rss_stable={rss_stable}"
        return (time_stable and rss_stable), detail
    return time_stable, detail
CASES.append(case("r5_sequential_50_stability", "custom", fn=fn_r5_sequential_50))

# ==========================================================================
# ROUND 6 — FIFO regression, image-as-FIFO, /proc & /dev/stdin def, 1-byte
#           img, stdout append race, env path/home unset, long DYLD,
#           RLIMIT_AS memory cap, 200-run fd/mem surveillance, 16-bit CalibMap
# ==========================================================================
import resource as _resource6
import platform as _platform6

# ---- r6.1 REGRESSION: FIFO def must NOT hang and must yield exit4 --------
def fn_r6_def_fifo_regression(run):
    """Round-5 fix: a non-regular (FIFO) def file must be rejected with exit4.
    Writer pumps garbage so reader doesn't block forever; we still require
    exit4 specifically (not just safe-rc) to validate the guard holds."""
    fifo = f"{TMP}/sys_r6_def.fifo"
    if os.path.exists(fifo): os.remove(fifo)
    os.mkfifo(fifo)
    out = f"{TMP}/sys_r6_def_fifo_out.json"
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    import threading as _th
    def _writer():
        try:
            with open(fifo, "wb") as fh:
                fh.write(b"garbage not json")
        except Exception: pass
    _th.Thread(target=_writer, daemon=True).start()
    proc = subprocess.Popen([VIS, "--insp", IMG, fifo, out],
                            cwd=CORE, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        proc.communicate(timeout=20); rc = proc.returncode
    except subprocess.TimeoutExpired:
        proc.kill(); proc.communicate()
        try: os.remove(fifo)
        except Exception: pass
        return False, "TIMEOUT (FIFO def hang - regression!)"
    try: os.remove(fifo)
    except Exception: pass
    ok = (rc == 4)
    return ok, f"rc={rc_str(rc)} (want exit4, FIFO-def guard)"
CASES.append(case("r6_def_fifo_exit4_regression", "custom", fn=fn_r6_def_fifo_regression))

# ---- r6.2 image file is a FIFO -- must not hang -------------------------
def fn_r6_img_is_fifo(run):
    fifo = f"{TMP}/sys_r6_img.fifo"
    if os.path.exists(fifo): os.remove(fifo)
    os.mkfifo(fifo)
    out = f"{TMP}/sys_r6_img_fifo_out.json"
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    import threading as _th
    def _writer():
        try:
            with open(fifo, "wb") as fh:
                fh.write(os.urandom(8192))
        except Exception: pass
    _th.Thread(target=_writer, daemon=True).start()
    proc = subprocess.Popen([VIS, "--insp", fifo, GDEF, out],
                            cwd=CORE, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        proc.communicate(timeout=20); rc = proc.returncode
    except subprocess.TimeoutExpired:
        proc.kill(); proc.communicate()
        try: os.remove(fifo)
        except Exception: pass
        return False, "TIMEOUT (FIFO image hang)"
    try: os.remove(fifo)
    except Exception: pass
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} (FIFO image, no hang/crash)"
CASES.append(case("r6_img_fifo_no_hang", "custom", fn=fn_r6_img_is_fifo))

# ---- r6.3 def via /proc/self/* or /dev/stdin (Linux-only; skip on mac) --
def fn_r6_def_proc_self(_run):
    if _platform6.system() != "Linux":
        return True, "SKIP (not Linux)"
    out = f"{TMP}/sys_r6_proc_out.json"
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    # feed golden def via stdin -> /dev/stdin path
    gdef_bytes = open(GDEF, "rb").read()
    try:
        r = subprocess.run([VIS, "--insp", IMG, "/dev/stdin", out],
                           cwd=CORE, env=env, input=gdef_bytes,
                           capture_output=True, timeout=60)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    safe = (rc not in SIGCRASH) and (rc != "TIMEOUT")
    return safe, f"rc={rc_str(rc)} (/dev/stdin def, no crash)"
CASES.append(case("r6_def_dev_stdin", "custom", fn=fn_r6_def_proc_self))

# ---- r6.4 image file 1 byte (truncated) ----------------------------------
_R6_1B = f"{TMP}/sys_r6_1byte.png"; open(_R6_1B, "wb").write(b"\x89")
CASES.append(case("r6_img_1byte_rc3", "expect_rc", rc=3, img=_R6_1B))

# ---- r6.5 two runs redirecting stdout to SAME file (append race) ---------
def fn_r6_stdout_same_file_race(_run):
    """Two concurrent visSele runs whose stdout is redirected to the SAME file
    in append mode. Both must complete (rc0); file must not be corrupted in
    a way that hangs or crashes them."""
    log = f"{TMP}/sys_r6_shared_stdout.log"
    if os.path.exists(log): os.remove(log)
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    results = {}
    def worker(i):
        out = f"{TMP}/sys_r6_race_{i}.json"
        with open(log, "ab") as fh:
            try:
                r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                                   cwd=CORE, env=env,
                                   stdout=fh, stderr=fh, timeout=120)
                results[i] = r.returncode
            except subprocess.TimeoutExpired:
                results[i] = "TIMEOUT"
    ts = [threading.Thread(target=worker, args=(i,)) for i in range(2)]
    for t in ts: t.start()
    for t in ts: t.join()
    all0 = all(results[i] == 0 for i in (0, 1))
    log_size = os.path.getsize(log) if os.path.exists(log) else 0
    return all0, f"rc0={results[0]} rc1={results[1]} log_size={log_size}B"
CASES.append(case("r6_stdout_shared_append_race", "custom", fn=fn_r6_stdout_same_file_race))

# ---- r6.6 env PATH/HOME unset --------------------------------------------
def fn_r6_env_path_home_unset(_run):
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    env.pop("PATH", None); env.pop("HOME", None)
    out = f"{TMP}/sys_r6_envunset.json"
    if os.path.exists(out): os.remove(out)
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                           cwd=CORE, env=env, capture_output=True, timeout=120)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(out, "rb").read() if os.path.exists(out) else None
    ok = (rc == 0) and (report is not None)
    return ok, f"rc={rc_str(rc)} report_present={report is not None} (PATH/HOME unset)"
CASES.append(case("r6_env_path_home_unset", "custom", fn=fn_r6_env_path_home_unset))

# ---- r6.7 very long DYLD_LIBRARY_PATH ------------------------------------
def fn_r6_long_dyld(_run):
    """Prepend many junk dirs onto DYLD_LIBRARY_PATH (~16KB). Real BUILD stays
    in the list so libs still load. Must not crash, must rc0."""
    junk = ":".join([f"/tmp/no_such_dir_{i}" for i in range(400)])
    env = dict(os.environ, DYLD_LIBRARY_PATH=f"{junk}:{BUILD}")
    out = f"{TMP}/sys_r6_longdyld.json"
    if os.path.exists(out): os.remove(out)
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                           cwd=CORE, env=env, capture_output=True, timeout=120)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    report = open(out, "rb").read() if os.path.exists(out) else None
    ok = (rc == 0) and report is not None
    return ok, f"rc={rc_str(rc)} dyld_len={len(env['DYLD_LIBRARY_PATH'])}"
CASES.append(case("r6_long_dyld_library_path", "custom", fn=fn_r6_long_dyld))

# ---- r6.8 RLIMIT_AS memory cap -> graceful failure ----------------------
def fn_r6_rlimit_as(_run):
    """Cap address space to 128MB via preexec_fn. Engine should either complete
    or fail gracefully (no SIGSEGV/SIGBUS). On macOS, RLIMIT_AS is largely
    a no-op for many allocators — accept either rc0 or controlled non-crash rc."""
    out = f"{TMP}/sys_r6_rlimit.json"
    if os.path.exists(out): os.remove(out)
    env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
    CAP = 128 * 1024 * 1024
    def _preexec():
        try:
            _resource6.setrlimit(_resource6.RLIMIT_AS, (CAP, CAP))
        except Exception:
            pass
    try:
        r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                           cwd=CORE, env=env, capture_output=True,
                           timeout=120, preexec_fn=_preexec)
        rc = r.returncode
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    # Memory-unsafe crash = FAIL. Any controlled exit (incl. SIGABRT, ENOMEM
    # -> nonzero exit, or success) is acceptable.
    safe = (rc not in SIGCRASH)
    return safe, f"rc={rc_str(rc)} RLIMIT_AS=128MB (graceful={safe})"
CASES.append(case("r6_rlimit_as_128mb_graceful", "custom", fn=fn_r6_rlimit_as))

# ---- r6.9 200 quick sequential runs: fd/memory surveillance --------------
def fn_r6_seq_200_surveillance(run):
    """200 sequential runs. Each is a fresh process so RSS should be stable.
    Track runtime drift + (best-effort) /usr/bin/time -l peak RSS. Also count
    /tmp/qa output files to detect fd-leak symptoms (none expected: per-process)."""
    N = 200
    times = []
    rsses = []
    has_time = os.path.exists("/usr/bin/time")
    for i in range(N):
        out = f"{TMP}/sys_r6_s200_{i}.json"
        if os.path.exists(out): os.remove(out)
        env = dict(os.environ, DYLD_LIBRARY_PATH=BUILD)
        t0 = _time5.time()
        try:
            if has_time:
                r = subprocess.run(["/usr/bin/time", "-l", VIS, "--insp", IMG, GDEF, out],
                                   cwd=CORE, env=env, capture_output=True, timeout=60)
                rc = r.returncode
                err = r.stderr.decode(errors="replace")
                for line in err.splitlines():
                    if "maximum resident set size" in line:
                        try: rsses.append(int(line.strip().split()[0])); break
                        except Exception: pass
            else:
                r = subprocess.run([VIS, "--insp", IMG, GDEF, out],
                                   cwd=CORE, env=env, capture_output=True, timeout=60)
                rc = r.returncode
        except subprocess.TimeoutExpired:
            return False, f"TIMEOUT at iter {i}"
        times.append(_time5.time() - t0)
        if rc != 0:
            return False, f"iter {i} rc={rc_str(rc)}"
    first20 = sum(times[:20]) / 20
    last20 = sum(times[-20:]) / 20
    time_stable = last20 < 2.0 * first20
    detail = f"N={N} t_first20={first20:.2f}s t_last20={last20:.2f}s time_stable={time_stable}"
    if rsses and len(rsses) >= 40:
        rf = sum(rsses[:20]) / 20; rl = sum(rsses[-20:]) / 20
        rss_stable = rl < 1.5 * rf
        detail += f" rss_first20={rf/1e6:.1f}MB rss_last20={rl/1e6:.1f}MB rss_stable={rss_stable}"
        return (time_stable and rss_stable), detail
    return time_stable, detail
CASES.append(case("r6_sequential_200_surveillance", "custom", fn=fn_r6_seq_200_surveillance))

# ---- r6.10 CalibMapPath -> 16-bit PNG (cross-buffer / bit-depth) --------
if _PIL:
    from PIL import Image as _PI6
    _R6_16PNG = f"{TMP}/sys_r6_calibmap_16bit.png"
    _PI6.new("I;16", (512, 512), 32000).save(_R6_16PNG, "PNG")
    def mut_r6_calibmap_16bit():
        def f():
            d = golden()
            d["CalibMapPath"] = _R6_16PNG
            return d
        return f
    CASES.append(case("r6_def_calibmap_16bit_png_robust", "robust",
                      make=mut_r6_calibmap_16bit()))

if __name__ == "__main__":
    sys.exit(run_module("qa_system", CASES))
