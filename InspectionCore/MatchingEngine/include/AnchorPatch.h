#ifndef ANCHOR_PATCH_H
#define ANCHOR_PATCH_H

// Template-patch validation for locating anchors (定位點). An anchor is a vital
// datum: measurements are taken relative to it, so a wrong-feature lock-on must
// FAIL the part (see docs/measurement_pipeline_and_caveats.md, memory
// project-locating-anchor). We validate the located anchor by zero-normalized
// cross-correlation (ZNCC) of a small taught patch (captured from the golden
// image at teach time, in the pose-normalized template domain) against the
// runtime neighborhood sampled in the SAME template domain.
//
// ZNCC is brightness/contrast invariant (handles exposure/gain drift) and a
// holistic appearance check, so it catches wrong-edge lock-ons that a 1-D
// edge-strength gate can miss. Abnormality is judged by APPEARANCE, not by how
// far the anchor moved (large legitimate shifts come from part deformation +
// low coarse-pose precision, which the anchor warp is meant to absorb).

// Zero-normalized cross-correlation of template `tpl` (wt x ht) against the
// larger search window `win` (ww x wh, ww>=wt, wh>=ht), sliding `tpl` over every
// integer offset. Returns the best score in [-1,1] (1 = identical pattern);
// optionally reports the best integer shift. NaN cells (out-of-image samples)
// invalidate that placement. Returns -2 if inputs are degenerate.
float anchor_patch_zncc(const float *tpl, int wt, int ht,
                        const float *win, int ww, int wh,
                        int *bestDx = 0, int *bestDy = 0);

#endif
