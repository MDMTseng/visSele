"""Edge-profile NN experiment, numpy only (torch does not load on this box).

Input : one caliper's along-averaged intensity profile, resampled to N=64 samples,
        two channels: raw/255 and per-profile z-score.
Output: a heatmap over the 64 samples (where the taught edge is) and one logit "there is an edge".
Labels: the fit consensus. target sample = sel + r/step (r = signed px from the hit to the fitted
        line/arc). st=2 (inlier) and st=1 (outlier with a target inside the window) are positives
        with a Gaussian heatmap at the target; st=0 (miss) and out-of-window targets are "no edge".
Split : by recipe (hash of the name), ~20% held out.
Eval  : localisation error vs the consensus for inliers (px), how often the NN lands within 1.5 px
        of the consensus on OUTLIER calipers (the current selector is wrong there by definition),
        and edge/no-edge separation.
"""
import json, sys, math, hashlib, time
import numpy as np

N = 512   # native resolution, zero-padded + masked; longer windows are dropped
PATH = sys.argv[1] if len(sys.argv) > 1 else '_nn_profiles.jsonl'
EPOCHS = int(sys.argv[2]) if len(sys.argv) > 2 else 25
rng = np.random.default_rng(0)

# ---------------------------------------------------------------- data
def load(path):
    X, T, HAS, META = [], [], [], []
    with open(path, encoding='utf-8') as f:
        for line in f:
            d = json.loads(line)
            raw = np.asarray(d['raw'], dtype=np.float32)
            n = len(raw)
            if n < 8 or n > N or d['sel'] is None: continue
            scale = 1.0
            r = np.zeros(N, np.float32); r[:n] = raw
            z = np.zeros(N, np.float32); z[:n] = (raw - raw.mean()) / (raw.std() + 1e-3)
            mask = np.pad(np.ones(n, np.float32), (0, N - n))
            # INTENT channels: where the operator taught the edge (window centre), which side of
            # the wire (polarity) and which of several edges (method). Without them the net can
            # only learn 'the most edge-like place', and whole recipes with two edges in the
            # window went to the other one (PS8095003S: 20 px, 100 calipers).
            pos = np.zeros(N, np.float32); pos[:n] = (np.arange(n) - (n - 1) / 2) / 32.0
            polv = {'rising': 1.0, 'falling': -1.0}.get(d['pol'], 0.0)
            meth = d['method'] or 'strongest'
            const = lambda v: np.full(N, v, np.float32) * mask
            x = np.stack([r / 255.0, z, mask, pos * mask, const(polv),
                          const(1.0 if meth == 'strongest' else 0.0), const(1.0 if meth == 'first' else 0.0), const(1.0 if meth == 'last' else 0.0)], 0)
            st = d['st'] if d['st'] is not None else 0
            sel = d['sel']
            has, t = 0.0, -1.0
            # r = px along the search dir from the hit to the FIT, so the fit sits at sel + r/step.
            # Only a primitive that measured (pst 0) with a sane residual is a label.
            if sel >= 0 and st in (1, 2) and d['pst'] == 0 and abs(d['r'] or 0.0) <= 3.0:
                tt = (sel + (d['r'] or 0.0) / d['step']) * scale
                if 0 <= tt <= N - 1: has, t = 1.0, tt
            elif sel >= 0 and st in (1, 2):
                continue   # unlabeled: neither positive nor a clean 'no edge'
            X.append(x); T.append(t); HAS.append(has)
            META.append((d['recipe'], d['type'], st, sel * scale if sel >= 0 else -1, d['step'] / scale, d['pol'], d['method'], n, d['ms'] if d['ms'] is not None else 30.0))
    return np.stack(X), np.asarray(T, np.float32), np.asarray(HAS, np.float32), META

t0 = time.time()
X, T, HAS, META = load(PATH)
recipes = np.array([m[0] for m in META])
hold = np.array([int(hashlib.md5(r.encode()).hexdigest(), 16) % 5 == 0 for r in recipes])
print(f'{len(X)} profiles, {hold.sum()} held out ({len(set(recipes[hold]))} recipes), positives {int(HAS.sum())}, load {time.time()-t0:.1f}s')

