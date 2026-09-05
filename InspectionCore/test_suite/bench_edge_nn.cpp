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
  }
  return 0;
}
