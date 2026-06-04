#ifndef FIELD_CALIB_H
#define FIELD_CALIB_H

#include <vector>
#include <string>
#include <cstdint>

// Bright/Dark field calibration captured at a configurable M×N grid over the
// image. Grids are produced by averaging N>0 frames per side; per-cell std is
// also kept so the consumer can weight the correction. Sampled bilinearly at
// measurement time. (Application in the inspection pipeline is intentionally
// deferred -- this header just defines the data + persistence contract.)
struct FieldGrid
{
  int rows = 0, cols = 0;
  int n_frames = 0;
  std::vector<double> mean;   // rows*cols, row-major
  std::vector<double> std;    // rows*cols, row-major (per-cell stddev across frames)
  // 1=valid, 0=excluded (vignette / saturated / out of dynamic range). Consumer
  // must skip equalization on invalid cells -- patching them with a fitted
  // value would mis-amplify regions the lens physically can't illuminate.
  std::vector<uint8_t> valid;
};

struct FieldCalibResult
{
  bool ok = false;
  int img_w = 0, img_h = 0;     // source frame size (so grid -> px mapping is unambiguous)
  FieldGrid bright;
  FieldGrid dark;

  // Summary stats (also persisted, computed on save).
  double bright_mean = 0, bright_min = 0, bright_max = 0;
  double dark_mean   = 0, dark_min   = 0, dark_max   = 0;
  double uniformity_pct = 0;        // 100*(1 - (max-min)/mean) on bright
  double dynamic_range  = 0;        // bright_mean - dark_mean
  int    hot_cells      = 0;        // dark cells > dark_mean + 4*dark_stddev (rough)
  int    bright_rejected_cells = 0; // zones replaced by robust-clean (scratch/dust)
  int    bright_vignette_cells = 0; // cells excluded as vignette (lens limit)
};

// JSON I/O. saveJsonPath may be null for to_json. Caller free()s returned str.
char *field_calib_to_json(const FieldCalibResult &r);
FieldCalibResult field_calib_from_json(const char *json);
bool field_calib_save_file(const FieldCalibResult &r, const char *path);
FieldCalibResult field_calib_load_file(const char *path);

// Recompute summary stats from grids. Called by save and useful after parse.
void field_calib_update_stats(FieldCalibResult &r);

// Bilinear sample of a grid at pixel (u,v) given image size. Returns 0 if grid
// is empty or img size is invalid. Provided for the future pipeline integration.
double field_grid_sample(const FieldGrid &g, double u, double v, int img_w, int img_h);

#endif
