// CLI: lens calibration from chessboard images.
// Usage: calib_chessboard <square_mm> <out.json> <model:telecentric|perspective> <img1> [img2 ...]
#include "LensCalib.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
  if (argc < 5)
  {
    fprintf(stderr,
            "Usage: %s <square_mm> <out.json> <telecentric|perspective> <img1> [img2 ...]\n",
            argv[0]);
    return 2;
  }
  double square_mm = atof(argv[1]);
  const char *outPath = argv[2];
  LensModel model = lens_model_from_string(argv[3]);
  std::vector<std::string> imgs;
  for (int i = 4; i < argc; i++) imgs.emplace_back(argv[i]);

  fprintf(stderr, "[calib] square=%.4fmm model=%s images=%zu out=%s\n",
          square_mm, lens_model_to_string(model), imgs.size(), outPath);

  LensCalibResult r = lens_calib_run_from_images(imgs, square_mm, model, outPath);
  if (!r.ok)
  {
    fprintf(stderr, "[calib] FAILED (ok=false)\n");
    return 1;
  }
  fprintf(stderr, "[calib] OK overall_rms_px=%.6g\n", r.overall_rms_px);
  if (model == LensModel::TELECENTRIC)
  {
    fprintf(stderr, "[calib] m=%.6f px/mm  (=%.4f um/px)  u0=%.3f v0=%.3f\n",
            r.tele.m, (r.tele.m > 0) ? 1000.0 / r.tele.m : 0.0, r.tele.u0, r.tele.v0);
  }
  return 0;
}
