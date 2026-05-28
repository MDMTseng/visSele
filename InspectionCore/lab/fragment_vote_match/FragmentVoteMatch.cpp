#include "FragmentVoteMatch.h"
#include <math.h>
#include <algorithm>

using std::vector;

static inline float angdiff(float a) { return atan2f(sinf(a), cosf(a)); }

// Resample an ordered polyline to uniform arc-length spacing ds.
static vector<acv_XY> resample(const vector<acv_XY> &pts, float ds, bool closed)
{
  vector<acv_XY> out;
  if (pts.size() < 2) return out;
  out.push_back(pts[0]);
  float acc = 0; acv_XY prev = pts[0];
  size_t n = pts.size();
  size_t lim = closed ? n + 1 : n; // wrap one extra for closed
  for (size_t i = 1; i < lim; i++)
  {
    acv_XY cur = pts[i % n];
    float seg = hypotf(cur.X - prev.X, cur.Y - prev.Y);
    while (seg > 0 && acc + seg >= ds)
    {
      float t = (ds - acc) / seg;
      acv_XY np = { prev.X + t * (cur.X - prev.X), prev.Y + t * (cur.Y - prev.Y) };
      out.push_back(np);
      prev = np;
      seg = hypotf(cur.X - prev.X, cur.Y - prev.Y);
      acc = 0;
    }
    acc += seg; prev = cur;
  }
  return out;
}

// Turning function (tangent angle per sample). closed => central diff wraps.
static vector<float> turning(const vector<acv_XY> &p, bool closed)
{
  int n = (int)p.size();
  vector<float> t(n, 0);
  for (int i = 0; i < n; i++)
  {
    int a, b;
    if (closed) { a = (i - 1 + n) % n; b = (i + 1) % n; }
    else { a = std::max(0, i - 1); b = std::min(n - 1, i + 1); }
    t[i] = atan2f(p[b].Y - p[a].Y, p[b].X - p[a].X);
  }
  return t;
}

static float curvSpan(const vector<float> &tau)
{
  // total absolute turning along the fragment (info content)
  float s = 0;
  for (size_t i = 1; i < tau.size(); i++) s += fabsf(angdiff(tau[i] - tau[i - 1]));
  return s;
}

FVTemplate fv_build_template(const vector<acv_XY> &contour, const FVParams &p)
{
  FVTemplate t;
  t.ds = p.ds;
  t.pts = resample(contour, p.ds, true);
  t.tau = turning(t.pts, true);
  acv_XY c = {0, 0};
  for (auto &q : t.pts) { c.X += q.X; c.Y += q.Y; }
  if (!t.pts.empty()) { c.X /= t.pts.size(); c.Y /= t.pts.size(); }
  t.origin = c;
  return t;
}

struct Vote { float x, y, theta, w; };

