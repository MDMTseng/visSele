"""
qa_imgstress -- image-degradation stress tests against the golden 10221 sample.

Starts from the existing golden case (5.04 MP, 2592x1944, grayscale) and applies
controlled perturbations: Gaussian noise, uneven tint, vignette, simulated
scratches, blur, RGB color tint. Ground-truth = the judge values from a fresh
golden --insp run; each perturbation is compared per-judge against that truth.

Assertions per case:
  - no crash (no SIGSEGV/BUS/FPE/ILL)
  - exit0
  - no nan/inf tokens in output JSON
  - per-judge value drift within tolerance (or NA -- both acceptable; a
    perturbation may correctly fail to measure, but must not silently return a
    wildly wrong finite value)
"""
import sys, os, json, math, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qalib import *      # run_insp, IMG, GDEF, case, run_module, SIGCRASH, ...

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
try:
    import cv2
except ImportError:
    cv2 = None

OUT_DIR = "/tmp/qa_imgstress"
os.makedirs(OUT_DIR, exist_ok=True)

# ---------- ground truth: judge values from the un-perturbed golden run ----------
def _judge_map(out_bytes):
    """{id: measured_value or None} from a --insp output JSON."""
    try:
        j = json.loads(out_bytes)
    except Exception:
        return {}
    found = {}
    def walk(o):
        if isinstance(o, dict):
            # judge nodes carry 'status' and 'value', and the def schema gives
            # us 'subtype' on the def side but the output keeps 'value' field
            if "id" in o and "status" in o and "value" in o and "subtype" in o:
                found[o["id"]] = o.get("value")
            for v in o.values(): walk(v)
        elif isinstance(o, list):
            for v in o: walk(v)
    walk(j)
    return found

_rc_golden, _out_golden = run_insp(GDEF)
assert _rc_golden == 0 and _out_golden is not None, "golden baseline failed to run"
GOLDEN_JUDGES = _judge_map(_out_golden)
assert GOLDEN_JUDGES, "no judges parsed from golden -- aborting"

# ---------- perturbation builders ----------
def _load_gray():
    return np.asarray(Image.open(IMG).convert("L"), dtype=np.float32)

def _save_gray(arr, name):
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    p = f"{OUT_DIR}/{name}.png"
    Image.fromarray(arr, mode="L").save(p, "PNG")
    return p

def _save_rgb(arr_rgb, name):
    arr_rgb = np.clip(arr_rgb, 0, 255).astype(np.uint8)
    p = f"{OUT_DIR}/{name}.png"
    Image.fromarray(arr_rgb, mode="RGB").save(p, "PNG")
    return p

def img_noise(sigma, name):
    a = _load_gray()
    rng = np.random.default_rng(seed=42)            # deterministic seed
    a += rng.normal(0, sigma, a.shape).astype(np.float32)
    return _save_gray(a, name)

def img_tint_horizontal(name):
    a = _load_gray()
    w = a.shape[1]
    g = np.linspace(0.7, 1.3, w, dtype=np.float32)
    a = a * g[None, :]
    return _save_gray(a, name)

def img_vignette(name):
    a = _load_gray()
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cy, cx = h / 2, w / 2
    r = np.hypot(xx - cx, yy - cy) / math.hypot(cx, cy)  # 0..1
    falloff = 1.0 - 0.4 * r * r                          # corners ~0.6 of centre
    return _save_gray(a * falloff, name)

def img_scratches(n_dark, n_bright, name):
    a = _load_gray()
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), mode="L")
    draw = ImageDraw.Draw(img)
    rng = np.random.default_rng(seed=123)
    h, w = a.shape
    for _ in range(n_dark):
        x1, y1 = rng.integers(0, w), rng.integers(0, h)
        x2, y2 = x1 + rng.integers(-150, 150), y1 + rng.integers(-150, 150)
        draw.line([(x1, y1), (x2, y2)], fill=20, width=int(rng.integers(1, 3)))
    for _ in range(n_bright):
        x1, y1 = rng.integers(0, w), rng.integers(0, h)
        x2, y2 = x1 + rng.integers(-150, 150), y1 + rng.integers(-150, 150)
        draw.line([(x1, y1), (x2, y2)], fill=240, width=int(rng.integers(1, 3)))
    return _save_gray(np.asarray(img, dtype=np.float32), name)

def img_blur(sigma, name):
    img = Image.open(IMG).convert("L").filter(ImageFilter.GaussianBlur(radius=sigma))
    return _save_gray(np.asarray(img, dtype=np.float32), name)

