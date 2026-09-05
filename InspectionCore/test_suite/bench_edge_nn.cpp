// Edge-NN forward pass, C++: how long does one caliper profile take?
//
// Three layouts of the same 4,562-parameter net (conv 8->16 k7, 16->16 k7, 16->16 k7,
// 16->1 k1, has-edge head on the max-pool), checked against a numpy reference:
//   scalar   plain loops, -O2, no vector flags (what a naive port gets)
//   autovec  same loops with the channel dimension innermost, -O3 -mavx2 -mfma
//   avx2     hand-written intrinsics: 16 channels = two __m256 accumulators per output sample
//
// BUILD (MSYS2 MinGW64):
//   export PATH=/c/msys64/mingw64/bin:$PATH
//   g++ -std=c++17 -O3 -mavx2 -mfma -o /tmp/bench_nn.exe test_suite/bench_edge_nn.cpp
//   /tmp/bench_nn.exe
#include <immintrin.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include "edge_nn_weights.h"

static constexpr int C_IN = 8, C = 16, K = 7, PAD = 3;

// ---------------------------------------------------------------- layout helpers
// Activations are stored sample-major: a[l*C + c]  (channel innermost, 16 floats = 64 B = one cache line).
// Weights for a k7 layer are re-laid as w[tap][cin][cout] so the inner product over cout is contiguous.
struct Net {
  std::vector<float> w1, w2, w3;   // [K][cin][C]
  std::vector<float> w4;           // [C]
  Net() {
    auto relay = [](const float *w, int cout, int cin, int k) {
      std::vector<float> o((size_t)k * cin * cout);
      for (int co = 0; co < cout; co++) for (int ci = 0; ci < cin; ci++) for (int t = 0; t < k; t++)
        o[((size_t)t * cin + ci) * cout + co] = w[((size_t)co * cin + ci) * k + t];
      return o;
    };
    w1 = relay(NN_w1, C, C_IN, K); w2 = relay(NN_w2, C, C, K); w3 = relay(NN_w3, C, C, K);
    w4.assign(NN_w4, NN_w4 + C);
  }
};

// One k7 layer, channel-innermost, written so the compiler can vectorise over cout.
template <int CIN>
static void conv7_autovec(const float *in, int L, const float *w, const float *b, float *out) {
  for (int l = 0; l < L; l++) {
    float acc[C];
    for (int c = 0; c < C; c++) acc[c] = b[c];
    for (int t = 0; t < K; t++) {
      int s = l + t - PAD; if (s < 0 || s >= L) continue;
      const float *x = in + (size_t)s * CIN;
      const float *wt = w + (size_t)t * CIN * C;
      for (int ci = 0; ci < CIN; ci++) {
        float xv = x[ci]; const float *wr = wt + (size_t)ci * C;
        for (int c = 0; c < C; c++) acc[c] += xv * wr[c];
      }
    }
    float *o = out + (size_t)l * C;
    for (int c = 0; c < C; c++) o[c] = acc[c] > 0 ? acc[c] : 0;
  }
}

// Same layer with AVX2/FMA: two 8-lane accumulators hold the 16 output channels.
template <int CIN>
static void conv7_avx2(const float *in, int L, const float *w, const float *b, float *out) {
  const __m256 b0 = _mm256_loadu_ps(b), b1 = _mm256_loadu_ps(b + 8), z = _mm256_setzero_ps();
  for (int l = 0; l < L; l++) {
    __m256 a0 = b0, a1 = b1;
    int t0 = std::max(0, PAD - l), t1 = std::min(K, L - l + PAD);
    for (int t = t0; t < t1; t++) {
      const float *x = in + (size_t)(l + t - PAD) * CIN;
      const float *wt = w + (size_t)t * CIN * C;
      for (int ci = 0; ci < CIN; ci++) {
        __m256 xv = _mm256_broadcast_ss(x + ci);
        a0 = _mm256_fmadd_ps(xv, _mm256_loadu_ps(wt + (size_t)ci * C), a0);
        a1 = _mm256_fmadd_ps(xv, _mm256_loadu_ps(wt + (size_t)ci * C + 8), a1);
      }
    }
    _mm256_storeu_ps(out + (size_t)l * C, _mm256_max_ps(a0, z));
    _mm256_storeu_ps(out + (size_t)l * C + 8, _mm256_max_ps(a1, z));
  }
}

