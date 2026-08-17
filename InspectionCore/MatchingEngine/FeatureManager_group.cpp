#include "FeatureManager.h"
#include "logctrl.h"
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include "FeatureManager_sig360_circle_line.h"
#include "FeatureManager_group.h"
#include "BackLightFieldCalib.h"
#include "LabelingCV.h"
#include "BinarizeCV.h"
#include "CvBridge.h"
#include "auto_release.hpp"
#include <opencv2/imgproc.hpp>

LOG_MODULE("match.group");

// Per-frame center-sampled adaptive threshold (the default "center_auto" binarize).
// The sig360 part sits near the image center, so the central 1/3 ROI captures the
// part's lit surface. We isolate the BRIGHT GROUP of that ROI via an Otsu split --
// excluding the low-brightness pixels (background/shadow that may share the crop)
// so a small bright part isn't drowned out -- and take the bright group's mean as
// "the brightest part" B. Then T = ratio * B (ratio 0.7), which scales with
// exposure so the sig360 signature survives brightness changes.
static double centerAutoThreshold(const cv::Mat &gray, double ratio, double /*dark*/, double fixedThres)
{
  if (gray.empty() || gray.type() != CV_8UC1) return fixedThres;
  int W = gray.cols, H = gray.rows;
  int rw = std::max(8, W / 3), rh = std::max(8, H / 3);
  cv::Rect roi((W - rw) / 2, (H - rh) / 2, rw, rh);
  roi &= cv::Rect(0, 0, W, H);
  if (roi.width < 4 || roi.height < 4) return fixedThres;
  cv::Mat c = gray(roi);
  // Strided histogram, auto-tuned to ~TARGET_SAMPLES points (bounds cost on big
  // frames, no under-sampling on small ones). Split the needed area-per-sample
  // into an INTEGER y stride (the larger -- skipping whole rows is cache-cheaper)
  // and a FRACTIONAL x stride (the smaller, interlaced within a row), kept close,
  // so the count lands near the target instead of overshooting like an integer
  // stride would. Otsu split + bright-group mean are computed from the histogram.
  const long TARGET_SAMPLES = 10000;
  long roiPix = (long)c.rows * c.cols;
  int ystep = 1; double xstep = 1.0;
  if (roiPix > TARGET_SAMPLES) {
    double area = (double)roiPix / (double)TARGET_SAMPLES;   // pixels per sample
    double D = std::sqrt(area);
    ystep = std::max(1, (int)std::ceil(D));                  // larger stride (integer, rows)
    xstep = area / (double)ystep;                            // smaller stride (fractional, cols)
    if (xstep < 1.0) xstep = 1.0;
  }
  long h[256] = {0}, total = 0; double sum = 0;
  for (int y = 0; y < c.rows; y += ystep) {
    const uchar *p = c.ptr<uchar>(y);
    for (double fx = 0.0; fx < (double)c.cols; fx += xstep) h[p[(int)fx]]++;
  }
  for (int i = 0; i < 256; i++) { total += h[i]; sum += (double)i * h[i]; }
  if (total <= 0) return fixedThres;
  // Otsu threshold (maximize between-class variance) from the histogram.
  double sumB = 0; long wB = 0, otsu = 0; double maxVar = -1;
  for (int t = 0; t < 256; t++) {
    wB += h[t]; if (wB == 0) continue;
    long wF = total - wB; if (wF == 0) break;
    sumB += (double)t * h[t];
    double mB = sumB / wB, mF = (sum - sumB) / wF;
    double var = (double)wB * wF * (mB - mF) * (mB - mF);
    if (var > maxVar) { maxVar = var; otsu = t; }
  }
  // Bright group = bins above Otsu; B = its mean (low-brightness excluded).
  double bsum = 0; long bn = 0;
  for (int i = (int)otsu + 1; i < 256; i++) { bsum += (double)i * h[i]; bn += h[i]; }
  double B = (bn > 0) ? bsum / bn : 0;
  if (B <= 0) return fixedThres;
  double T = ratio * B;
  if (T < 1) T = 1; else if (T > 254) T = 254;
  return T;
}
/*
  FeatureManager_group_proto Section
*/
int FeatureManager_group_proto::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  clearFeatureGroup();
  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }
  return 0;
}

