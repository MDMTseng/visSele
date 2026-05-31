#include "LabelingCV.h"
#include <opencv2/opencv.hpp>
#include <vector>

static void labeling_impl(cv::Mat &Pic, cv::Mat &labelOut, std::vector<acv_LabeledData> &ld, int connectivity)
{
  int W = Pic.cols, H = Pic.rows;
  ld.clear();
  if (W <= 0 || H <= 0) return;
  if (Pic.type() != CV_8UC1 && Pic.type() != CV_8UC3) return;

  // Mask for cv::connectedComponentsWithStats: nonzero = foreground.
  //   CV_8UC1 input: bg=255 / fg=0, invert in place. (Phase 1 fast path.)
  //   CV_8UC3 input: BGR-replicated grayscale, extract R then invert.
  cv::Mat m;
  if (Pic.type() == CV_8UC1) {
    cv::bitwise_not(Pic, m);
  } else {
    cv::Mat r_ch; cv::extractChannel(Pic, r_ch, 2);
    cv::bitwise_not(r_ch, m);
  }

  cv::Mat lbl, stats, cent;
  int n = cv::connectedComponentsWithStats(m, lbl, stats, cent, connectivity, CV_32S);

  // cage = foreground component with largest bbox -> label 1; objects -> 2,3,...
  int cage = -1; long bestbb = -1;
  for (int L = 1; L < n; L++)
  {
    long bb = (long)stats.at<int>(L, cv::CC_STAT_WIDTH) * stats.at<int>(L, cv::CC_STAT_HEIGHT);
    if (bb > bestbb) { bestbb = bb; cage = L; }
  }
  std::vector<int> remap(n, 0);
  int nxt = 2;
  for (int L = 1; L < n; L++) remap[L] = (L == cage) ? 1 : nxt++;

  ld.resize(nxt);
  for (int i = 0; i < nxt; i++) { ld[i].area = 0; ld[i].misc = 0; }
  for (int L = 1; L < n; L++)
  {
    int idx = remap[L];
    acv_LabeledData &d = ld[idx];
    int left = stats.at<int>(L, cv::CC_STAT_LEFT), top = stats.at<int>(L, cv::CC_STAT_TOP);
    int w = stats.at<int>(L, cv::CC_STAT_WIDTH), h = stats.at<int>(L, cv::CC_STAT_HEIGHT);
    d.area = stats.at<int>(L, cv::CC_STAT_AREA);
    d.LTBound = acv_XY((float)left, (float)top);
    d.RBBound = acv_XY((float)(left + w - 1), (float)(top + h - 1));
    d.Center  = acv_XY((float)cent.at<double>(L, 0), (float)cent.at<double>(L, 1));
  }

  // Pack labels back into BGR.
  //   L == 0   -> (255, 255, 255)  (background)
  //   L  > 0   -> (idx&0xFF, (idx>>8)&0xFF, (idx>>16)&0xFF)
  // If caller didn't pass a separate buffer (legacy single-arg overload), pack
  // in place into Pic; cv::Mat::create reallocates Pic to CV_8UC3 if needed.
  cv::Mat &out = labelOut.empty() && Pic.type() == CV_8UC3 ? Pic : labelOut;
  if (&out == &labelOut) {
    out.create(H, W, CV_8UC3);
  } else {
    // in-place CV_8UC1 -> CV_8UC3 reallocates; that's the legacy path.
    Pic.create(H, W, CV_8UC3);
  }
  cv::parallel_for_(cv::Range(0, H), [&](const cv::Range &rng){
    for (int y = rng.start; y < rng.end; y++)
    {
      const int *ll = lbl.ptr<int>(y);
      unsigned char *dst = out.ptr<unsigned char>(y);
      for (int x = 0; x < W; x++)
      {
        int L = ll[x];
        unsigned char *p = dst + x * 3;
        if (L == 0) { p[0] = 255; p[1] = 255; p[2] = 255; }
        else {
          int idx = remap[L];
          p[0] = (unsigned char)(idx);
          p[1] = (unsigned char)(idx >> 8);
          p[2] = (unsigned char)(idx >> 16);
        }
      }
    }
  });
}

void acvComponentLabeling_cv(cv::Mat &Pic, std::vector<acv_LabeledData> &ld, int connectivity)
{
  cv::Mat noOut;
  labeling_impl(Pic, noOut, ld, connectivity);
}

void acvComponentLabeling_cv(cv::Mat &Pic, cv::Mat &labelOut, std::vector<acv_LabeledData> &ld, int connectivity)
{
  labeling_impl(Pic, labelOut, ld, connectivity);
}

// ---------- Phase 2: combined CCL + morph boundary + dual-sig signature ----------