// Scalar reference in the numpy layout (channel-major, w[co][ci][k]) -- the naive port.
static void conv7_scalar(const float *in, int L, int cin, const float *w, const float *b, float *out) {
  for (int co = 0; co < C; co++) for (int l = 0; l < L; l++) {
    float acc = b[co];
    for (int ci = 0; ci < cin; ci++) for (int t = 0; t < K; t++) {
      int s = l + t - PAD; if (s < 0 || s >= L) continue;
      acc += in[(size_t)ci * L + s] * w[((size_t)co * cin + ci) * K + t];
    }
    out[(size_t)co * L + l] = acc > 0 ? acc : 0;
  }
}


// Four samples per pass, 8 independent accumulators: the 2-accumulator kernel above is
// latency-bound (each FMA waits ~4 cycles on its own register); interior blocks skip the
// boundary tests altogether.
template <int CIN>
static void conv7_avx2_x4(const float *in, int L, const float *w, const float *b, float *out) {
  const __m256 b0 = _mm256_loadu_ps(b), b1 = _mm256_loadu_ps(b + 8), z = _mm256_setzero_ps();
  int L4 = L & ~3, l = 0;
  for (; l + 4 <= L4; l += 4) {
    __m256 a00 = b0, a01 = b1, a10 = b0, a11 = b1, a20 = b0, a21 = b1, a30 = b0, a31 = b1;
    const bool edge = (l < PAD) || (l + 3 + PAD >= L);
    for (int t = 0; t < K; t++) {
      const float *wt = w + (size_t)t * CIN * C;
      const int s0 = l + t - PAD;
      const float *x0 = in + (ptrdiff_t)s0 * CIN, *x1 = x0 + CIN, *x2 = x1 + CIN, *x3 = x2 + CIN;
      if (!edge) {
        for (int ci = 0; ci < CIN; ci++) {
          const __m256 w0 = _mm256_loadu_ps(wt + (size_t)ci * C), w1 = _mm256_loadu_ps(wt + (size_t)ci * C + 8);
          __m256 xv;
          xv = _mm256_broadcast_ss(x0 + ci); a00 = _mm256_fmadd_ps(xv, w0, a00); a01 = _mm256_fmadd_ps(xv, w1, a01);
          xv = _mm256_broadcast_ss(x1 + ci); a10 = _mm256_fmadd_ps(xv, w0, a10); a11 = _mm256_fmadd_ps(xv, w1, a11);
          xv = _mm256_broadcast_ss(x2 + ci); a20 = _mm256_fmadd_ps(xv, w0, a20); a21 = _mm256_fmadd_ps(xv, w1, a21);
          xv = _mm256_broadcast_ss(x3 + ci); a30 = _mm256_fmadd_ps(xv, w0, a30); a31 = _mm256_fmadd_ps(xv, w1, a31);
        }
      } else {
        const bool v0 = s0 >= 0 && s0 < L, v1 = s0 + 1 >= 0 && s0 + 1 < L, v2 = s0 + 2 >= 0 && s0 + 2 < L, v3 = s0 + 3 >= 0 && s0 + 3 < L;
        for (int ci = 0; ci < CIN; ci++) {
          const __m256 w0 = _mm256_loadu_ps(wt + (size_t)ci * C), w1 = _mm256_loadu_ps(wt + (size_t)ci * C + 8);
          if (v0) { __m256 xv = _mm256_broadcast_ss(x0 + ci); a00 = _mm256_fmadd_ps(xv, w0, a00); a01 = _mm256_fmadd_ps(xv, w1, a01); }
          if (v1) { __m256 xv = _mm256_broadcast_ss(x1 + ci); a10 = _mm256_fmadd_ps(xv, w0, a10); a11 = _mm256_fmadd_ps(xv, w1, a11); }
          if (v2) { __m256 xv = _mm256_broadcast_ss(x2 + ci); a20 = _mm256_fmadd_ps(xv, w0, a20); a21 = _mm256_fmadd_ps(xv, w1, a21); }
          if (v3) { __m256 xv = _mm256_broadcast_ss(x3 + ci); a30 = _mm256_fmadd_ps(xv, w0, a30); a31 = _mm256_fmadd_ps(xv, w1, a31); }
        }
      }
    }
    float *o = out + (size_t)l * C;
    _mm256_storeu_ps(o,      _mm256_max_ps(a00, z)); _mm256_storeu_ps(o + 8,  _mm256_max_ps(a01, z));
    _mm256_storeu_ps(o + 16, _mm256_max_ps(a10, z)); _mm256_storeu_ps(o + 24, _mm256_max_ps(a11, z));
    _mm256_storeu_ps(o + 32, _mm256_max_ps(a20, z)); _mm256_storeu_ps(o + 40, _mm256_max_ps(a21, z));
    _mm256_storeu_ps(o + 48, _mm256_max_ps(a30, z)); _mm256_storeu_ps(o + 56, _mm256_max_ps(a31, z));
  }
  for (; l < L; l++) {   // tail, <4 samples
    __m256 a0 = b0, a1 = b1;
    const int t0 = std::max(0, PAD - l), t1 = std::min(K, L - l + PAD);
    for (int t = t0; t < t1; t++) {
      const float *x = in + (size_t)(l + t - PAD) * CIN; const float *wt = w + (size_t)t * CIN * C;
      for (int ci = 0; ci < CIN; ci++) { __m256 xv = _mm256_broadcast_ss(x + ci);
        a0 = _mm256_fmadd_ps(xv, _mm256_loadu_ps(wt + (size_t)ci * C), a0); a1 = _mm256_fmadd_ps(xv, _mm256_loadu_ps(wt + (size_t)ci * C + 8), a1); }
    }
    _mm256_storeu_ps(out + (size_t)l * C, _mm256_max_ps(a0, z)); _mm256_storeu_ps(out + (size_t)l * C + 8, _mm256_max_ps(a1, z));
  }
}

