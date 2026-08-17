#include "FieldCalib.h"
#include "cJSON.h"
#include "logctrl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

LOG_MODULE("calib.field");

static cJSON *grid_to_json(const FieldGrid &g)
{
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "rows", g.rows);
  cJSON_AddNumberToObject(o, "cols", g.cols);
  cJSON_AddNumberToObject(o, "n_frames", g.n_frames);
  auto vec2arr = [](const std::vector<double> &v) {
    cJSON *a = cJSON_CreateArray();
    for (double x : v) cJSON_AddItemToArray(a, cJSON_CreateNumber(x));
    return a;
  };
  cJSON_AddItemToObject(o, "mean", vec2arr(g.mean));
  cJSON_AddItemToObject(o, "std",  vec2arr(g.std));
  cJSON *va = cJSON_CreateArray();
  for (uint8_t v : g.valid) cJSON_AddItemToArray(va, cJSON_CreateNumber(v));
  cJSON_AddItemToObject(o, "valid", va);
  return o;
}

static FieldGrid grid_from_json(cJSON *o)
{
  FieldGrid g;
  if (!o) return g;
  cJSON *r = cJSON_GetObjectItem(o, "rows");
  cJSON *c = cJSON_GetObjectItem(o, "cols");
  cJSON *n = cJSON_GetObjectItem(o, "n_frames");
  if (r && cJSON_IsNumber(r)) g.rows = r->valueint;
  if (c && cJSON_IsNumber(c)) g.cols = c->valueint;
  if (n && cJSON_IsNumber(n)) g.n_frames = n->valueint;
  auto arr2vec = [](cJSON *a) {
    std::vector<double> v;
    if (!cJSON_IsArray(a)) return v;
    int n = cJSON_GetArraySize(a);
    v.reserve(n);
    for (int i = 0; i < n; i++) {
      cJSON *it = cJSON_GetArrayItem(a, i);
      v.push_back(cJSON_IsNumber(it) ? it->valuedouble : 0.0);
    }
    return v;
  };
  g.mean = arr2vec(cJSON_GetObjectItem(o, "mean"));
  g.std  = arr2vec(cJSON_GetObjectItem(o, "std"));
  cJSON *va = cJSON_GetObjectItem(o, "valid");
  if (cJSON_IsArray(va)) {
    int n = cJSON_GetArraySize(va);
    g.valid.resize(n);
    for (int i = 0; i < n; i++) {
      cJSON *it = cJSON_GetArrayItem(va, i);
      g.valid[i] = (cJSON_IsNumber(it) && it->valueint != 0) ? 1 : 0;
    }
  } else {
    g.valid.assign(g.mean.size(), 1);
  }
  // rows/cols and the arrays are read independently, so a hand-edited or
  // truncated calib file could claim a 10x10 grid and carry an empty mean[].
  // Downstream only checks rows>0 && cols>0 and then indexes r*cols+c, so that
  // file writes 100 doubles past the end of a zero-length vector. A grid whose
  // dimensions do not match its data is not a usable grid at all.
  if ((size_t)g.rows * (size_t)g.cols != g.mean.size() || g.rows <= 0 || g.cols <= 0)
  {
    if (g.rows != 0 || g.cols != 0 || !g.mean.empty())
      LOGE("field calib grid discarded: %dx%d but mean[] has %d entries",
           g.rows, g.cols, (int)g.mean.size());
    return FieldGrid();
  }
  if (g.valid.size() != g.mean.size()) g.valid.assign(g.mean.size(), 1);
  if (g.std.size()   != g.mean.size()) g.std.assign(g.mean.size(), 0.0);
  return g;
}

void field_calib_update_stats(FieldCalibResult &r)
{
  auto agg = [](const FieldGrid &g, double &mn, double &lo, double &hi) {
    if (g.mean.empty()) { mn=lo=hi=0; return; }
    bool first = true; double s = 0; int n = 0;
    for (size_t i = 0; i < g.mean.size(); i++) {
      if (i < g.valid.size() && g.valid[i] == 0) continue;
      double x = g.mean[i];
      if (first) { lo = hi = x; first = false; }
      else { if (x<lo) lo=x; if (x>hi) hi=x; }
      s += x; n++;
    }
    mn = n > 0 ? s / n : 0;
  };
  agg(r.bright, r.bright_mean, r.bright_min, r.bright_max);
  agg(r.dark,   r.dark_mean,   r.dark_min,   r.dark_max);
  int vc = 0;
  for (uint8_t v : r.bright.valid) if (!v) vc++;
  r.bright_vignette_cells = vc;
  r.uniformity_pct = (r.bright_mean > 0)
    ? 100.0 * (1.0 - (r.bright_max - r.bright_min) / r.bright_mean) : 0.0;
  r.dynamic_range = r.bright_mean - r.dark_mean;
  // Hot-cell heuristic: dark cell whose mean exceeds dark_mean + 4*median(std).
  double medStd = 0;
  if (!r.dark.std.empty()) {
    std::vector<double> s = r.dark.std;
    std::nth_element(s.begin(), s.begin()+s.size()/2, s.end());
    medStd = s[s.size()/2];
  }
  double thr = r.dark_mean + 4.0 * medStd;
  int hot = 0;
  for (double x : r.dark.mean) if (x > thr) hot++;
  r.hot_cells = hot;
}

