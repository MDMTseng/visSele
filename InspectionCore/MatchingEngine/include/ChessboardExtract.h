#ifndef CHESSBOARD_EXTRACT_H
#define CHESSBOARD_EXTRACT_H

// Chessboard corner detection + auto-grid -> obj/img correspondences for
// lens calibration; this
// header is OpenCV-free so any caller can include it.
//
// Pipeline (ports telecentric_calib.py / the validated /tmp/telecal_run.cpp):
//   goodFeaturesToTrack -> 4-blob ring saddle filter -> cornerSubPix
//   -> auto-grid affine lattice (per-view integer (i,j); offset absorbed by
//      per-view extrinsics in bundle adjustment).

#include "TelecentricCalib.h"   // TelecentricViewData
#include <string>
#include <vector>

// Detect + grid one image. Returns true and fills `out` (obj 3*N mm, img 2*N px).
// Optionally reports the image dimensions.
bool chessboard_detect_and_grid(const std::string &imagePath, double square_mm,
                                TelecentricViewData &out, int *imgW = 0, int *imgH = 0);

// Batch: detect+grid each path; collect usable views. imgW/imgH = last image size.
// Returns the number of usable views.
int chessboard_extract_views(const std::vector<std::string> &imagePaths, double square_mm,
                             std::vector<TelecentricViewData> &outViews, int &imgW, int &imgH);

#endif
