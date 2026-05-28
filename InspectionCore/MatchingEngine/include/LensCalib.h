#ifndef LENS_CALIB_H
#define LENS_CALIB_H

// User-selectable lens calibration: NORMAL (perspective/pinhole, OpenCV
// calibrateCamera) vs TELECENTRIC (orthographic, our dependency-free solver).
// Both consume the same chessboard correspondences (TelecentricViewData) from
// ChessboardExtract. OpenCV-backed (FEATURE_OPENCV); header is OpenCV-free.

#include "TelecentricCalib.h"
#include <string>
#include <vector>

enum class LensModel { PERSPECTIVE, TELECENTRIC };

LensModel lens_model_from_string(const std::string &s); // "perspective"/"telecentric"
const char *lens_model_to_string(LensModel m);

struct LensCalibResult
{
  LensModel model = LensModel::TELECENTRIC;
  bool ok = false;
  double overall_rms_px = 0;

  // PERSPECTIVE (pinhole): camera matrix + distortion k1,k2,p1,p2,k3.
  double fx = 0, fy = 0, cx = 0, cy = 0;
  double dist[5] = {0, 0, 0, 0, 0};

  // TELECENTRIC: full orthographic intrinsics (m = px/mm, etc.).
  TelecentricIntrinsics tele;
};

// Perspective calibration via OpenCV calibrateCamera.
LensCalibResult perspective_calibrate(const std::vector<TelecentricViewData> &views,
                                      int imgW, int imgH, bool fixK3 = true);

// Dispatch on the chosen model.
LensCalibResult lens_calibrate(LensModel model, const std::vector<TelecentricViewData> &views,
                               int imgW, int imgH);

// Point-space distortion correction: map observed (distorted) pixel coords to
// their ideal distortion-free pixel position. Applied to the sparse extracted
// feature points at measurement time (NOT a full-image remap), per the design.
// uv/outUV are row-major 2*n.
void lens_undistort_points(const LensCalibResult &r, const double *uv, int n, double *outUV);

// JSON serialisation of a calibration result (caller owns the returned string;
// free with free()). Schema includes "lens_model", params, rms.
char *lens_calib_to_json(const LensCalibResult &r);

// Parse a calibration result from JSON (as produced by lens_calib_to_json).
LensCalibResult lens_calib_from_json(const char *json);

// End-to-end runner: detect+grid the images, calibrate with `model`, and
// optionally write the result JSON to saveJsonPath. Returns the result.
LensCalibResult lens_calib_run_from_images(const std::vector<std::string> &imagePaths,
                                           double square_mm, LensModel model,
                                           const char *saveJsonPath = 0);

#endif