int FeatureManager_group_proto::parse_jobj()
{
  const char *type_str= (char *)JFetch(root,"type",cJSON_String);
  if(type_str==NULL)
  {
    LOGE("ptr: type:<%p> ",type_str);
    return -1;
  }
  LOGI("type:<%s> ",type_str);


  briThres=JFetch_NUMBER_ex(root,"briThres",80);

  // binarize method:  (default) "center_auto" | "bg_flatten" | "fixed"
  //   0 center_auto : DEFAULT. T = ratio * (brightest of the central 1/3), ratio
  //                   0.7. Scales with exposure so the signature survives 0.5x-2x
  //                   brightness; matches the legacy fixed result at nominal.
  //   1 bg_flatten  : calibration-free vignette-tolerant local flat-field
  //   2 fixed       : legacy fixed global briThres (per-def opt-out)
  // edgeRatio is the center_auto ratio; field-cal (adaptiveThres) overrides it
  // below when that path is enabled.
  binarize_method = 0;
  edgeRatio = 0.7;   // center_auto: T = ratio * (brightest of central 1/3)
  darkLevel = 0;
  {
    char *bm = (char *)JFetch(root, "binarize", cJSON_String);
    if (bm != NULL) {
      if      (strcmp(bm, "bg_flatten") == 0) binarize_method = 1;
      else if (strcmp(bm, "fixed")      == 0) binarize_method = 2;
    }
    edgeRatio = JFetch_NUMBER_ex(root, "center_thres_ratio", edgeRatio);
    bg_close_kernel = (int)JFetch_NUMBER_ex(root, "bg_close_kernel", 81);
    bg_ratio = JFetch_NUMBER_ex(root, "bg_ratio", 0.5);
    bg_downscale = (int)JFetch_NUMBER_ex(root, "bg_downscale", 4);
    if (bg_downscale < 1)  bg_downscale = 1;    // a bad def must not resize to 0
    if (bg_downscale > 16) bg_downscale = 16;
  }

  // Optional per-region adaptive threshold (background-evenness soft calib).
  // Schema: "adaptiveThres":{ "enable":true, "ratio":0.5, "gridW":W,"gridH":H,
  //                           "bright":[...W*H...], "dark":[...W*H...](optional) }
  useAdaptiveThres = false;
  useCalibBackground = false;
  bgThreshMap.clear();
  {
    cJSON *adaptive = cJSON_GetObjectItem(root, "adaptiveThres");
    int enable = (adaptive) ? (cJSON_IsTrue(cJSON_GetObjectItem(adaptive, "enable")) ? 1 : 0) : 0;
    const char *source = (adaptive) ? (const char *)JFetch(adaptive, "source", cJSON_String) : NULL;
    if (adaptive && enable && source && strcmp(source, "calib") == 0)
    {
      // Per-camera background: build the threshold map from the loaded
      // stage-light model (sampler) at match time. T = D + ratio*(B - D).
      edgeRatio = JFetch_NUMBER_ex(adaptive, "ratio", 0.5);
      darkLevel = JFetch_NUMBER_ex(adaptive, "dark", 0);
      useCalibBackground = true;
      useAdaptiveThres = true;
      LOGI("adaptiveThres source=calib ratio=%.2f dark=%.1f", edgeRatio, darkLevel);
    }
    else if (adaptive && enable)
    {
      edgeRatio = JFetch_NUMBER_ex(adaptive, "ratio", 0.5);
      bgMapW = (int)JFetch_NUMBER_ex(adaptive, "gridW", 0);
      bgMapH = (int)JFetch_NUMBER_ex(adaptive, "gridH", 0);
      cJSON *bright = cJSON_GetObjectItem(adaptive, "bright");
      cJSON *dark = cJSON_GetObjectItem(adaptive, "dark");
      int n = bgMapW * bgMapH;
      if (cJSON_IsArray(bright) && cJSON_GetArraySize(bright) == n && n > 0)
      {
        bgThreshMap.resize(n);
        for (int k = 0; k < n; k++)
        {
          float B = (float)cJSON_GetArrayItem(bright, k)->valuedouble;
          float D = 0.0f;
          if (cJSON_IsArray(dark) && cJSON_GetArraySize(dark) == n)
            D = (float)cJSON_GetArrayItem(dark, k)->valuedouble;
          bgThreshMap[k] = D + edgeRatio * (B - D);
        }
        useAdaptiveThres = true;
        LOGI("adaptiveThres ON %dx%d ratio=%.2f", bgMapW, bgMapH, edgeRatio);
      }
      else
      {
        LOGE("adaptiveThres enabled but bright grid size != gridW*gridH (%d)", n);
      }
    }
  }

  cJSON *featureSetList = cJSON_GetObjectItem(root,"featureSet");

  if(featureSetList==NULL)
  {
    LOGE("featureSetList array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureSetList))
  {
    LOGE("featureSetList is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureSetList) ; i++)
  {
     cJSON * featureSet = cJSON_GetArrayItem(featureSetList, i);
     int ret = addSubFeature(featureSet);
     if(ret!=0)
     {
       LOGE("Add feature[%d] failed...",i);
       return -1;
     }
  }
  return 0;
}


/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_binary_processing_group::FeatureManager_binary_processing_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  sub_reports.resize(0);
  report.data.binary_processing_group.reports = &sub_reports;
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
  
  ClearReport();
}

int FeatureManager_binary_processing_group::clearFeatureGroup()
{
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      delete binaryFeatureBundle[i];
    }
    binaryFeatureBundle.resize(0);
    return 0;
}

int FeatureManager_binary_processing_group::addSubFeature(cJSON * subFeature)
{
  
  char *str=JFetch_STRING(subFeature,"type");
  if(str==NULL)
  {
    return -1;
  }
  FeatureManager_binary_processing *newFeature=NULL;
  if(strcmp(FeatureManager_sig360_circle_line::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_sig360_circle_line is the type...");
    // The ctor parses this string and keeps its own tree, so the serialisation
    // is ours to free. Passing cJSON_Print() straight in leaked one copy of
    // the sub-feature's def per sub-feature, on every def load -- measured as
    // a steady 50-125KB per INST_CHECK before this.
    MallocHold _s(cJSON_Print(subFeature));
    newFeature = new FeatureManager_sig360_circle_line(_s.str());
  }
  else if(strcmp(FeatureManager_sig360_extractor::GetFeatureTypeName(),str) == 0)
  {
    LOGI("FeatureManager_sig360_extractor is the type...");
    MallocHold _s(cJSON_Print(subFeature));
    newFeature = new FeatureManager_sig360_extractor(_s.str());
  }
  // else if(strcmp(FM_camera_calibration::GetFeatureTypeName(),str) == 0)
  // {

  //   LOGI("FeatureManager_camera_calibration is the type...");
  //   newFeature = new FM_camera_calibration(cJSON_Print(subFeature));
  // }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  binaryFeatureBundle.push_back(newFeature);

  sub_reports.resize(binaryFeatureBundle.size());
  report.data.binary_processing_group.reports = &sub_reports;
  report.type = FeatureReport::binary_processing_group;
  return 0;
}

int FeatureManager_binary_processing_group::FeatureMatching(cv::Mat &img_cv)
{
  if (img_cv.empty()) return -1;
  if (!img_cv.isContinuous()) img_cv = img_cv.clone();

  // Fast path: when no sub-feature needs the binary silhouette (e.g. a
  // shape-based locator works on the raw grayscale), skip binarize -> cage ->
  // CCL -> intrusion entirely. Old sig360 defs still take the full path below.
  {
    bool anyNeedsBinary = false;
    for (size_t i = 0; i < binaryFeatureBundle.size(); i++)
      if (binaryFeatureBundle[i]->needsBinaryPreprocessing()) { anyNeedsBinary = true; break; }
    if (!binaryFeatureBundle.empty() && !anyNeedsBinary)
    {
      if (getenv("SHAPE_DBG"))
        fprintf(stderr, "[SHAPE_DBG] group: raw-gray fast path (binarize/cage/CCL/contour skipped)\n");
      report.bacpac = bacpac;
      error = FeatureReport_ERROR::NONE;
      ldData.resize(0);
      const int dsampLevel = (inspection_downsample > 0) ? inspection_downsample : 1;
      for (size_t i = 0; i < binaryFeatureBundle.size(); i++)
      {
        binaryFeatureBundle[i]->setOriginalImage(img_cv);
        binaryFeatureBundle[i]->setLabeledData(&ldData);
        binaryFeatureBundle[i]->setBacPac(bacpac);
        binaryFeatureBundle[i]->setLabelDownSampLevel(dsampLevel);
        binaryFeatureBundle[i]->FeatureMatching(img_cv);   // raw-gray locator ignores arg
      }
      return 0;
    }
  }

  auto _pt0 = std::chrono::steady_clock::now();   // [PROF] start

  report.bacpac=bacpac;
    error=FeatureReport_ERROR::NONE;
    ldData.resize(0);

    // Pre-binarization downsample. When > 1, threshold + CCL + signature build
    // run at the reduced resolution; sub-features get setLabelDownSampLevel(N)
    // so they scale coords back to original-image space for measurement.
    const int dsampLevel = (inspection_downsample > 0) ? inspection_downsample : 1;

    // Source image typically arrives as CV_8UC3 BGR-replicated grayscale (from
    // cv::imread(..., IMREAD_COLOR)). Pull a single channel + optional resize
    // up-front so all three threshold variants below do single-channel work.
    cv::Mat gray_in;
    {
      cv::Mat gray_full;
      if (img_cv.channels() == 1) gray_full = img_cv;
      else cv::extractChannel(img_cv, gray_full, 0);
      if (dsampLevel > 1)
        cv::resize(gray_full, gray_in,
                   cv::Size(gray_full.cols / dsampLevel, gray_full.rows / dsampLevel),
                   0, 0, cv::INTER_AREA);
      else
        gray_in = gray_full;
    }
    auto _pt1 = std::chrono::steady_clock::now();   // [PROF] gray extract + downsample done
    // Binary image is single-channel at the downsampled resolution.
    binary_img_storage.create(gray_in.rows, gray_in.cols, CV_8UC1);

    // Per-camera adaptive threshold from bacpac->fieldCal (bright grid is
    // already vignette-masked + robust-cleaned at save time, so we don't
    // re-clean here). T = D + ratio*(B - D) per valid cell; invalid
    // (vignette) cells fall back to the global briThres. They used to be NAN,
    // which was never handled: a NAN cell makes the bilinear T NAN, every
    // "pixel > T" false, and the whole neighbourhood comes out as foreground.
    if (useCalibBackground && bgThreshMap.empty() &&
        bacpac && bacpac->fieldCal && bacpac->fieldCal->ok)
    {
      const FieldGrid &Bg = bacpac->fieldCal->bright;
      if (Bg.rows > 0 && Bg.cols > 0 && (int)Bg.mean.size() == Bg.rows * Bg.cols)
      {
        int W = Bg.cols, H = Bg.rows;
        bgThreshMap.resize(W * H);
        for (int k = 0; k < W * H; k++) {
          bool valid = (k < (int)Bg.valid.size()) ? (Bg.valid[k] != 0) : true;
          float T = valid ? (float)(darkLevel + edgeRatio * (Bg.mean[k] - darkLevel))
                          : (float)briThres;
          if (!std::isfinite(T)) T = (float)briThres;
          bgThreshMap[k] = T;
        }
        bgMapW = W; bgMapH = H;
        LOGI("adaptiveThres fieldCal map %dx%d ratio=%.2f dark=%.1f vignette=%d",
             W, H, edgeRatio, darkLevel, bacpac->fieldCal->bright_vignette_cells);
      }
    }

    if (binarize_method == 1) // calibration-free vignette-tolerant bg-flatten
    {
      binarize_bg_flatten_cv(gray_in, binary_img_storage, bg_close_kernel, bg_ratio, bg_downscale);
    }
    else if (useAdaptiveThres && !bgThreshMap.empty())
    {
      cvThresholdMap(binary_img_storage, gray_in, bgThreshMap.data(), bgMapW, bgMapH, 0);
    }
    else if (binarize_method == 2) // per-def opt-out: legacy fixed global threshold
    {
      cv::threshold(gray_in, binary_img_storage, (double)briThres, 255.0, cv::THRESH_BINARY);
    }
    else // DEFAULT: per-frame center-sampled adaptive threshold (0.7 * brightest center)
    {
      double T = centerAutoThreshold(gray_in, edgeRatio, darkLevel, (double)briThres);
      cv::threshold(gray_in, binary_img_storage, T, 255.0, cv::THRESH_BINARY);
    }

    int downScaleF = dsampLevel;

    // ---- station mask: stop speckle outside the station becoming labels -----
    //
    // The station used to be applied to the CCL OUTPUT: label the whole frame,
    // then drop what fell outside. One measured run produced 1,901 labels to
    // keep 2 -- the other 1,899 were backlight speckle that cost a full stats
    // record each. Blank them in the binary instead, before CCL runs.
    //
    // MASK, not crop, and the reason is specific: sig360's phase 2 runs a
    // SECOND CCL over bacpac->binary_uc1_for_phase2 and looks its per-label
    // signatures up BY LABEL INDEX against this ldData. Handing one of those a
    // cropped image and the other a full one renumbers the labels and silently
    // attaches signatures to the wrong objects. Masking keeps both passes on
    // the same full-size image, so every coordinate downstream is untouched --
    // no offset to thread through the measurement chain, nothing to get wrong.
    //
    // Cropping the SOURCE would also have moved every adaptive threshold:
    // cvThresholdMap stretches the fieldCal grid across whatever image it is
    // given, so a cropped gray_in silently rescales the whole map. Masking
    // after binarization leaves that pass bit-identical.
    //
    // INSP_LABEL_NOCROP=1 restores whole-frame labeling for A/B.
    static const bool labelNoMask = [] {
      const char *e = getenv("INSP_LABEL_NOCROP");
      const bool v = e && atoi(e) != 0;
      if (v) LOGW("INSP_LABEL_NOCROP=1 -- labeling the whole frame; the station "
                  "is applied to labels afterwards");
      return v;
    }();

    // The cage rectangle. Whole image by default -- which reproduces the old
    // geometry exactly -- or the station when masking is on.
    cv::Rect cageR(0, 0, binary_img_storage.cols, binary_img_storage.rows);
    if (bacpac && bacpac->hasInspRegion() && !labelNoMask)
    {
      acv_XY sOff = bacpac->sampler ? bacpac->sampler->getOriginOffset() : acv_XY{0.f, 0.f};
      // Region is full-sensor px; the binary is input px / dsampLevel.
      cv::Rect want((int)std::floor((bacpac->insp_region_x - sOff.x) / downScaleF),
                    (int)std::floor((bacpac->insp_region_y - sOff.y) / downScaleF),
                    (int)std::ceil (bacpac->insp_region_w / downScaleF),
                    (int)std::ceil (bacpac->insp_region_h / downScaleF));
      cv::Rect r = want & cv::Rect(0, 0, binary_img_storage.cols, binary_img_storage.rows);
      // Needs room for the cage + inner fence, else the fence maths goes
      // negative and the "cage" swallows the station.
      const int need = 2 * (15 / downScaleF) + 8;
      if (r.width > need && r.height > need &&
          (r.width < binary_img_storage.cols || r.height < binary_img_storage.rows))
      {
        // Blank everything outside the station to background (255). Four
        // rectangles rather than a mask Mat: no allocation, no full-frame copy.
        if (r.y > 0)
          binary_img_storage(cv::Rect(0, 0, binary_img_storage.cols, r.y)).setTo(255);
        if (r.br().y < binary_img_storage.rows)
          binary_img_storage(cv::Rect(0, r.br().y, binary_img_storage.cols,
                                      binary_img_storage.rows - r.br().y)).setTo(255);
        if (r.x > 0)
          binary_img_storage(cv::Rect(0, r.y, r.x, r.height)).setTo(255);
        if (r.br().x < binary_img_storage.cols)
          binary_img_storage(cv::Rect(r.br().x, r.y,
                                      binary_img_storage.cols - r.br().x, r.height)).setTo(255);
        cageR = r;
        LOGI_EVERY_N(100, "station mask: labeling %dx%d at (%d,%d) of %dx%d (binary px)",
                     r.width, r.height, r.x, r.y,
                     binary_img_storage.cols, binary_img_storage.rows);
      }
    }

    // Draw the cage on the BINARY image (CV_8UC1, bg=255/fg=0). Cage pixels
    // are foreground (0) so CCL picks them up as one big component. It rides
    // cageR, so with masking on it lands on the station boundary and label 1
    // becomes "material crossing the station", which is what intrusion means
    // once the station is the thing being inspected.
    cv::rectangle(binary_img_storage, cv::Point(cageR.x + 1, cageR.y + 1),
                  cv::Point(cageR.x + cageR.width - 2, cageR.y + cageR.height - 2),
                  cv::Scalar(0), 1);

    int FENCE_AREA = (cageR.width+cageR.height)*2-4;//External frame
    {
      int xDist=15/downScaleF;
      cv::rectangle(binary_img_storage, cv::Point(cageR.x + xDist, cageR.y + xDist),
                    cv::Point(cageR.x + cageR.width - xDist, cageR.y + cageR.height - xDist),
                    cv::Scalar(0), 1);
      FENCE_AREA+=(cageR.width-xDist+cageR.height-xDist)*2-4;
      // 1 px black at y=xDist+3, x=1..xDist-1 (inclusive), relative to the cage.
      cv::line(binary_img_storage, cv::Point(cageR.x + 1, cageR.y + xDist + 3),
               cv::Point(cageR.x + xDist - 1, cageR.y + xDist + 3),
               cv::Scalar(0), 1);
      FENCE_AREA+=xDist;
    }
    //The labeling starts from (1 1) => (W-2,H-2), ie. it will not touch the outmost pixel to simplify the boundary condition
    //You need to draw a black/white cage to work(not crash).
    //The advantage of black cage is you can know which area touches the boundary then we can exclude it
    // CCL once -> packed label image + acv_LabeledData (from stats), no rescan.
    // Input: CV_8UC1 binary; output: CV_8UC3 BGR-packed labels.
    auto _pt2 = std::chrono::steady_clock::now();   // [PROF] threshold/binarize + cage done
    cv::Mat &lableImg_cv = labeled_img_storage;
    acvComponentLabeling_cv(binary_img_storage, lableImg_cv, ldData);
    auto _pt3 = std::chrono::steady_clock::now();   // [PROF] CCL (component labeling) done

    // Label 1 is everything the cage touches: the border cage itself plus
    // anything connected to it. Subtracting FENCE_AREA leaves the part of it
    // that is real intruding material.
    //
    // The intrusionSizeLimitRatio gate that used to sit here -- refuse to
    // inspect the WHOLE frame when this exceeded ratio * image area -- is gone
    // (2026-08-07). It was one global rule with one number for the entire def,
    // it could only ever say "somewhere in this image", and with the default it
    // never got (absent -> 0 -> every frame rejected) it was a trap. obj_detect
    // clean-space regions do the same job per region, with an operator-legible
    // limit in mm^2, and with a per-region choice of whether a trip means "this
    // part is bad" or "this measurement is untrustworthy".
    //
    // Nothing was labeled at all (not even the cage): bail out BEFORE touching
    // ldData[1]. The size gate used to sit below this read.
    if(ldData.size()<=1)
    {
      error=FeatureReport_ERROR::GENERIC;
      for(int i=0;i<binaryFeatureBundle.size();i++)
      {
        binaryFeatureBundle[i]->ClearReport();
      }
      return 0;
    }

    int intrusionObjectArea = ldData[1].area - FENCE_AREA;

    // if(downScaleF!=1)
    // {
    //   for(int i=2;i<ldData.size();i++)
    //   {
    //     ldData[i].area*=downScaleF*downScaleF;
    //     ldData[i].Center=acvVecMult(ldData[i].Center,downScaleF);
    //     ldData[i].LTBound=acvVecMult(ldData[i].LTBound,downScaleF);
    //     ldData[i].RBBound=acvVecMult(ldData[i].RBBound,downScaleF);
    //   }
    //   labeledUpScale(&binary_img,lableImg,downScaleF);
    //   downScaleF=1;
    // }




    ldData[1].area =intrusionObjectArea;

    //Delete the object that has less than certain amount of area on ldData
    //acvRemoveRegionLessThan(img, &ldData, 120);


    //acvCloneImage( img,buff, -1);

    // return 0;
    // LOGI("_________  %f %f ",param.ppb2b,param.mmpb2b);
    
    // Phase 2 path: expose the CV_8UC1 binary so matching_version=2 sub-features
    // can build signatures directly via morph boundary, bypassing the legacy
    // BGR-walker. Reset after the loop so the pointer doesn't outlive this call.
    cv::Mat *prev_bin = bacpac ? bacpac->binary_uc1_for_phase2 : nullptr;
    if (bacpac) bacpac->binary_uc1_for_phase2 = &binary_img_storage;

    LOGI(">>>> ");
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      binaryFeatureBundle[i]->setOriginalImage(img_cv);
      binaryFeatureBundle[i]->setLabeledData(&ldData);
      binaryFeatureBundle[i]->setBacPac(bacpac);
      binaryFeatureBundle[i]->setLabelDownSampLevel(downScaleF);
      // Sub-feature still consumes acvImage*; the base FeatureManager mutual
      // bridge converts cv::Mat -> acvImage shim transparently.
      binaryFeatureBundle[i]->FeatureMatching(lableImg_cv);
    }
    if (bacpac) bacpac->binary_uc1_for_phase2 = prev_bin;

    auto _pt4 = std::chrono::steady_clock::now();   // [PROF] sub-features (sig360) done
    auto _ms = [](std::chrono::steady_clock::time_point a,
                  std::chrono::steady_clock::time_point b){
      return std::chrono::duration<double, std::milli>(b - a).count();
    };
    LOGI("[PROF] gray:%.2f bin:%.2f ccl:%.2f sig360:%.2f  total:%.2f ms  (labels:%d %dx%d)",
         _ms(_pt0,_pt1), _ms(_pt1,_pt2), _ms(_pt2,_pt3), _ms(_pt3,_pt4), _ms(_pt0,_pt4),
         (int)ldData.size(), binary_img_storage.cols, binary_img_storage.rows);
  return 0;
}

const FeatureReport* FeatureManager_binary_processing_group::GetReport()
{
  if(binaryFeatureBundle.size()!=sub_reports.size())
  {
    sub_reports.resize(binaryFeatureBundle.size());
  }
  
  report.data.binary_processing_group.error = error;

  for(int i=0;i<sub_reports.size();i++)
  {
    sub_reports[i] = binaryFeatureBundle[i]->GetReport();
  }
  report.type = FeatureReport::binary_processing_group;
  report.data.binary_processing_group.reports = &sub_reports;
  report.data.binary_processing_group.labeledData = &ldData;
  report.data.binary_processing_group.subFeatureDefSha1 = subFeatureDefSha1;
  report.data.binary_processing_group.mmpp =
      (bacpac && bacpac->sampler) ? bacpac->sampler->mmpP_ideal() : NAN;
  return &report;
}

cJSON *FeatureManager_binary_processing_group::getShapeFeaturePointsJson()
{
  for (auto *sub : binaryFeatureBundle)
  {
    cJSON *j = sub->getShapeFeaturePointsJson();
    if (j != NULL) return j;
  }
  return NULL;
}


void FeatureManager_binary_processing_group::ClearReport()
{
  if(binaryFeatureBundle.size()!=sub_reports.size())
  {
    sub_reports.resize(binaryFeatureBundle.size());
  }

  for(int i=0;i<sub_reports.size();i++)
  {
    binaryFeatureBundle[i]->ClearReport();
  }
  report.type = FeatureReport::binary_processing_group;
  report.data.binary_processing_group.reports = &sub_reports;
  report.data.binary_processing_group.labeledData = &ldData;
  report.data.binary_processing_group.error=error=FeatureReport_ERROR::NONE;
}

int FeatureManager_binary_processing_group::parse_jobj()
{
  // "intrusionSizeLimitRatio" is no longer read. Old defs still carry the key and
  // that is fine -- an unknown key is ignored, so a factory def loads unchanged
  // and simply stops being gated by it. See FeatureMatching() for what replaced it.

  // Pre-binarization downsample. 1 = off (default). 2 or 4 cuts threshold/CCL/
  // signature build to 1/N^2 of the work. Coordinate scale-back lives in the
  // existing dsampLevel plumbing on sub-features.
  this->inspection_downsample = 1;
  double *dsamp = JFetch_NUMBER(root, "inspection_downsample");
  if (dsamp != NULL && *dsamp >= 1 && *dsamp <= 8)
    this->inspection_downsample = (int)*dsamp;

  FeatureManager_group_proto::parse_jobj();

  strcpy(subFeatureDefSha1,"");
  const char *sSet_sha1= JFetch_STRING(root,"featureSet_sha1");
  if(sSet_sha1!=NULL)
  {
    strncpy(subFeatureDefSha1,sSet_sha1,sizeof(subFeatureDefSha1));
    subFeatureDefSha1[sizeof(subFeatureDefSha1)-1]='\0';
  }

  return 0;
}



/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_group::FeatureManager_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
  
  ClearReport();
  
}

int FeatureManager_group::clearFeatureGroup()
{
    for(int i=0;i<featureBundle.size();i++)
    {
      delete featureBundle[i];
    }
    featureBundle.resize(0);
    return 0;
}

int FeatureManager_group::addSubFeature(cJSON * subFeature)
{
  char *str=JFetch_STRING(subFeature,"type");
  if(str==NULL)
  {
    return -1;
  }
  FeatureManager *newFeature=NULL;
  if(strcmp(FeatureManager_group::GetFeatureTypeName(),str) == 0)
  {

    // Was printing the whole sub-def into the log AND leaking that copy, on
    // top of the one below.
    MallocHold _s(cJSON_Print(subFeature));
    LOGI("FeatureManager_group is the type...:%s", _s.str());
    newFeature = new FeatureManager_group(_s.str());
  }
  else if(strcmp(FeatureManager_binary_processing_group::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_binary_processing_group is the type...");
    MallocHold _s(cJSON_Print(subFeature));
    newFeature = new FeatureManager_binary_processing_group(_s.str());
  }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  featureBundle.push_back(newFeature);
  return 0;
}


int FeatureManager_group::FeatureMatching(cv::Mat &img)
{
  for(int i=0;i<featureBundle.size();i++)
  {
    featureBundle[i]->setBacPac(bacpac);
    featureBundle[i]->FeatureMatching(img);
  }
  return 0;
}