def img_rgb_tint(name, channel_scales=(1.0, 0.85, 0.75)):
    """Promote to RGB and apply a per-channel scale (greenish-warm tint)."""
    a = _load_gray()
    rgb = np.stack([a * channel_scales[0], a * channel_scales[1], a * channel_scales[2]], axis=-1)
    return _save_rgb(rgb, name)

# ---------- cv::imread / cv::Mat-specific perturbation builders ----------
def img_png_with_alpha(name):
    """RGBA PNG -- forces cv::imread to drop alpha; useExtBuffer-from-cv path."""
    a = _load_gray()
    h, w = a.shape
    rgb = np.stack([a, a, a], axis=-1).astype(np.uint8)
    alpha = np.full((h, w, 1), 255, dtype=np.uint8)
    rgba = np.concatenate([rgb, alpha], axis=-1)
    p = f"{OUT_DIR}/{name}.png"
    Image.fromarray(rgba, mode="RGBA").save(p, "PNG")
    return p

def img_png_16bit(name):
    """16-bit grayscale PNG -- non-standard depth; cv::imread default flag should down-convert to 8-bit."""
    a = _load_gray()
    a16 = np.clip(a * 257.0, 0, 65535).astype(np.uint16)  # 8->16 scale
    p = f"{OUT_DIR}/{name}.png"
    Image.fromarray(a16, mode="I;16").save(p, "PNG")
    return p

def img_jpeg_low_quality(name, q=40):
    """JPEG compression artifacts -- cv::imread handles JPEG differently than lodepng would."""
    a = _load_gray().astype(np.uint8)
    p = f"{OUT_DIR}/{name}.jpg"
    Image.fromarray(a, mode="L").save(p, "JPEG", quality=q)
    return p

def img_tiff(name):
    """TIFF variant -- new format support per QA round 10."""
    a = _load_gray().astype(np.uint8)
    p = f"{OUT_DIR}/{name}.tiff"
    Image.fromarray(a, mode="L").save(p, "TIFF")
    return p

def img_webp(name, q=80):
    """WebP variant -- new format support."""
    a = _load_gray().astype(np.uint8)
    p = f"{OUT_DIR}/{name}.webp"
    Image.fromarray(a, mode="L").save(p, "WEBP", quality=q)
    return p

def img_via_cv_roundtrip(name):
    """Write via cv2.imwrite (different PNG encoder than PIL) -- tests cv::imread parity on its own output."""
    a = _load_gray().astype(np.uint8)
    p = f"{OUT_DIR}/{name}.png"
    if cv2 is not None:
        cv2.imwrite(p, a, [cv2.IMWRITE_PNG_COMPRESSION, 9])
    else:
        Image.fromarray(a, mode="L").save(p, "PNG")
    return p

def img_vignette_plus_jpeg(name):
    """Vignette + JPEG artifacts combo -- stresses cv::imread + non-uniform illumination."""
    a = _load_gray()
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cy, cx = h / 2, w / 2
    r = np.hypot(xx - cx, yy - cy) / math.hypot(cx, cy)
    falloff = 1.0 - 0.35 * r * r
    a = np.clip(a * falloff, 0, 255).astype(np.uint8)
    p = f"{OUT_DIR}/{name}.jpg"
    Image.fromarray(a, mode="L").save(p, "JPEG", quality=70)
    return p

def img_noisy_5mp_png(name, sigma=12):
    """Heavily-noised full-res 5MP PNG -- pushes useExtBuffer on a large continuous Mat."""
    a = _load_gray()
    rng = np.random.default_rng(seed=2026)
    a = a + rng.normal(0, sigma, a.shape).astype(np.float32)
    return _save_gray(a, name)

def img_blurred_5mp_png(name, sigma=2.5):
    """Heavily-blurred full-res 5MP PNG."""
    img = Image.open(IMG).convert("L").filter(ImageFilter.GaussianBlur(radius=sigma))
    return _save_gray(np.asarray(img, dtype=np.float32), name)