def heat(t):
    """Gaussian heatmap (sigma 1 sample) at t, zeros for no edge."""
    H = np.zeros((len(t), N), np.float32)
    idx = np.arange(N)[None, :]
    m = t >= 0
    H[m] = np.exp(-0.5 * (idx - t[m, None]) ** 2)
    return H

# ---------------------------------------------------------------- model: conv(2->16,k7) relu conv(16->16,k7) relu conv(16->16,k7) relu conv(16->1,k1) ; has-edge = w . maxpool + b
def conv_init(cin, cout, k):
    return (rng.standard_normal((cout, cin, k)) * math.sqrt(2.0 / (cin * k))).astype(np.float32), np.zeros(cout, np.float32)

K = 7
P = {}
P['w1'], P['b1'] = conv_init(8, 16, K)
P['w2'], P['b2'] = conv_init(16, 16, K)
P['w3'], P['b3'] = conv_init(16, 16, K)
P['w4'], P['b4'] = conv_init(16, 1, 1)
P['wh'] = (rng.standard_normal(16) * 0.1).astype(np.float32); P['bh'] = np.zeros(1, np.float32)
nparam = sum(v.size for v in P.values()); print('params', nparam)

def im2col(x, k):                       # x: (B,C,N) -> (B, C*k, N) with zero pad
    B, C, L = x.shape; p = k // 2
    xp = np.pad(x, ((0, 0), (0, 0), (p, p)))
    cols = np.stack([xp[:, :, i:i + L] for i in range(k)], 2)   # (B,C,k,N)
    return cols.reshape(B, C * k, L)

def conv_f(x, w, b):
    B, C, L = x.shape; cout, cin, k = w.shape
    cols = im2col(x, k)                                          # (B, C*k, N)
    y = np.einsum('oc,bcn->bon', w.reshape(cout, cin * k), cols) + b[None, :, None]
    return y, cols

def conv_b(dy, cols, w):
    B, cout, L = dy.shape; _, cin, k = w.shape
    dw = np.einsum('bon,bcn->oc', dy, cols).reshape(cout, cin, k)
    db = dy.sum((0, 2))
    dcols = np.einsum('oc,bon->bcn', w.reshape(cout, cin * k), dy)  # (B, cin*k, N)
    dcols = dcols.reshape(B, cin, k, L); p = k // 2
    dx = np.zeros((B, cin, L + 2 * p), np.float32)
    for i in range(k): dx[:, :, i:i + L] += dcols[:, :, i, :]
    return dx[:, :, p:p + L], dw, db

def forward(x):
    c = {}
    a1, c['c1'] = conv_f(x, P['w1'], P['b1']); r1 = np.maximum(a1, 0)
    a2, c['c2'] = conv_f(r1, P['w2'], P['b2']); r2 = np.maximum(a2, 0)
    a3, c['c3'] = conv_f(r2, P['w3'], P['b3']); r3 = np.maximum(a3, 0)
    hm, c['c4'] = conv_f(r3, P['w4'], P['b4']); hm = hm[:, 0, :]
    mask = x[:, 2, :] > 0.5
    hm = np.where(mask, hm, -30.0)
    r3m = np.where(mask[:, None, :], r3, 0.0)
    pooled = r3m.max(2); c['argmax'] = r3m.argmax(2)
    hl = pooled @ P['wh'] + P['bh']
    c.update(a1=a1, a2=a2, a3=a3, r3=r3, pooled=pooled)
    return hm, hl, c

def sigmoid(v): return 1 / (1 + np.exp(-v))

