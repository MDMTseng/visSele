#ifndef BINARIZE_CV_H
#define BINARIZE_CV_H

// Vignette / uneven-illumination tolerant binarization for backlit images,
// CALIBRATION-FREE. Estimates the smooth background illumination from the image
// itself (grayscale morphological close fills dark objects with surrounding
// bright background), divides it out (flat-field), then thresholds. A strong
// radial vignette or brightness gradient that makes a global threshold turn dark
// corners into phantom object is removed automatically.
//
// Output matches the pipeline's binary convention: background = 255 (white),
// object = 0 (black).
//
//   closeKernel : structuring-element size (px) at the estimation scale; must be
//                 LARGER than the biggest object to fill it as background.
//   ratio       : edge fraction of local background (0.5 => object where the
//                 flat-fielded pixel is below 50% of background).
//   downscale   : estimate the background at 1/downscale res for speed (the
//                 illumination field is low-frequency, so this is near-free).

#include "acvImage.hpp"

void binarize_bg_flatten_cv(acvImage *src, acvImage *dst,
                            int closeKernel = 81, float ratio = 0.5f, int downscale = 4);

#endif