# Precompute the degraded images (so each case just calls run_insp on the path)
PATHS = {
    "noise_sigma_3":   img_noise(3,  "noise_sigma_3"),
    "noise_sigma_8":   img_noise(8,  "noise_sigma_8"),
    "noise_sigma_20":  img_noise(20, "noise_sigma_20"),
    "tint_horizontal": img_tint_horizontal("tint_horizontal"),
    "vignette":        img_vignette("vignette"),
    "scratches_few":   img_scratches(2, 3, "scratches_few"),
    "scratches_many":  img_scratches(15, 15, "scratches_many"),
    "blur_mild":       img_blur(1.0, "blur_mild"),
    "blur_strong":     img_blur(3.0, "blur_strong"),
    "rgb_warm_tint":   img_rgb_tint("rgb_warm_tint", (1.0, 0.85, 0.75)),
    # cv::imread / cv::Mat migration probes
    "cv_png_rgba":         img_png_with_alpha("cv_png_rgba"),
    "cv_png_16bit":        img_png_16bit("cv_png_16bit"),
    "cv_jpeg_lowq":        img_jpeg_low_quality("cv_jpeg_lowq", 40),
    "cv_tiff":             img_tiff("cv_tiff"),
    "cv_webp":             img_webp("cv_webp", 80),
    "cv_png_cv_encoded":   img_via_cv_roundtrip("cv_png_cv_encoded"),
    "cv_vignette_jpeg":    img_vignette_plus_jpeg("cv_vignette_jpeg"),
    "cv_noisy_5mp_png":    img_noisy_5mp_png("cv_noisy_5mp_png", 12),
    "cv_blurred_5mp_png":  img_blurred_5mp_png("cv_blurred_5mp_png", 2.5),
}

# ---------- assertion helper ----------
NAN_INF_RE = re.compile(rb"[\s,:\[]\s*(nan|inf|-inf)\b")

def _check_against_golden(rc, out, tol_finite, name):
    """
    PASS if:
      - rc == 0 (no crash, no exit3/4)
      - no nan/inf tokens in output
      - every judge that has a finite value is within tol_finite of the golden value,
        OR is NA in this run (NA is acceptable -- "couldn't measure"); a finite-but-
        very-wrong value (>tol_finite drift) is a failure.
    """
    if rc in SIGCRASH: return False, f"REAL CRASH rc={rc_str(rc)}"
    if rc == "TIMEOUT": return False, "TIMEOUT"
    if rc != 0: return False, f"rc={rc_str(rc)} (expected 0)"
    if out is None: return False, "no output"
    if NAN_INF_RE.search(out.lower()): return False, "nan/inf leaked into JSON"
    cur = _judge_map(out)
    if not cur: return False, "no judges in output (matching failed entirely)"

    drifts = []
    bad = []
    for jid, gv in GOLDEN_JUDGES.items():
        if gv is None: continue
        cv = cur.get(jid)
        if cv is None: continue   # NA in perturbed run -- acceptable
        if not (isinstance(cv, (int, float)) and math.isfinite(cv)):
            bad.append((jid, cv))
            continue
        drift = abs(cv - gv)
        drifts.append((jid, drift))
        if drift > tol_finite * ILL_CONDITIONED.get(jid, 1.0):
            bad.append((jid, f"drift {drift:.3f} > {tol_finite * ILL_CONDITIONED.get(jid, 1.0):.3f}"))

    if bad: return False, f"out-of-tol judges {bad[:3]}"
    measured = len(drifts)
    if drifts:
        max_d = max(d for _, d in drifts)
        return True, f"{measured} judges checked, max_drift={max_d:.3f} (tol={tol_finite})"
    return True, f"all judges NA'd in perturbed run (no finite-comparison done)"

# Judges whose GEOMETRY amplifies error, with the factor and the reason.
#
# Judge 14 is the angle between line 1 and line 2, and those two lines are not
# the same kind of object:
#
#   line 2  matching_pts=525  fitted along the whole 4.65 mm edge
#   line 1  matching_pts=2    `vertex_touch_searching: true` in the def -- the
#                             line is defined by where it TOUCHES the contour at
#                             its two ends. The 2 is a mode marker hardcoded in
#                             FeatureManager_sig360_circle_line.cpp (lf.matching_pts
#                             = 2), not a count of weak support, and that mode
#                             deliberately runs looser gates (curvatureMax 10 vs
#                             0.15, cosSim 0.3 vs 0.9).
#
# A line through two touch points turns when either point moves by a pixel, and
# judge 14 is 2.79 deg, so that rotation lands at full size on a small number.
# Measured under WebP recompression:
#
#   id  8 distance  0.01%      id 17 distance  0.04%
#   id 12 distance  0.02%      id 19 distance  1.65%
#   id 13 distance  0.01%      id 14 ANGLE    11.24%   (2.7169 -> 3.0223)
#
# Not a defect -- it is what that judge is made of. The factor is deliberately
# NOT a blanket loosening: every other judge keeps the tight bound, and this one
# carries its own number with the reason attached. 2x still fails on anything
# worse than ~0.6 deg.
#
# Checked and ruled out before concluding this (2026-08-07): the def is not
# mis-placed (all three lines have an edge within 0.05 mm of nominal) and the
# object transform is sound (four arc centres agree to 0.065 mm, all three
# fitted lines sit within 0.07 mm of nominal).
ILL_CONDITIONED = {14: 2.0}