def backward(x, c, dhm, dhl):
    G = {}
    # has-edge head
    G['wh'] = c['pooled'].T @ dhl; G['bh'] = dhl.sum(0, keepdims=True)[0:1]
    dpooled = dhl[:, None] * P['wh'][None, :]
    dr3 = np.zeros_like(c['r3'])
    B, C, L = dr3.shape
    bi, ci = np.meshgrid(np.arange(B), np.arange(C), indexing='ij')
    dr3[bi, ci, c['argmax']] += dpooled
    # heatmap head
    dx4, G['w4'], G['b4'] = conv_b(dhm[:, None, :], c['c4'], P['w4'])
    dr3 += dx4
    da3 = dr3 * (c['a3'] > 0)
    dr2, G['w3'], G['b3'] = conv_b(da3, c['c3'], P['w3'])
    da2 = dr2 * (c['a2'] > 0)
    dr1, G['w2'], G['b2'] = conv_b(da2, c['c2'], P['w2'])
    da1 = dr1 * (c['a1'] > 0)
    _, G['w1'], G['b1'] = conv_b(da1, c['c1'], P['w1'])
    return G

# Adam
M = {k: np.zeros_like(v) for k, v in P.items()}; V = {k: np.zeros_like(v) for k, v in P.items()}
def adam(G, lr, t):
    for k in P:
        g = G[k].reshape(P[k].shape)
        M[k] = 0.9 * M[k] + 0.1 * g; V[k] = 0.999 * V[k] + 0.001 * g * g
        P[k] -= lr * (M[k] / (1 - 0.9 ** t)) / (np.sqrt(V[k] / (1 - 0.999 ** t)) + 1e-8)

def augment(x, t):
    """Brightness scale/offset, noise, and a random extra step (a 'speck') on a positive."""
    x = x.copy()
    B = len(x)
    gain = rng.uniform(0.6, 1.2, (B, 1)); off = rng.uniform(-0.1, 0.1, (B, 1))
    raw = np.clip(x[:, 0, :] * gain + off, 0, 1)
    raw += rng.normal(0, rng.uniform(0, 0.02, (B, 1)), raw.shape)
    # speck: a narrow dark dip at a random place, on half the batch
    spk = rng.random(B) < 0.5
    pos = rng.integers(3, 60, B); wid = rng.integers(1, 4, B); dep = rng.uniform(0.05, 0.4, B)
    idx = np.arange(N)[None, :]
    dip = np.exp(-0.5 * ((idx - pos[:, None]) / wid[:, None]) ** 2) * dep[:, None]
    raw = np.where(spk[:, None], np.clip(raw - dip, 0, 1), raw)
    # second edge: a smoothed step of random sign/height at a random place >= 6 samples from the target,
    # on a third of the batch. The target does not move: the operator's edge is still the one meant.
    m = x[:, 2, :] > 0.5
    spl = rng.random(B) < 0.33
    n_valid = m.sum(1)
    pos2 = (rng.random(B) * np.maximum(n_valid - 6, 1)).astype(int) + 3
    far = np.abs(pos2 - np.where(t >= 0, t, -100)) >= 6
    spl &= far
    h2 = rng.uniform(0.15, 0.6, B) * np.where(rng.random(B) < 0.5, 1, -1)
    w2 = rng.uniform(0.7, 2.0, B)
    stepf = 0.5 * (1 + np.tanh((idx - pos2[:, None]) / w2[:, None])) * h2[:, None]
    raw = np.where(spl[:, None], np.clip(raw + stepf, 0, 1), raw)
    raw = np.where(m, raw, 0.0)
    cnt = m.sum(1, keepdims=True); mu = raw.sum(1, keepdims=True) / cnt
    sd = np.sqrt((np.where(m, (raw - mu) ** 2, 0.0)).sum(1, keepdims=True) / cnt) + 1e-3
    z = np.where(m, (raw - mu) / sd, 0.0)
    return np.concatenate([np.stack([raw, z], 1), x[:, 2:, :]], 1).astype(np.float32), t