// Match one fragment turning fn vs template; emit up to max_cands best offsets.
static void matchFragment(const FVTemplate &tmpl, const vector<acv_XY> &fragR,
                          const vector<float> &tf, const FVParams &p, vector<Vote> &votes)
{
  int M = (int)tmpl.tau.size();
  int L = (int)tf.size();
  if (L < 3 || M < 3) return;

  float span = curvSpan(tf);
  if (span < p.min_curv_span) return; // near-straight => too ambiguous, skip
  float fragW = span * L; // distinctiveness * length

  // residual & rotation for every arc offset
  vector<float> resid(M), rot(M);
  for (int s = 0; s < M; s++)
  {
    float ss = 0, cc = 0;
    for (int k = 0; k < L; k++) { float d = tf[k] - tmpl.tau[(s + k) % M]; ss += sinf(d); cc += cosf(d); }
    float dth = atan2f(ss, cc);
    float e = 0;
    for (int k = 0; k < L; k++) { float r = angdiff(tf[k] - tmpl.tau[(s + k) % M] - dth); e += r * r; }
    resid[s] = e / L; rot[s] = dth;
  }
  // Keep the TOP-K low-residual arc offsets (local minima). A partial arc is
  // genuinely ambiguous (it can fit several template locations), so we emit a
  // hypothesis for each and let cross-fragment VOTING consensus disambiguate:
  // the true pose is reinforced by many fragments, wrong correspondences scatter.
  vector<int> cands;
  for (int s = 0; s < M; s++)
  {
    if (resid[s] >= p.residual_thresh) continue;
    if (resid[s] <= resid[(s - 1 + M) % M] && resid[s] <= resid[(s + 1) % M]) cands.push_back(s);
  }
  std::sort(cands.begin(), cands.end(), [&](int a, int b){ return resid[a] < resid[b]; });
  if ((int)cands.size() > p.max_cands_per_frag) cands.resize(p.max_cands_per_frag);

  for (int s1 : cands)
  {
    // Closed-form 2D Procrustes on matched pairs model tmpl.pts[s1+k] -> scene
    // fragR[k] -> precise (R,t) (the turning-fn offset is arc-bin quantized).
    double mAx=0,mAy=0,mBx=0,mBy=0;
    for (int k=0;k<L;k++){ const acv_XY&A=tmpl.pts[(s1+k)%M]; const acv_XY&B=fragR[k]; mAx+=A.X;mAy+=A.Y;mBx+=B.X;mBy+=B.Y; }
    mAx/=L;mAy/=L;mBx/=L;mBy/=L;
    double Sc=0,Ss=0;
    for (int k=0;k<L;k++){ const acv_XY&A=tmpl.pts[(s1+k)%M]; const acv_XY&B=fragR[k];
      double ax=A.X-mAx,ay=A.Y-mAy,bx=B.X-mBx,by=B.Y-mBy; Sc+=ax*bx+ay*by; Ss+=ax*by-ay*bx; }
    double th = atan2(Ss,Sc);
    double ct=cos(th), st=sin(th);
    double tx = mBx - (ct*mAx - st*mAy), ty = mBy - (st*mAx + ct*mAy);
    Vote vt;
    vt.x = (float)(ct*tmpl.origin.X - st*tmpl.origin.Y + tx);
    vt.y = (float)(st*tmpl.origin.X + ct*tmpl.origin.Y + ty);
    vt.theta = (float)th;
    vt.w = fragW / (resid[s1] + 1e-4f);
#ifdef FV_DEBUG
    printf("  frag L=%d span=%.2f s1=%d r=%.4f -> VOTE (%.1f,%.1f) th=%.1f w=%.0f\n",
           L, span, s1, resid[s1], vt.x, vt.y, vt.theta*180/M_PI, vt.w);
#endif
    votes.push_back(vt);
  }
}

std::vector<FVPose> fv_match(const FVTemplate &tmpl,
                             const std::vector<std::vector<acv_XY>> &fragments,
                             const FVParams &p)
{
  vector<Vote> votes;
  for (auto &frag : fragments)
  {
    vector<acv_XY> fr = resample(frag, p.ds, false);
    if (fr.size() < 3) continue;
    vector<float> tf = turning(fr, false);
    matchFragment(tmpl, fr, tf, p, votes);
  }

  // greedy clustering by descending weight (consensus peaks)
  vector<char> used(votes.size(), 0);
  vector<int> order(votes.size());
  for (size_t i = 0; i < votes.size(); i++) order[i] = (int)i;
  std::sort(order.begin(), order.end(), [&](int a, int b){ return votes[a].w > votes[b].w; });

  std::vector<FVPose> poses;
  for (int oi : order)
  {
    if (used[oi]) continue;
    float sx = 0, sy = 0, ss = 0, cc = 0, sw = 0; int cnt = 0;
    for (size_t j = 0; j < votes.size(); j++)
    {
      if (used[j]) continue;
      if (fabsf(votes[j].x - votes[oi].x) <= p.pos_tol &&
          fabsf(votes[j].y - votes[oi].y) <= p.pos_tol &&
          fabsf(angdiff(votes[j].theta - votes[oi].theta)) <= p.ang_tol)
      {
        used[j] = 1;
        float w = votes[j].w;
        sx += w * votes[j].x; sy += w * votes[j].y;
        ss += w * sinf(votes[j].theta); cc += w * cosf(votes[j].theta);
        sw += w; cnt++;
      }
    }
    if (cnt < p.min_inliers) continue;
    FVPose ps; ps.x = sx / sw; ps.y = sy / sw; ps.theta = atan2f(ss, cc);
    ps.score = sw; ps.inliers = cnt;
    poses.push_back(ps);
  }
  std::sort(poses.begin(), poses.end(), [](const FVPose &a, const FVPose &b){ return a.score > b.score; });
  return poses;
}