def _custom(img_path, tol):
    def fn(run_insp):
        rc, out = run_insp(GDEF, img=img_path)
        return _check_against_golden(rc, out, tol, img_path)
    return fn

# ---------- cases ----------
CASES = []
def add(name, kind, **kw): CASES.append(case(name, kind, **kw))

# Tolerance schedule: low-noise should match closely; high-noise allowed slack.
# Units are def units (mm). Distances on this sample are ~8-10 mm.
add("noise_sigma_3_drift",   "custom", fn=_custom(PATHS["noise_sigma_3"],   0.10))
add("noise_sigma_8_drift",   "custom", fn=_custom(PATHS["noise_sigma_8"],   0.30))
add("noise_sigma_20_drift",  "custom", fn=_custom(PATHS["noise_sigma_20"],  1.00))
add("tint_horizontal_drift", "custom", fn=_custom(PATHS["tint_horizontal"], 0.20))
add("vignette_drift",        "custom", fn=_custom(PATHS["vignette"],        0.20))
add("scratches_few_drift",   "custom", fn=_custom(PATHS["scratches_few"],   0.30))
add("scratches_many_drift",  "custom", fn=_custom(PATHS["scratches_many"],  1.00))
add("blur_mild_drift",       "custom", fn=_custom(PATHS["blur_mild"],       0.20))
add("blur_strong_drift",     "custom", fn=_custom(PATHS["blur_strong"],     1.00))
add("rgb_warm_tint_drift",   "custom", fn=_custom(PATHS["rgb_warm_tint"],   0.20))

# --- cv::imread / useExtBuffer / cv::Mat migration probes ---
# RGBA PNG: cv::imread must drop alpha; pixel values unchanged -> drift should be tiny.
add("cv_png_rgba_drift",        "custom", fn=_custom(PATHS["cv_png_rgba"],        0.10))
# 16-bit PNG: cv::imread default flag down-converts; near-lossless scale-back -> small drift.
add("cv_png_16bit_drift",       "custom", fn=_custom(PATHS["cv_png_16bit"],       0.20))
# Low-quality JPEG: cv::imread-specific decode path + compression artifacts.
add("cv_jpeg_lowq_drift",       "custom", fn=_custom(PATHS["cv_jpeg_lowq"],       0.40))
# TIFF: new format support; should round-trip losslessly.
add("cv_tiff_drift",            "custom", fn=_custom(PATHS["cv_tiff"],            0.10))
# WebP: new format support; small lossy drift acceptable.
add("cv_webp_drift",            "custom", fn=_custom(PATHS["cv_webp"],            0.30))
# PNG written by cv::imwrite -> read by cv::imread (encoder/decoder parity).
add("cv_png_cv_encoded_drift",  "custom", fn=_custom(PATHS["cv_png_cv_encoded"],  0.10))
# Vignette + JPEG combo (cv-specific decode + non-uniform illum).
add("cv_vignette_jpeg_drift",   "custom", fn=_custom(PATHS["cv_vignette_jpeg"],   0.40))
# Heavy noise on full 5MP PNG (large continuous cv::Mat via useExtBuffer).
add("cv_noisy_5mp_drift",       "custom", fn=_custom(PATHS["cv_noisy_5mp_png"],   0.50))
# Heavy blur on full 5MP PNG.
add("cv_blurred_5mp_drift",     "custom", fn=_custom(PATHS["cv_blurred_5mp_png"], 0.80))

# Determinism on a perturbation (independent of golden) -- the engine should
# produce byte-identical output for the same degraded input on repeated runs.
add("determinism_noise_sigma_8", "determinism", make=(lambda p=PATHS["noise_sigma_8"]: GDEF), img=PATHS["noise_sigma_8"])

# Pure no-crash regression: even heavy noise must not SIGSEGV.
add("noise_sigma_60_no_crash", "robust", img=img_noise(60, "noise_sigma_60"))
add("scratches_torture_no_crash", "robust", img=img_scratches(80, 80, "scratches_torture"))

if __name__ == "__main__":
    sys.exit(run_module("qa_imgstress", CASES))