void buildLabeledSignatures_phase2(const cv::Mat &binary_uc1,
                                   std::vector<acv_LabeledData> &ld,
                                   std::vector<std::vector<acv_XY>> &perLabelSignature,
                                   std::vector<std::vector<acv_XY>> &perLabelCartesian,
                                   int N_bins, int K_min, int connectivity)
{
  ld.clear();
  perLabelSignature.clear();
  perLabelCartesian.clear();
  const int H = binary_uc1.rows, W = binary_uc1.cols;
  if (W <= 0 || H <= 0 || binary_uc1.type() != CV_8UC1) return;

  // --- Step 1: CCL (gives 32S labels image + per-label centroids/bbox/area).
  cv::Mat mask; cv::bitwise_not(binary_uc1, mask);   // bg=255/fg=0  ->  bg=0/fg=255
  cv::Mat labels32, stats, centroids;
  int n = cv::connectedComponentsWithStats(mask, labels32, stats, centroids, connectivity, CV_32S);
  if (n <= 1) return;

  // Cage = largest-bbox foreground component -> label 1; other objects -> 2..nxt-1.
  int cage = -1; long bestbb = -1;
  for (int L = 1; L < n; L++)
  {
    long bb = (long)stats.at<int>(L, cv::CC_STAT_WIDTH) * stats.at<int>(L, cv::CC_STAT_HEIGHT);
    if (bb > bestbb) { bestbb = bb; cage = L; }
  }
  std::vector<int> remap(n, 0);
  int nxt = 2;
  for (int L = 1; L < n; L++) remap[L] = (L == cage) ? 1 : nxt++;

  // Resize outputs to numRealLabels = nxt.
  const int NL = nxt;
  ld.resize(NL);
  perLabelCartesian.resize(NL);
  perLabelSignature.assign(NL, std::vector<acv_XY>(N_bins, acv_XY(0.f, 0.f)));
  for (int i = 0; i < NL; i++) { ld[i].area = 0; ld[i].misc = 0; }

  // Fill per-label metadata.
  for (int L = 1; L < n; L++)
  {
    int idx = remap[L];
    acv_LabeledData &d = ld[idx];
    int left = stats.at<int>(L, cv::CC_STAT_LEFT), top = stats.at<int>(L, cv::CC_STAT_TOP);
    int w = stats.at<int>(L, cv::CC_STAT_WIDTH), h = stats.at<int>(L, cv::CC_STAT_HEIGHT);
    d.area = stats.at<int>(L, cv::CC_STAT_AREA);
    d.LTBound = acv_XY((float)left, (float)top);
    d.RBBound = acv_XY((float)(left + w - 1), (float)(top + h - 1));
    d.Center  = acv_XY((float)centroids.at<double>(L, 0), (float)centroids.at<double>(L, 1));
  }

  // --- Step 2: boundary mask via morph: boundary = mask & ~erode(mask).
  cv::Mat eroded, boundary;
  cv::erode(mask, eroded, cv::Mat(), cv::Point(-1,-1), 1, cv::BORDER_REPLICATE);
  cv::bitwise_xor(mask, eroded, boundary);

  // --- Step 3: per-label dual signatures.
  // Per-pixel hot loop optimisations:
  //   - max-R ranking works on R^2 (monotonic), so we skip sqrt on every pixel
  //     and store A/B as R^2; one sqrt per bin at the end.
  //   - acvFAtan2 (Rajan-Wang polynomial, |err| ~ 0.0015 rad ~ 0.09 deg) is
  //     way cheaper than std::atan2f and signature binning at 1deg / bin
  //     doesn't need atan2 precision.
  //   - span needs 1/R; do ONE sqrt per pixel for it (not the per-pixel sqrt
  //     we'd otherwise pay for R itself).
  // A[L][bin]: max R^2 over direct-hit pixels in that bin (anchors).
  // B[L][bin]: max R^2 over any pixel whose angular span covers this bin.
  std::vector<std::vector<float>> A(NL, std::vector<float>(N_bins, 0.0f));
  std::vector<std::vector<float>> B(NL, std::vector<float>(N_bins, 0.0f));

  const float TWO_PI = 2.0f * (float)M_PI;
  const float BINS_OVER_2PI = (float)N_bins / TWO_PI;
  for (int y = 0; y < H; y++)
  {
    const unsigned char *bnd = boundary.ptr<unsigned char>(y);
    const int *lbl = labels32.ptr<int>(y);
    for (int x = 0; x < W; x++)
    {
      if (!bnd[x]) continue;
      int Lraw = lbl[x]; if (Lraw <= 0) continue;
      int L = remap[Lraw]; if (L < 1 || L >= NL) continue;
      float dx = (float)x - ld[L].Center.x;
      float dy = (float)y - ld[L].Center.y;
      float R2 = dx*dx + dy*dy;
      if (R2 < 1e-6f) continue;
      // Polynomial atan2 approximation: error well under 0.1deg, fine for
      // signature binning. (acvFAtan2 returns radians in [-pi, pi].)
      float theta = (float)acvFAtan2((double)dy, (double)dx);
      int bin = (int)lroundf(BINS_OVER_2PI * theta);
      if (bin < 0) bin += N_bins;
      if (bin >= N_bins) bin -= N_bins;

      if (A[L][bin] < R2) A[L][bin] = R2;

      // span needs R (not R^2). One sqrt per pixel here is unavoidable; the
      // alternative (Quake-style inv-sqrt) saves ~10ns/px but adds complexity
      // for negligible payoff at the boundary-pixel count we're processing.
      float R = sqrtf(R2);
      int span = (int)ceilf(BINS_OVER_2PI / R);
      if (span < K_min) span = K_min;
      int halfSpan = span / 2;
      // Diffusion writes are also into R^2 space.
      for (int db = -halfSpan; db <= halfSpan; db++)
      {
        int b = bin + db;
        if (b < 0) b += N_bins; else if (b >= N_bins) b -= N_bins;
        if (B[L][b] < R2) B[L][b] = R2;
      }

      perLabelCartesian[L].push_back(acv_XY((float)x, (float)y));
    }
  }

  // --- Step 4: per-label single-pass anchor-respecting linear interp.
  // For each bin i:
  //   A[i] > 0    -> sig[i] = A[i]                 (anchor)
  //   B[i] == 0   -> sig[i] = 0                    (real gap)
  //   otherwise   -> sig[i] = lerp(A[L], A[R])     (sparseness, interp between
  //                   nearest anchors on each side, stopping at any B==0 bin)
  // Precompute, per label, for each bin i:
  //   leftAnchor[i]  = index of nearest j <= i (cyclic) with A[j] > 0, or -1
  //                    if scanning hit a B==0 before reaching one.
  //   rightAnchor[i] = same but to the right.
  // Two cyclic sweeps. To handle cyclic correctly we go around twice (forward
  // pass for left, backward pass for right) on a doubled index space.
  for (int L = 1; L < NL; L++)
  {
    const std::vector<float> &Av = A[L];
    const std::vector<float> &Bv = B[L];
    std::vector<acv_XY> &sig = perLabelSignature[L];

    std::vector<int> leftA(N_bins, -1), rightA(N_bins, -1);

    // Forward sweep: track last seen anchor; reset at any bin where B==0.
    int last = -1;
    for (int pass = 0; pass < 2; pass++)
    {
      for (int i = 0; i < N_bins; i++)
      {
        if (Bv[i] == 0.0f) { last = -1; }
        else if (Av[i] > 0.0f) { last = i; }
        if (pass == 1) leftA[i] = last;
      }
    }
    // Backward sweep: same logic, the other direction.
    last = -1;
    for (int pass = 0; pass < 2; pass++)
    {
      for (int i = N_bins - 1; i >= 0; i--)
      {
        if (Bv[i] == 0.0f) { last = -1; }
        else if (Av[i] > 0.0f) { last = i; }
        if (pass == 1) rightA[i] = last;
      }
    }

    // Combine.  A and B still hold R^2; sqrt once here per bin.  Interpolation
    // happens in R^2 space (slightly biased vs interpolation in R space, but
    // the difference is below polar-signature matching sensitivity and saves
    // one sqrt-per-anchor-pair).
    for (int i = 0; i < N_bins; i++)
    {
      float bin_center_theta = TWO_PI * (float)i / (float)N_bins;
      if (bin_center_theta > (float)M_PI) bin_center_theta -= TWO_PI;
      float r2 = 0.0f;
      if (Av[i] > 0.0f) {
        r2 = Av[i];
      } else if (Bv[i] == 0.0f) {
        r2 = 0.0f;
      } else {
        int LA = leftA[i], RA = rightA[i];
        if (LA >= 0 && RA >= 0 && LA != RA) {
          int dL = (i - LA + N_bins) % N_bins;
          int dR = (RA - i + N_bins) % N_bins;
          float t = (float)dL / (float)(dL + dR);
          r2 = Av[LA] * (1.0f - t) + Av[RA] * t;
        } else if (LA >= 0) {
          r2 = Av[LA];
        } else if (RA >= 0) {
          r2 = Av[RA];
        } else {
          r2 = Bv[i];
        }
      }
      sig[i] = acv_XY(r2 > 0.0f ? sqrtf(r2) : 0.0f, bin_center_theta);
    }
  }
}