char *field_calib_to_json(const FieldCalibResult &r_in)
{
  FieldCalibResult r = r_in;
  field_calib_update_stats(r);
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "report_type", "field_calib");
  cJSON_AddBoolToObject(o, "ok", r.ok);
  cJSON *sz = cJSON_CreateArray();
  cJSON_AddItemToArray(sz, cJSON_CreateNumber(r.img_w));
  cJSON_AddItemToArray(sz, cJSON_CreateNumber(r.img_h));
  cJSON_AddItemToObject(o, "image_size", sz);
  cJSON_AddItemToObject(o, "bright", grid_to_json(r.bright));
  cJSON_AddItemToObject(o, "dark",   grid_to_json(r.dark));
  cJSON *st = cJSON_CreateObject();
  cJSON_AddNumberToObject(st, "bright_mean", r.bright_mean);
  cJSON_AddNumberToObject(st, "bright_min",  r.bright_min);
  cJSON_AddNumberToObject(st, "bright_max",  r.bright_max);
  cJSON_AddNumberToObject(st, "dark_mean",   r.dark_mean);
  cJSON_AddNumberToObject(st, "dark_min",    r.dark_min);
  cJSON_AddNumberToObject(st, "dark_max",    r.dark_max);
  cJSON_AddNumberToObject(st, "uniformity_pct", r.uniformity_pct);
  cJSON_AddNumberToObject(st, "dynamic_range",  r.dynamic_range);
  cJSON_AddNumberToObject(st, "hot_cells",      r.hot_cells);
  cJSON_AddNumberToObject(st, "bright_rejected_cells", r.bright_rejected_cells);
  cJSON_AddNumberToObject(st, "bright_vignette_cells", r.bright_vignette_cells);
  cJSON_AddItemToObject(o, "stats", st);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

FieldCalibResult field_calib_from_json(const char *json)
{
  FieldCalibResult r;
  if (!json) return r;
  cJSON *o = cJSON_Parse(json);
  if (!o) return r;
  cJSON *ok = cJSON_GetObjectItem(o, "ok"); r.ok = ok ? cJSON_IsTrue(ok) : true;
  cJSON *sz = cJSON_GetObjectItem(o, "image_size");
  if (cJSON_IsArray(sz) && cJSON_GetArraySize(sz) >= 2) {
    r.img_w = cJSON_GetArrayItem(sz, 0)->valueint;
    r.img_h = cJSON_GetArrayItem(sz, 1)->valueint;
  }
  r.bright = grid_from_json(cJSON_GetObjectItem(o, "bright"));
  r.dark   = grid_from_json(cJSON_GetObjectItem(o, "dark"));
  field_calib_update_stats(r);
  cJSON_Delete(o);
  return r;
}

bool field_calib_save_file(const FieldCalibResult &r, const char *path)
{
  char *s = field_calib_to_json(r);
  if (!s) return false;
  FILE *f = fopen(path, "w");
  bool ok = false;
  // "the file opened" is not "the calibration is saved": ENOSPC surfaces at
  // fputs or (for the last buffered block) at fclose, and reporting success
  // anyway means the operator believes a calibration exists that does not.
  if (f) { ok = (fputs(s, f) >= 0); if (fclose(f) != 0) ok = false; }
  free(s);
  return ok;
}

FieldCalibResult field_calib_load_file(const char *path)
{
  FieldCalibResult r;
  FILE *f = fopen(path, "rb");
  if (!f) return r;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<char> buf(n + 1, 0);
  fread(buf.data(), 1, n, f);
  fclose(f);
  return field_calib_from_json(buf.data());
}

double field_grid_sample(const FieldGrid &g, double u, double v, int img_w, int img_h)
{
  if (g.rows <= 0 || g.cols <= 0 || img_w <= 0 || img_h <= 0) return 0.0;
  if (g.mean.size() != (size_t)g.rows * g.cols) return 0.0;
  double gx = (u / img_w) * g.cols - 0.5;
  double gy = (v / img_h) * g.rows - 0.5;
  int x0 = std::max(0, std::min(g.cols - 1, (int)std::floor(gx)));
  int y0 = std::max(0, std::min(g.rows - 1, (int)std::floor(gy)));
  int x1 = std::min(g.cols - 1, x0 + 1);
  int y1 = std::min(g.rows - 1, y0 + 1);
  double fx = std::max(0.0, std::min(1.0, gx - x0));
  double fy = std::max(0.0, std::min(1.0, gy - y0));
  // Vignette / excluded cells short-circuit: caller must skip equalization here.
  if (!g.valid.empty()) {
    if (!g.valid[y0*g.cols+x0] || !g.valid[y0*g.cols+x1] ||
        !g.valid[y1*g.cols+x0] || !g.valid[y1*g.cols+x1]) return 0.0;
  }
  double v00 = g.mean[y0 * g.cols + x0];
  double v10 = g.mean[y0 * g.cols + x1];
  double v01 = g.mean[y1 * g.cols + x0];
  double v11 = g.mean[y1 * g.cols + x1];
  return (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10
       + (1 - fx) * fy * v01       + fx * fy * v11;
}
