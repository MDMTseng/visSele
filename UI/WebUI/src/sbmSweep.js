// Robustness sweep: how far can the scene drift before the locator stops coping?
//
// The axes and the step maths live here rather than in the panel because they
// are the part with an answer. What the panel does with the numbers is taste;
// which numbers they are is not.
//
// THE PERTURBATION IS THE GROUND TRUTH.
//
// This is what makes a sweep more than "did it still find it". Rotate the image
// by a known theta and the locator MUST report theta -- so every step carries
// its own reference, with no golden measurement to maintain and nothing to
// trust but arithmetic. A step that locates with a high score and the wrong
// angle is a worse outcome than one that fails outright, and only the residual
// tells them apart.
import { angleDelta } from './sbmInspectResult';

// Each axis says what it perturbs, over what range, and in what units. `key` is
// the field TestPerturb.h parses; `neutral` is the value that means "no
// perturbation" and is what step 0 (the baseline) uses.
export const SWEEP_AXES = {
  rot: {
    label: '旋轉 rotation', key: 'rot_deg', unit: '°', neutral: 0,
    from: -10, to: 10, steps: 11,
    // The only axis with a ground truth today: a rotation of the scene has to
    // come back as the same rotation of the pose.
    expect: (v) => v,
    hint: '影像繞中心轉。定位器回報的角度應該剛好等於這個值,差多少就是定位誤差。',
  },
  gain: {
    label: '亮度 gain', key: 'gain', unit: '×', neutral: 1,
    from: 0.5, to: 1.6, steps: 12,
    hint: '像素乘上這個倍率(會飽和)。模擬照明變亮/變暗或鏡頭髒污。',
  },
  bias: {
    label: 'offset bias', key: 'bias', unit: 'lv', neutral: 0,
    from: -40, to: 40, steps: 9,
    hint: '整張圖加減灰階。模擬背光漏光或暗電流漂移。',
  },
  noise: {
    label: '雜訊 noise', key: 'noise', unit: 'σ', neutral: 0,
    from: 0, to: 30, steps: 11,
    hint: '高斯雜訊,在幾何變換之後加(和感測器一樣)。',
  },
  scale: {
    label: '縮放 scale', key: 'scale', unit: '×', neutral: 1,
    from: 0.9, to: 1.1, steps: 11,
    hint: '模擬工作距離或鏡頭倍率跑掉。',
  },
  skew: {
    label: '歪斜 skew', key: 'skew', unit: '', neutral: 0,
    from: -0.1, to: 0.1, steps: 11,
    hint: '相機沒有正對零件時的一階近似。',
  },
};

// The step values, baseline FIRST.
//
// The baseline is not decoration: every other step is read against it. The
// locator's angle at rest is not necessarily 0 (the part sits how it sits), so
// a residual computed against 0 instead of against the baseline would report
// the part's own mounting angle as localization error on every single step.
export function sweepValues(axis, from, to, steps) {
  const A = SWEEP_AXES[axis];
  if (!A) return [];
  const n = Math.max(2, Math.min(41, Math.round(steps)));
  const out = [A.neutral];
  for (let i = 0; i < n; i++) {
    const v = from + (to - from) * (i / (n - 1));
    // Skip a step that IS the baseline -- it would be run twice and shown twice.
    if (Math.abs(v - A.neutral) > 1e-9) out.push(v);
  }
  return out;
}

export function perturbFor(axis, value, seed) {
  const A = SWEEP_AXES[axis];
  if (!A) return null;
  if (Math.abs(value - A.neutral) <= 1e-9) return null;   // baseline: no perturb at all
  // A fixed seed per sweep, not per step: the noise field must not change
  // between two steps of a gain sweep, or the curve mixes two variables.
  return { [A.key]: value, seed };
}

// One step's verdict, given the summary and the baseline summary.
export function sweepRow(axis, value, sum, base) {
  const A = SWEEP_AXES[axis];
  const row = {
    value, located: !!(sum && sum.located),
    sim: sum && sum.located ? sum.pose.similarity : NaN,
    ok: sum ? sum.counts.ok : 0,
    na: sum ? sum.counts.na + sum.counts.ng : 0,
    why: sum ? sum.why : '',
    residual: NaN,
  };
  if (!row.located || !base || !base.located) return row;

  if (A && A.expect) {
    // found - baseline is what the SCENE did; expect(value) is what we asked
    // it to do. angleDelta keeps the comparison honest across +/-pi.
    const moved = angleDelta(sum.pose.rotate, base.pose.rotate) * 180 / Math.PI;
    row.moved = moved;
    row.expected = A.expect(value);
    row.residual = moved - row.expected;
    // An INVERTED sign convention somewhere between here and
    // getRotationMatrix2D shows up as moved = -expected, i.e. a residual of
    // MINUS twice the applied value. That is not "the locator is twice as bad
    // as it looks" -- it is a bug in this file or in the core's angle
    // convention, and it produces a confident linear error curve that somebody
    // would otherwise act on. Flagged, not absorbed.
    row.signSuspect = Math.abs(row.expected) > 1e-6 &&
                      Math.abs(row.residual + 2 * row.expected) < Math.abs(row.expected) * 0.25;
  }
  return row;
}

// What the sweep found, as a sentence. The table is for reading a step; this is
// for reading the sweep.
export function sweepVerdict(axis, rows) {
  const A = SWEEP_AXES[axis] || {};
  const done = rows.filter((r) => r);
  if (!done.length) return '';
  const base = done.find((r) => Math.abs(r.value - A.neutral) <= 1e-9);
  const lost = done.filter((r) => !r.located);
  const u = A.unit || '';

  if (base && !base.located)
    return '連沒有擾動的原圖都定位不到 — 先把單次檢驗弄過再跑掃描。';

  // The usable range is the run of steps around the baseline that all located.
  const sorted = [...done].sort((a, b) => a.value - b.value);
  const bi = sorted.findIndex((r) => Math.abs(r.value - A.neutral) <= 1e-9);
  let lo = bi, hi = bi;
  while (lo > 0 && sorted[lo - 1].located) lo--;
  while (hi < sorted.length - 1 && sorted[hi + 1].located) hi++;

  const fmt = (v) => (Math.abs(v) >= 10 ? v.toFixed(0) : v.toFixed(2));
  let s = `連續可定位範圍 ${fmt(sorted[lo].value)}${u} ～ ${fmt(sorted[hi].value)}${u}`;
  if (!lost.length) s += '(整段都過,可以把範圍拉大再測)';
  const sims = done.filter((r) => r.located && Number.isFinite(r.sim)).map((r) => r.sim);
  if (sims.length) s += `,分數 ${Math.min(...sims).toFixed(3)}～${Math.max(...sims).toFixed(3)}`;
  const res = done.filter((r) => Number.isFinite(r.residual)).map((r) => Math.abs(r.residual));
  if (res.length) s += `,角度殘差最大 ${Math.max(...res).toFixed(3)}°`;
  return s;
}
