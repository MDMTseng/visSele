#ifndef BACKLIGHT_FIELD_CALIB_H
#define BACKLIGHT_FIELD_CALIB_H

// Robustly clean a per-zone bright-field grid (row-major, W x H) so that
// localized surface defects (scratches, dust) don't get baked into the
// background calibration. Fits a smooth quadratic surface, iteratively rejects
// zones whose residual exceeds rejectK * residual-std, and replaces the
// rejected/invalid zones with the fitted smooth value.
//
// Real backlight illumination is low-frequency/smooth; scratches are local and
// high-frequency, so they show up as large residuals against the fit.
//
// Returns the number of zones that were replaced (a high count is a useful
// "surface degraded -- clean/recalibrate" signal for the UI).
int backLightField_robustClean(float *grid, int W, int H, float rejectK = 2.5f, int maxIter = 3);

#endif
