#include "TelecentricCalib.h"
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Small dense linear algebra
// ---------------------------------------------------------------------------

// Solve A x = b (n x n) in place, Gaussian elimination + partial pivoting.
static bool solveLinear(double *A, double *b, int n)
{
  for (int col = 0; col < n; col++)
  {
    int piv = col; double best = fabs(A[col * n + col]);
    for (int r = col + 1; r < n; r++)
    { double v = fabs(A[r * n + col]); if (v > best) { best = v; piv = r; } }
    if (best < 1e-18) return false;
    if (piv != col)
    {
      for (int c = 0; c < n; c++) { double t = A[col*n+c]; A[col*n+c]=A[piv*n+c]; A[piv*n+c]=t; }
      double tb = b[col]; b[col] = b[piv]; b[piv] = tb;
    }
    for (int r = 0; r < n; r++)
    {
      if (r == col) continue;
      double f = A[r*n+col] / A[col*n+col];
      for (int c = col; c < n; c++) A[r*n+c] -= f * A[col*n+c];
      b[r] -= f * b[col];
    }
  }
  for (int i = 0; i < n; i++) b[i] /= A[i*n+i];
  return true;
}

// Least-squares affine fit [X Y 1] -> z, returns coef[3]. Solves 3x3 normal eqs.
static bool affineFit(const double *X, const double *Y, const double *z, int n, double coef[3])
{
  double A[9] = {0}; double b[3] = {0};
  for (int i = 0; i < n; i++)
  {
    double phi[3] = {X[i], Y[i], 1.0};
    for (int a = 0; a < 3; a++)
    {
      b[a] += phi[a] * z[i];
      for (int c = 0; c < 3; c++) A[a*3+c] += phi[a]*phi[c];
    }
  }
  coef[0]=b[0]; coef[1]=b[1]; coef[2]=b[2];
  return solveLinear(A, coef, 3);
}

// ---------------------------------------------------------------------------
// Rodrigues
// ---------------------------------------------------------------------------

static void rodrigues(const double rvec[3], double R[9])
{
  double th = sqrt(rvec[0]*rvec[0] + rvec[1]*rvec[1] + rvec[2]*rvec[2]);
  if (th < 1e-12)
  {
    R[0]=1;R[1]=0;R[2]=0; R[3]=0;R[4]=1;R[5]=0; R[6]=0;R[7]=0;R[8]=1;
    return;
  }
  double kx=rvec[0]/th, ky=rvec[1]/th, kz=rvec[2]/th;
  double c=cos(th), s=sin(th), v=1-c;
  R[0]=c+kx*kx*v;     R[1]=kx*ky*v-kz*s;  R[2]=kx*kz*v+ky*s;
  R[3]=ky*kx*v+kz*s;  R[4]=c+ky*ky*v;     R[5]=ky*kz*v-kx*s;
  R[6]=kz*kx*v-ky*s;  R[7]=kz*ky*v+kx*s;  R[8]=c+kz*kz*v;
}