// Sample-vectorised: 8 consecutive samples per register, channel-major activations with zero
// padding (row stride S = L' + 2*PAD), weights broadcast from the numpy layout. 8 output channels
// = 8 independent accumulators per block.
template <int CIN>
static void conv7_samplevec(const float *inp, int Lc /*samples to compute*/, int S /*row stride*/, const float *w, const float *b, float *out /*[C][Lc]*/) {
  const __m256 z = _mm256_setzero_ps();
  for (int l = 0; l < Lc; l += 8) {
    for (int cog = 0; cog < C; cog += 8) {
      __m256 acc[8];
      for (int j = 0; j < 8; j++) acc[j] = _mm256_set1_ps(b[cog + j]);
      for (int ci = 0; ci < CIN; ci++) {
        const float *xr = inp + (size_t)ci * S + l;
        for (int t = 0; t < K; t++) {
          const __m256 xv = _mm256_loadu_ps(xr + t);
          for (int j = 0; j < 8; j++) acc[j] = _mm256_fmadd_ps(xv, _mm256_broadcast_ss(w + (((size_t)(cog + j) * CIN + ci) * K + t)), acc[j]);
        }
      }
      for (int j = 0; j < 8; j++) _mm256_storeu_ps(out + (size_t)(cog + j) * Lc + l, _mm256_max_ps(acc[j], z));
    }
  }
}

struct Out { std::vector<float> hm; float hl; };

static Out forward_scalar(const float *x /*[C_IN][L]*/, int L) {
  std::vector<float> a((size_t)C * L), bb((size_t)C * L);
  conv7_scalar(x, L, C_IN, NN_w1, NN_b1, a.data());
  conv7_scalar(a.data(), L, C, NN_w2, NN_b2, bb.data());
  conv7_scalar(bb.data(), L, C, NN_w3, NN_b3, a.data());
  Out o; o.hm.resize(L); float pooled[C];
  for (int c = 0; c < C; c++) { float m = 0; for (int l = 0; l < L; l++) m = std::max(m, a[(size_t)c * L + l]); pooled[c] = m; }
  for (int l = 0; l < L; l++) { float s = NN_b4[0]; for (int c = 0; c < C; c++) s += a[(size_t)c * L + l] * NN_w4[c]; o.hm[l] = s; }
  o.hl = NN_bh[0]; for (int c = 0; c < C; c++) o.hl += pooled[c] * NN_wh[c];
  return o;
}