# ---------------------------------------------------------------- train
tr = ~hold
Xtr, Ttr, Htr = X[tr], T[tr], HAS[tr]
BS = 128; step = 0; lr = 2e-3
for ep in range(EPOCHS):
    perm = rng.permutation(len(Xtr)); tot = 0.0
    for i in range(0, len(perm), BS):
        b = perm[i:i + BS]
        xb, tb = augment(Xtr[b], Ttr[b]); hb = Htr[b]
        Hb = heat(tb)
        hm, hl, c = forward(xb)
        # heatmap: BCE per sample; has-edge: BCE
        p = sigmoid(hm); q = sigmoid(hl)
        loss = -np.where(xb[:, 2, :] > 0.5, Hb * np.log(p + 1e-7) + (1 - Hb) * np.log(1 - p + 1e-7), 0.0).sum(1).mean() * 0.1 \
               - (hb * np.log(q + 1e-7) + (1 - hb) * np.log(1 - q + 1e-7)).mean()
        mask = xb[:, 2, :] > 0.5
        dhm = np.where(mask, (p - Hb), 0.0) / len(b) * 0.1; dhl = (q - hb) / len(b)
        G = backward(xb, c, dhm.astype(np.float32), dhl.astype(np.float32))
        step += 1; adam(G, lr * (0.5 if ep > EPOCHS * 0.7 else 1.0), step)
        tot += loss * len(b)
    print(f'epoch {ep+1:2d} loss {tot/len(perm):.4f}  {time.time()-t0:.0f}s', flush=True)

# ---------------------------------------------------------------- eval on held-out recipes
def predict(Xe):
    outs_t, outs_q = [], []
    for i in range(0, len(Xe), 1024):
        hm, hl, _ = forward(Xe[i:i + 1024])
        # soft-argmax in a 5-sample window around the max
        am = hm.argmax(1)
        idx = np.arange(N)[None, :]
        w = np.exp(hm - hm.max(1, keepdims=True)) * (np.abs(idx - am[:, None]) <= 2)
        t = (w * idx).sum(1) / w.sum(1)
        outs_t.append(t); outs_q.append(sigmoid(hl))
    return np.concatenate(outs_t), np.concatenate(outs_q)

def rule_select(raw255, pol, ms, method):
    """edge_select as the core does it: central-diff gradient, polarity, strict local max of |g| above max(ms, 0.15*gmax), then method."""
    g = np.zeros_like(raw255); g[1:-1] = raw255[2:] - raw255[:-2]; g[0] = g[1]; g[-1] = g[-2]
    gmax = np.abs(g).max(); floor = max(ms, 0.15 * gmax)
    peaks = []
    for i in range(1, len(g) - 1):
        gi = g[i]
        if pol == 'rising' and gi <= 0: continue
        if pol == 'falling' and gi >= 0: continue
        a, b, c = abs(g[i-1]), abs(gi), abs(g[i+1])
        if not (b >= a and b >= c and (b > a or b > c)): continue
        if b <= floor: continue
        den = a - 2*b + c; delta = 0.5*(a - c)/den if den != 0 else 0.0
        peaks.append((i + max(-1, min(1, delta)), b))
    if not peaks: return -1.0
    if method == 'first': return peaks[0][0]
    if method == 'last': return peaks[-1][0]
    return max(peaks, key=lambda p_: p_[1])[0]

def speck_test(name, Xe, Te, Me, depth):
    """Add one speck (dark dip, width 1-3, given depth in 0..1 units) at a random place >= 5 px from the target on every
    held-out inlier profile; where does the rule land, where does the NN land?"""
    r2 = np.random.default_rng(1)
    inl = np.array([m[2] == 2 for m in Me]) & (Te >= 0)
    Xs = Xe[inl].copy(); Ts = Te[inl]; Ms = [m for m, k in zip(Me, inl) if k]
    n_valid = (Xs[:, 2, :] > 0.5).sum(1)
    B = len(Xs); idx = np.arange(N)[None, :]
    pos = np.array([r2.integers(3, max(4, nv - 3)) for nv in n_valid])
    ok = np.abs(pos - Ts) >= 5
    wid = r2.integers(1, 4, B); dip = np.exp(-0.5 * ((idx - pos[:, None]) / wid[:, None]) ** 2) * depth
    raw = np.clip(Xs[:, 0, :] - dip, 0, 1) * (Xs[:, 2, :] > 0.5)
    mu = raw.sum(1, keepdims=True) / n_valid[:, None]
    sd = np.sqrt(((raw - mu) ** 2 * (Xs[:, 2, :] > 0.5)).sum(1, keepdims=True) / n_valid[:, None]) + 1e-3
    Xs[:, 0, :] = raw; Xs[:, 1, :] = np.where(Xs[:, 2, :] > 0.5, (raw - mu) / sd, 0)
    t, q = predict(Xs)
    err_nn = np.abs(t - Ts)
    err_rule = np.array([abs(rule_select(raw[i, :n_valid[i]] * 255.0, Ms[i][5], Ms[i][8], Ms[i][6]) - Ts[i]) for i in range(B)])
    sel_clean = np.array([m[3] for m in Ms]); err_clean = np.abs(sel_clean - Ts)
    print(f'  speck depth {depth:.2f} ({ok.sum()} profiles): rule taken away (>1.5 px) {100*(err_rule[ok] > 1.5).mean():.1f}%  NN {100*(err_nn[ok] > 1.5).mean():.1f}%   '
          f'medians rule {np.median(err_rule[ok]):.2f} NN {np.median(err_nn[ok]):.2f} px   (clean rule was {100*(err_clean[ok] > 1.5).mean():.1f}% >1.5px)')