static void rodrigues_inv(const double R[9], double rvec[3])
{
  double tr = R[0]+R[4]+R[8];
  double cth = (tr - 1.0) * 0.5;
  if (cth > 1) cth = 1; if (cth < -1) cth = -1;
  double th = acos(cth);
  double rx = R[7]-R[5], ry = R[2]-R[6], rz = R[3]-R[1];
  if (th < 1e-9) { rvec[0]=0.5*rx; rvec[1]=0.5*ry; rvec[2]=0.5*rz; return; }
  double f = th / (2.0 * sin(th));
  rvec[0]=f*rx; rvec[1]=f*ry; rvec[2]=f*rz;
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

void telecentric_project(const TelecentricIntrinsics &in, const TelecentricView &vw,
                         const double *obj, int n, double *outUV)
{
  double R[9]; rodrigues(vw.rvec, R);
  for (int i = 0; i < n; i++)
  {
    double X = obj[i*3+0], Y = obj[i*3+1]; // Z=0
    double xn = R[0]*X + R[1]*Y + vw.tx;
    double yn = R[3]*X + R[4]*Y + vw.ty;
    double r2 = xn*xn + yn*yn, r4 = r2*r2, r6 = r4*r2;
    double radial = 1.0 + in.k1*r2 + in.k2*r4 + in.k3*r6;
    double xd = xn*radial + 2.0*in.p1*xn*yn + in.p2*(r2+2.0*xn*xn) + in.s1*r2 + in.s2*r4;
    double yd = yn*radial + in.p1*(r2+2.0*yn*yn) + 2.0*in.p2*xn*yn + in.s3*r2 + in.s4*r4;
    outUV[i*2+0] = in.m*xd + in.u0;
    outUV[i*2+1] = in.m*yd + in.v0;
  }
}

// ---------------------------------------------------------------------------
// Param packing: [12 intr][per view: rvec(3), tx, ty]
// ---------------------------------------------------------------------------

static const int N_INTR = 12;
static const int N_EXTR = 5;

static void unpack(const double *x, TelecentricIntrinsics &in, std::vector<TelecentricView> &vs, int nv)
{
  in.m=x[0]; in.u0=x[1]; in.v0=x[2];
  in.k1=x[3]; in.k2=x[4]; in.k3=x[5];
  in.p1=x[6]; in.p2=x[7];
  in.s1=x[8]; in.s2=x[9]; in.s3=x[10]; in.s4=x[11];
  vs.resize(nv);
  for (int i=0;i<nv;i++)
  {
    const double *e = x + N_INTR + i*N_EXTR;
    vs[i].rvec[0]=e[0]; vs[i].rvec[1]=e[1]; vs[i].rvec[2]=e[2];
    vs[i].tx=e[3]; vs[i].ty=e[4];
  }
}

// residual vector r (size 2*totalPts): projected - observed.
static void residuals(const double *x, const std::vector<TelecentricViewData> &data,
                      std::vector<double> &r)
{
  TelecentricIntrinsics in; std::vector<TelecentricView> vs;
  unpack(x, in, vs, (int)data.size());
  int idx = 0;
  for (size_t v = 0; v < data.size(); v++)
  {
    int n = data[v].n;
    std::vector<double> uv(2*n);
    telecentric_project(in, vs[v], data[v].obj.data(), n, uv.data());
    for (int i = 0; i < 2*n; i++) r[idx++] = uv[i] - data[v].img[i];
  }
}

static double cost(const std::vector<double> &r)
{
  double s = 0; for (double v : r) s += v*v; return s;
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

static double init_magnification(const std::vector<TelecentricViewData> &data)
{
  double mmax = 0;
  for (auto &d : data)
  {
    int n = d.n;
    std::vector<double> X(n), Y(n), U(n), V(n);
    for (int i=0;i<n;i++){ X[i]=d.obj[i*3]; Y[i]=d.obj[i*3+1]; U[i]=d.img[i*2]; V[i]=d.img[i*2+1]; }
    double cu[3], cv[3];
    if (!affineFit(X.data(),Y.data(),U.data(),n,cu)) continue;
    if (!affineFit(X.data(),Y.data(),V.data(),n,cv)) continue;
    double a = cu[0]*cu[0]+cv[0]*cv[0];
    double b = cu[0]*cu[1]+cv[0]*cv[1];
    double c = cu[1]*cu[1]+cv[1]*cv[1];
    double smax = sqrt(0.5*((a+c) + sqrt((a-c)*(a-c) + 4*b*b)));
    if (smax > mmax) mmax = smax;
  }
  return mmax;
}

static void init_pose(const TelecentricViewData &d, double m, double u0, double v0,
                      TelecentricView &out)
{
  int n = d.n;
  std::vector<double> X(n), Y(n), xn(n), yn(n);
  for (int i=0;i<n;i++)
  {
    X[i]=d.obj[i*3]; Y[i]=d.obj[i*3+1];
    xn[i]=(d.img[i*2]   - u0)/m;
    yn[i]=(d.img[i*2+1] - v0)/m;
  }
  double cu[3], cv[3];
  affineFit(X.data(),Y.data(),xn.data(),n,cu);
  affineFit(X.data(),Y.data(),yn.data(),n,cv);
  double r1t[2]={cu[0],cv[0]}, r2t[2]={cu[1],cv[1]};
  out.tx = cu[2]; out.ty = cv[2];
  double z1 = sqrt(fmax(0.0, 1.0 - (r1t[0]*r1t[0]+r1t[1]*r1t[1])));
  double z2 = sqrt(fmax(0.0, 1.0 - (r2t[0]*r2t[0]+r2t[1]*r2t[1])));
  double target = -(r1t[0]*r2t[0] + r1t[1]*r2t[1]);
  double sign2 = 1.0;
  if (z1*z2 > 1e-12) sign2 = (target >= 0) ? 1.0 : -1.0;
  double r1[3]={r1t[0],r1t[1],z1};
  double r2[3]={r2t[0],r2t[1],sign2*z2};
  double n1 = sqrt(r1[0]*r1[0]+r1[1]*r1[1]+r1[2]*r1[2]);
  if (n1>0){r1[0]/=n1;r1[1]/=n1;r1[2]/=n1;}
  double dot = r1[0]*r2[0]+r1[1]*r2[1]+r1[2]*r2[2];
  r2[0]-=dot*r1[0]; r2[1]-=dot*r1[1]; r2[2]-=dot*r1[2];
  double n2 = sqrt(r2[0]*r2[0]+r2[1]*r2[1]+r2[2]*r2[2]);
  if (n2>0){r2[0]/=n2;r2[1]/=n2;r2[2]/=n2;}
  double r3[3]={r1[1]*r2[2]-r1[2]*r2[1], r1[2]*r2[0]-r1[0]*r2[2], r1[0]*r2[1]-r1[1]*r2[0]};
  // R columns = r1,r2,r3
  double R[9]={r1[0],r2[0],r3[0], r1[1],r2[1],r3[1], r1[2],r2[2],r3[2]};
  double det = R[0]*(R[4]*R[8]-R[5]*R[7]) - R[1]*(R[3]*R[8]-R[5]*R[6]) + R[2]*(R[3]*R[7]-R[4]*R[6]);
  if (det < 0) { R[2]=-R[2]; R[5]=-R[5]; R[8]=-R[8]; }
  rodrigues_inv(R, out.rvec);
}

// ---------------------------------------------------------------------------
// Levenberg-Marquardt (numeric Jacobian)
// ---------------------------------------------------------------------------

TelecentricCalibResult telecentric_calibrate(const std::vector<TelecentricViewData> &data,
                                             int imgW, int imgH, int max_iter, double tol)
{
  TelecentricCalibResult res;
  int nv = (int)data.size();
  if (nv < 2) return res;

  int P = N_INTR + N_EXTR*nv;
  int total = 0; for (auto &d : data) total += d.n;
  int Rn = 2*total;

  double m0 = init_magnification(data);
  double u0 = (imgW-1)*0.5, v0 = (imgH-1)*0.5;
  if (!(m0 > 0)) return res;

  std::vector<double> x(P, 0.0);
  x[0]=m0; x[1]=u0; x[2]=v0;
  for (int i=0;i<nv;i++)
  {
    TelecentricView vw; init_pose(data[i], m0, u0, v0, vw);
    double *e = &x[N_INTR + i*N_EXTR];
    e[0]=vw.rvec[0]; e[1]=vw.rvec[1]; e[2]=vw.rvec[2]; e[3]=vw.tx; e[4]=vw.ty;
  }

  std::vector<double> r(Rn), rTrial(Rn);
  residuals(x.data(), data, r);
  double c = cost(r);

  std::vector<double> J((size_t)Rn * P);
  std::vector<double> JtJ((size_t)P * P), Jtr(P), dp(P);
  double lambda = 1e-3;

  for (int iter = 0; iter < max_iter; iter++)
  {
    // numeric Jacobian (forward differences, per-param relative step)
    for (int j = 0; j < P; j++)
    {
      double xj = x[j];
      double h = 1e-6 * (fabs(xj) + 1e-6);
      x[j] = xj + h;
      residuals(x.data(), data, rTrial);
      x[j] = xj;
      double inv = 1.0 / h;
      for (int i = 0; i < Rn; i++) J[(size_t)i*P + j] = (rTrial[i] - r[i]) * inv;
    }
    // JtJ and Jtr
    for (int a = 0; a < P; a++)
    {
      Jtr[a] = 0;
      for (int b = 0; b < P; b++) JtJ[(size_t)a*P+b] = 0;
    }
    for (int i = 0; i < Rn; i++)
    {
      const double *Ji = &J[(size_t)i*P];
      double ri = r[i];
      for (int a = 0; a < P; a++)
      {
        double Jia = Ji[a];
        Jtr[a] += Jia * ri;
        double *row = &JtJ[(size_t)a*P];
        for (int b = a; b < P; b++) row[b] += Jia * Ji[b];
      }
    }
    for (int a = 0; a < P; a++)
      for (int b = 0; b < a; b++) JtJ[(size_t)a*P+b] = JtJ[(size_t)b*P+a];

    // LM inner loop: try damping until cost decreases
    bool improved = false;
    for (int tryC = 0; tryC < 12; tryC++)
    {
      std::vector<double> A = JtJ;
      for (int a = 0; a < P; a++) A[(size_t)a*P+a] += lambda * JtJ[(size_t)a*P+a];
      for (int a = 0; a < P; a++) dp[a] = -Jtr[a];
      if (!solveLinear(A.data(), dp.data(), P)) { lambda *= 10; continue; }

      std::vector<double> xTrial(x);
      for (int a = 0; a < P; a++) xTrial[a] += dp[a];
      residuals(xTrial.data(), data, rTrial);
      double cTrial = cost(rTrial);
      if (cTrial < c)
      {
        x = xTrial; r = rTrial;
        double rel = (c - cTrial) / (c + 1e-30);
        c = cTrial;
        lambda = fmax(lambda * 0.3, 1e-12);
        improved = true;
        if (rel < tol) { iter = max_iter; }
        break;
      }
      else lambda *= 10;
    }
    if (!improved) break;
  }

  unpack(x.data(), res.intr, res.views, nv);
  // per-view rms
  res.per_view_rms_px.resize(nv);
  double sq = 0; int nt = 0;
  for (int v = 0; v < nv; v++)
  {
    int n = data[v].n;
    std::vector<double> uv(2*n);
    telecentric_project(res.intr, res.views[v], data[v].obj.data(), n, uv.data());
    double s = 0;
    for (int i = 0; i < n; i++)
    {
      double du = uv[i*2]-data[v].img[i*2], dv = uv[i*2+1]-data[v].img[i*2+1];
      s += du*du+dv*dv;
    }
    res.per_view_rms_px[v] = sqrt(s / n);
    sq += s; nt += n;
  }
  res.overall_rms_px = sqrt(sq / (nt>0?nt:1));
  res.ok = true;
  return res;
}