static Out forward_x4(const Net &n, const float *x, int L, float *buf) {
  float *a = buf, *bb = buf + (size_t)L * C;
  conv7_avx2_x4<C_IN>(x, L, n.w1.data(), NN_b1, a); conv7_avx2_x4<C>(a, L, n.w2.data(), NN_b2, bb); conv7_avx2_x4<C>(bb, L, n.w3.data(), NN_b3, a);
  Out o; o.hm.resize(L); float pooled[C] = {0};
  for (int l = 0; l < L; l++) { const float *r = a + (size_t)l * C; float s = NN_b4[0];
    for (int c = 0; c < C; c++) { s += r[c] * n.w4[c]; pooled[c] = std::max(pooled[c], r[c]); } o.hm[l] = s; }
  o.hl = NN_bh[0]; for (int c = 0; c < C; c++) o.hl += pooled[c] * NN_wh[c];
  return o;
}
// Channel-major, zero padding rebuilt between layers (those copies are part of the cost).
// Lc = L rounded up to 8 so every block is a full register; the extra samples read zeros.
static Out forward_samplevec(const float *x /*[C_IN][L]*/, int L, float *buf) {
  const int Lc = ((L + 7) / 8) * 8, S = Lc + 2 * PAD + 8;
  float *xin = buf, *p1 = xin + (size_t)C_IN * S, *p2 = p1 + (size_t)C * S, *a = p2 + (size_t)C * S;   // a: [C][Lc]
  std::memset(xin, 0, sizeof(float) * (size_t)C_IN * S);
  for (int c = 0; c < C_IN; c++) std::memcpy(xin + (size_t)c * S + PAD, x + (size_t)c * L, sizeof(float) * L);
  auto pad_in = [&](const float *src, float *dst) { std::memset(dst, 0, sizeof(float) * (size_t)C * S); for (int c = 0; c < C; c++) std::memcpy(dst + (size_t)c * S + PAD, src + (size_t)c * Lc, sizeof(float) * L); };
  conv7_samplevec<C_IN>(xin, Lc, S, NN_w1, NN_b1, a);
  pad_in(a, p1); conv7_samplevec<C>(p1, Lc, S, NN_w2, NN_b2, a);
  pad_in(a, p2); conv7_samplevec<C>(p2, Lc, S, NN_w3, NN_b3, a);
  Out o; o.hm.resize(L); float pooled[C] = {0};
  for (int c = 0; c < C; c++) { float m = 0; for (int l = 0; l < L; l++) m = std::max(m, a[(size_t)c * Lc + l]); pooled[c] = m; }
  for (int l = 0; l < L; l++) { float s = NN_b4[0]; for (int c = 0; c < C; c++) s += a[(size_t)c * Lc + l] * NN_w4[c]; o.hm[l] = s; }
  o.hl = NN_bh[0]; for (int c = 0; c < C; c++) o.hl += pooled[c] * NN_wh[c];
  return o;
}
static size_t samplevec_buf(int L) { const int Lc = ((L + 7) / 8) * 8, S = Lc + 2 * PAD + 8; return (size_t)(C_IN + 2 * C) * S + (size_t)C * Lc; }

template <bool AVX>
static Out forward_fast(const Net &n, const float *x /*[L][C_IN]*/, int L, float *buf /*2*L*C*/) {
  float *a = buf, *bb = buf + (size_t)L * C;
  if (AVX) { conv7_avx2<C_IN>(x, L, n.w1.data(), NN_b1, a); conv7_avx2<C>(a, L, n.w2.data(), NN_b2, bb); conv7_avx2<C>(bb, L, n.w3.data(), NN_b3, a); }
  else     { conv7_autovec<C_IN>(x, L, n.w1.data(), NN_b1, a); conv7_autovec<C>(a, L, n.w2.data(), NN_b2, bb); conv7_autovec<C>(bb, L, n.w3.data(), NN_b3, a); }
  Out o; o.hm.resize(L); float pooled[C] = {0};
  for (int l = 0; l < L; l++) {
    const float *r = a + (size_t)l * C; float s = NN_b4[0];
    for (int c = 0; c < C; c++) { s += r[c] * n.w4[c]; pooled[c] = std::max(pooled[c], r[c]); }
    o.hm[l] = s;
  }
  o.hl = NN_bh[0]; for (int c = 0; c < C; c++) o.hl += pooled[c] * NN_wh[c];
  return o;
}