def report(name, Xe, Te, He, Me):
    t, q = predict(Xe)
    st = np.array([m[2] for m in Me]); sel = np.array([m[3] for m in Me]); pxs = np.array([m[4] for m in Me])  # px per resampled sample
    inl = (st == 2) & (Te >= 0); outl = (st == 1) & (Te >= 0); miss = (st == 0)
    err_nn = np.abs(t - Te) * pxs; err_sel = np.abs(sel - Te) * pxs
    ns = np.array([m[7] for m in Me])
    print(f'\n== {name}: {len(Xe)} calipers  inliers {inl.sum()}  outliers {outl.sum()}  misses {miss.sum()}  window samples median {np.median(ns):.0f} max {ns.max()}')
    print(f'  inliers : |NN - fit| median {np.median(err_nn[inl]):.2f} px, 90% {np.percentile(err_nn[inl],90):.2f} px   |selector - fit| median {np.median(err_sel[inl]):.2f}, 90% {np.percentile(err_sel[inl],90):.2f}')
    for thr in (1.0, 1.5, 3.0):
        print(f'  outliers: NN within {thr} px of the fit {100*(err_nn[outl] <= thr).mean():.1f}%   (selector {100*(err_sel[outl] <= thr).mean():.1f}%)')
    print(f'  edge score q: inliers mean {q[inl].mean():.2f}, outliers {q[outl].mean():.2f}, misses {q[miss].mean():.2f};  q<0.5 on misses {100*(q[miss]<0.5).mean():.1f}%, q>=0.5 on inliers {100*(q[inl]>=0.5).mean():.1f}%')
    # by primitive type
    # offenders: recipes whose inlier error is far off, with their step and window
    rec = np.array([m[0] for m in Me]); stp = np.array([m[4] for m in Me])
    rows = []
    for rname in sorted(set(rec[inl])):
        m = inl & (rec == rname)
        if m.sum() >= 5 and np.median(err_nn[m]) > 2.0:
            rows.append((np.median(err_nn[m]), rname, int(m.sum()), float(np.median(stp[m])), int(np.median(ns[m])), float(np.median(err_sel[m]))))
    for r_ in sorted(rows, reverse=True)[:8]: print('  offender', r_[1], 'n', r_[2], 'NN med %.1f px' % r_[0], 'sel med %.2f' % r_[5], 'step %.2f px' % r_[3], 'window', r_[4])
    for ty in ('line', 'arc'):
        m = inl & (np.array([x[1] for x in Me]) == ty)
        if m.sum(): print(f'  {ty:4s} inliers {m.sum():5d}: NN median {np.median(err_nn[m]):.2f} px  selector {np.median(err_sel[m]):.2f} px')
    for depth in (0.1, 0.2, 0.4): speck_test(name, Xe, Te, Me, depth)

report('held-out recipes', X[hold], T[hold], HAS[hold], [m for m, h in zip(META, hold) if h])
report('training recipes', X[tr], T[tr], HAS[tr], [m for m, h in zip(META, tr) if h])
np.savez('_nn_weights.npz', **P)
print('saved _nn_weights.npz')
