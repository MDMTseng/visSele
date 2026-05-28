#include "BackLightFieldCalib.h"
#include <math.h>
#include <vector>

// Solve a small (n x n) linear system A x = b in place via Gaussian elimination
// with partial pivoting. Returns false if singular. n is tiny (6) here.
static bool solveLinear(double *A, double *b, int n)
{
  for (int col = 0; col < n; col++)
  {
    // pivot
    int piv = col;
    double best = fabs(A[col * n + col]);
    for (int r = col + 1; r < n; r++)
    {
      double v = fabs(A[r * n + col]);
      if (v > best) { best = v; piv = r; }
    }
    if (best < 1e-12)
      return false;
    if (piv != col)
    {
      for (int c = 0; c < n; c++)
      {
        double t = A[col * n + c]; A[col * n + c] = A[piv * n + c]; A[piv * n + c] = t;
      }
      double tb = b[col]; b[col] = b[piv]; b[piv] = tb;
    }
    // eliminate
    for (int r = 0; r < n; r++)
    {
      if (r == col) continue;
      double f = A[r * n + col] / A[col * n + col];
      for (int c = col; c < n; c++)
        A[r * n + c] -= f * A[col * n + c];
      b[r] -= f * b[col];
    }
  }
  for (int i = 0; i < n; i++)
    b[i] /= A[i * n + i];
  return true;
}

// Quadratic surface basis at normalized coords (x,y) in [0,1]: [1,x,y,x^2,y^2,xy]
static void basis(double x, double y, double *phi)
{
  phi[0] = 1; phi[1] = x; phi[2] = y; phi[3] = x * x; phi[4] = y * y; phi[5] = x * y;
}

int backLightField_robustClean(float *grid, int W, int H, float rejectK, int maxIter)
{
  const int N = W * H;
  const int M = 6; // quadratic terms
  if (grid == NULL || N < M)
    return 0;

  std::vector<unsigned char> active(N, 1);
  // Start by ignoring obviously-invalid zones (NaN / non-positive).
  for (int k = 0; k < N; k++)
    if (!(grid[k] > 0) || grid[k] != grid[k])
      active[k] = 0;

  double coef[M] = {0};
  int rejected = 0;

  for (int iter = 0; iter < maxIter; iter++)
  {
    // Weighted least squares: normal equations (M x M).
    double A[M * M] = {0};
    double b[M] = {0};
    double phi[M];
    int used = 0;
    for (int yy = 0; yy < H; yy++)
      for (int xx = 0; xx < W; xx++)
      {
        int k = yy * W + xx;
        if (!active[k]) continue;
        double nx = (W > 1) ? (double)xx / (W - 1) : 0.0;
        double ny = (H > 1) ? (double)yy / (H - 1) : 0.0;
        basis(nx, ny, phi);
        for (int i = 0; i < M; i++)
        {
          b[i] += phi[i] * grid[k];
          for (int j = 0; j < M; j++)
            A[i * M + j] += phi[i] * phi[j];
        }
        used++;
      }
    if (used < M)
      break; // not enough clean zones to fit

    double sol[M];
    for (int i = 0; i < M; i++) sol[i] = b[i];
    if (!solveLinear(A, sol, M))
      break;
    for (int i = 0; i < M; i++) coef[i] = sol[i];

    // residual std over active zones
    double sumsq = 0; int cnt = 0;
    for (int yy = 0; yy < H; yy++)
      for (int xx = 0; xx < W; xx++)
      {
        int k = yy * W + xx;
        if (!active[k]) continue;
        double nx = (W > 1) ? (double)xx / (W - 1) : 0.0;
        double ny = (H > 1) ? (double)yy / (H - 1) : 0.0;
        basis(nx, ny, phi);
        double fit = 0; for (int i = 0; i < M; i++) fit += coef[i] * phi[i];
        double r = grid[k] - fit;
        sumsq += r * r; cnt++;
      }
    double std = (cnt > 0) ? sqrt(sumsq / cnt) : 0;
    if (std < 1e-6)
      break; // perfectly smooth; nothing to reject

    // reject zones with large residual (scratch / dust contaminated)
    int newlyRejected = 0;
    for (int yy = 0; yy < H; yy++)
      for (int xx = 0; xx < W; xx++)
      {
        int k = yy * W + xx;
        if (!active[k]) continue;
        double nx = (W > 1) ? (double)xx / (W - 1) : 0.0;
        double ny = (H > 1) ? (double)yy / (H - 1) : 0.0;
        basis(nx, ny, phi);
        double fit = 0; for (int i = 0; i < M; i++) fit += coef[i] * phi[i];
        if (fabs(grid[k] - fit) > rejectK * std)
        {
          active[k] = 0;
          newlyRejected++;
        }
      }
    if (newlyRejected == 0)
      break;
  }

  // Replace every rejected/invalid zone with the fitted smooth value.
  double phi[M];
  for (int yy = 0; yy < H; yy++)
    for (int xx = 0; xx < W; xx++)
    {
      int k = yy * W + xx;
      if (active[k]) continue;
      double nx = (W > 1) ? (double)xx / (W - 1) : 0.0;
      double ny = (H > 1) ? (double)yy / (H - 1) : 0.0;
      basis(nx, ny, phi);
      double fit = 0; for (int i = 0; i < M; i++) fit += coef[i] * phi[i];
      grid[k] = (float)fit;
      rejected++;
    }
  return rejected;
}