int main() {
  Net net;
  // ---- correctness against the numpy reference (REF_X is [C_IN][L] channel-major)
  {
    int L = REF_L;
    std::vector<float> xt((size_t)L * C_IN);
    for (int c = 0; c < C_IN; c++) for (int l = 0; l < L; l++) xt[(size_t)l * C_IN + c] = REF_X[(size_t)c * L + l];
    std::vector<float> buf((size_t)2 * L * C);
    Out s = forward_scalar(REF_X, L), v = forward_fast<false>(net, xt.data(), L, buf.data()), a = forward_fast<true>(net, xt.data(), L, buf.data());
    std::vector<float> buf2(samplevec_buf(L));
    Out x4 = forward_x4(net, xt.data(), L, buf.data()), sv = forward_samplevec(REF_X, L, buf2.data());
    float e4 = 0, e5 = 0; for (int l = 0; l < L; l++) { e4 = std::max(e4, std::fabs(x4.hm[l] - REF_HM[l])); e5 = std::max(e5, std::fabs(sv.hm[l] - REF_HM[l])); }
    printf("check x4 %.2e samplevec %.2e  hl %.5f / %.5f\n", e4, e5, x4.hl, sv.hl);
    float e1 = 0, e2 = 0, e3 = 0;
    for (int l = 0; l < L; l++) { e1 = std::max(e1, std::fabs(s.hm[l] - REF_HM[l])); e2 = std::max(e2, std::fabs(v.hm[l] - REF_HM[l])); e3 = std::max(e3, std::fabs(a.hm[l] - REF_HM[l])); }
    printf("check vs numpy: max|dhm| scalar %.2e autovec %.2e avx2 %.2e   hl %.5f / %.5f / %.5f (ref %.5f)\n",
           e1, e2, e3, s.hl, v.hl, a.hl, REF_HL);
  }
  // ---- timing
  const int Ls[] = {46, 68, 114, 200};
  for (int L : Ls) {
    std::vector<float> xc((size_t)C_IN * L), xt((size_t)L * C_IN), buf((size_t)2 * L * C);
    for (size_t i = 0; i < xc.size(); i++) xc[i] = (float)((i * 2654435761u) % 1000) / 1000.f;
    for (int c = 0; c < C_IN; c++) for (int l = 0; l < L; l++) xt[(size_t)l * C_IN + c] = xc[(size_t)c * L + l];
    auto bench = [&](const char *name, auto fn) {
      const int reps = 2000; volatile float sink = 0;
      for (int i = 0; i < 50; i++) sink += fn().hl;
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < reps; i++) sink += fn().hl;
      double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / reps;
      double macs = (double)L * (16 * 8 * 7 + 16 * 16 * 7 * 2 + 16);
      printf("  L=%3d %-8s %7.1f us/profile  %5.2f GMAC/s\n", L, name, us, macs / us / 1e3);
    };
    printf("L=%d (%.0f kMAC)\n", L, (double)L * (16 * 8 * 7 + 16 * 16 * 7 * 2 + 16) / 1e3);
    bench("scalar",  [&] { return forward_scalar(xc.data(), L); });
    bench("autovec", [&] { return forward_fast<false>(net, xt.data(), L, buf.data()); });
    bench("avx2",    [&] { return forward_fast<true>(net, xt.data(), L, buf.data()); });
    std::vector<float> buf2(samplevec_buf(L));
    bench("avx2_x4", [&] { return forward_x4(net, xt.data(), L, buf.data()); });
    bench("samplev", [&] { return forward_samplevec(xc.data(), L, buf2.data()); });
  }
  return 0;
}
