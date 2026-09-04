#ifndef FeatureManagerSIG360_CIRCLE_LINE__HPP
#define FeatureManagerSIG360_CIRCLE_LINE__HPP

#include "FeatureManager.h"
#include "FeatureReport.h"

#include "logctrl.h"
#include "FeatureManager_binary_processing.h"
#include <ContourGrid.h>
#include <MatchingCore.h>
#include <memory>
#include <string>

// Shape-based localizer (line2Dup + ROI refine). Held by pointer so the heavy
// header (shape_matcher.h / MIPP) stays out of this widely-included file.
namespace sbm { class ShapeMatcher; struct FeatureSet; }



class ContourSignature
{
public :
  vector<acv_XY>signature_data;
  // Ordered cartesian boundary points (ideal coord) the signature was sampled
  // from. v1 doesn't use this; v2 needs it to re-sample after centroid shift.
  // Populated lazily by convertContourGrid2Signature when v2 is requested.
  vector<acv_XY>cartesian_ideal;
  // Centroid the signature_data was sampled from (ideal coord).
  acv_XY sample_center = {0, 0};
  // |dy| < this threshold skip guard (mm units, same as cartesian_ideal).
  // Mirrors convertContourGrid2Signature's `|diffY| < 0.1` px guard expressed
  // in the cartesian's unit. Populated by v2 init path.
  float dy_skip_thres = 0.0f;
  float mean;
  float sigma;
  float angleOffset;

  ContourSignature(cJSON*);
  ContourSignature(int Len=0);
  int CalcInfo();
  int RELOAD(cJSON*);

  int RESET(int Len);

  float match_min_error(ContourSignature &s,
    float searchAngleOffset,float searchAngleRange,int facing,
    bool *ret_isInv, float *ret_angle);

  void match_span(ContourSignature &s,
    float offset1,float offset2,int count,vector<acv_XY> &error,float stride,bool flip);

  // Re-sample signature_data from cartesian_ideal as seen from `center`.
  // Used by v2 centroid-iteration. Returns false if cartesian_ideal is empty.
  bool resampleFromCenter(acv_XY center);

};


class ConstrainMap
{
  public:
  typedef struct anchorPair{
    acv_XY from;
    acv_XY to;
    acv_XY constrainVector;   // primary (well-localized) direction = major eigenvector
    float weight;             // precision along constrainVector (w_major)
    // Precision along the tangent (perpendicular to constrainVector). Auto-detected
    // from the local structure: 0 = pure edge (1D, aperture along tangent), ~weight
    // = corner (2D). Anything between is a graded ellipse. Used by the anisotropic
    // data term in mode 2 (TPS) and mode 1.
    float w_minor;
  }anchorPair;

  acv_XY center;
  vector<anchorPair> anchorPairs;

  // --- morph model selection (backward compatible: mode 0 == legacy convert_polar) ---
  // 0 = legacy polar/complex distance-weighted similarity-about-center.
  // 1 = directional weighted least-squares similarity (solve() caches it; default).
  // 2 = anisotropic approximating-TPS: nonlinear/regional warp with per-anchor 2x2
  //     precision (edge->corner handled simultaneously); solve_tps() caches it.
  int   mode        = 0;
  float reg         = 1e-3f; // ridge toward identity for the WLS fit (mode 1)
  float outlier_k   = 0;     // >0 enables MAD outlier rejection on anchor residuals
  int   min_anchors = 0;     // fewer valid anchors than this => identity morph (mode 1)
  float tps_lambda  = 0.5f;  // mode 2 bending stiffness (0 -> interpolate; large -> affine)

  // Cached similarity transform for mode 1: [x';y'] = [sa -sb; sb sa][x;y] + [stx;sty]
  double sa = 1, sb = 0, stx = 0, sty = 0;
  int   valid_count = 0;     // # of non-NAN anchors used by the last solve()

  // Cached approximating-TPS warp (mode 2):
  //   f(p) = A[1,x,y] + sum_j c_j * phi(|p - center_j|),  phi(r)=r^2 log r^2.
  bool   tps_valid = false;
  std::vector<acv_XY> tps_centers;       // the from_j the RBF coeffs attach to
  std::vector<double> tps_cx, tps_cy;    // RBF coeffs per center
  double tps_ax[3] = {0, 1, 0};          // affine x-row (identity: x)
  double tps_ay[3] = {0, 0, 1};          // affine y-row (identity: y)

  ConstrainMap()
  {

  }

  void add(acv_XY from,acv_XY to,acv_XY constrainVector,float weight=1.0f,float w_minor=0.0f)
  {
    anchorPairs.push_back((anchorPair){from:from,to:to,constrainVector:acvVecNormalize(constrainVector),weight:weight,w_minor:w_minor});
  }

  int size()
  {
    return anchorPairs.size();
  }
  void clear()
  {
    anchorPairs.clear();
  }

  // Reset the cached mode-1 transform to identity. Call once per frame before the
  // locating iteration so iteration 1 relocates anchors on an un-morphed pose.
  void resetTransform()
  {
    sa = 1; sb = 0; stx = 0; sty = 0; valid_count = 0;
    tps_valid = false; tps_centers.clear(); tps_cx.clear(); tps_cy.clear();
    tps_ax[0] = 0; tps_ax[1] = 1; tps_ax[2] = 0;
    tps_ay[0] = 0; tps_ay[1] = 0; tps_ay[2] = 1;
  }

  // Recompute the cached transform from the currently-filled anchors (mode 1).
  // Returns the number of valid (non-NAN) anchors. No-op for mode 0.
  int solve();

  // Mode 2: fit the anisotropic approximating-TPS from the filled anchors, using
  // each anchor's 2x2 precision (weight along constrainVector, w_minor along the
  // tangent). Returns the number of anchors used. Falls back to identity on too
  // few anchors / singular system.
  int solve_tps();
  acv_XY convert_tps(acv_XY from);



  acv_XY convert_Ave(acv_XY from)
  {
    acv_XY wvecSum={0,0};
    acv_XY vecSum={0,0};

    for(int i=0;i<anchorPairs.size();i++)
    {
      anchorPair pair = anchorPairs[i];
      if(pair.to.x!=pair.to.x)//NAN
      {
        continue;
      }
      float distance=acvDistance(from,pair.from);
      if(distance<0.01)distance=0.01;
      float w=1/distance;


      wvecSum=acvVecAdd(wvecSum,acv_XY(w, w));

      acv_XY vec=acvVecSub(pair.to,pair.from);
      vecSum=acvVecAdd(vecSum,acvVecMult(vec,w));
    }
    // LOGI("vecSum:%f %f wvecSum: %f %f",vecSum.x,vecSum.y,wvecSum.x,wvecSum.y);
    vecSum.x/=wvecSum.x;
    vecSum.y/=wvecSum.y;
    
    LOGI("vecAve:%f %f",vecSum.x,vecSum.y); 
    return acvVecAdd(from,vecSum);
  }



  acv_XY convert_polar(acv_XY from);

  acv_XY convert(acv_XY from);
};


// SBM feature extraction is an AUTHORING action, not something a parse does.
//
// trainShapeMatcher() will take a cached feature set at any time, but it
// refuses to EXTRACT one unless this window is open. Only the SF handler --
// the core's side of the studio's 生成特徵點 -- opens it, so features are
// generated where somebody asked for them and nowhere else. Without this, every
// def load and every II round trip re-derived the same features from the same
// picture, and nothing on screen said when it had happened.
bool shape_extract_allowed();
bool shape_force_extract();
struct ShapeExtractWindow {
  bool prev, prevForce;
  // force = "ignore the def's cache and extract fresh" -- what 生成特徵點 means.
  // Without it a def carrying an unusable cache could not be repaired from the
  // studio, because pressing the button would take that cache every time.
  explicit ShapeExtractWindow(bool force = false);
  ~ShapeExtractWindow();
  ShapeExtractWindow(const ShapeExtractWindow &) = delete;
  ShapeExtractWindow &operator=(const ShapeExtractWindow &) = delete;
};

class FeatureManager_sig360_circle_line:public FeatureManager_binary_processing {

  typedef enum FEATURETYPE {
    
    NA,
    LINE,
    ARC,
    AUX_POINT,
    SEARCH_POINT,
    MEASURE,
    OBJ_DETECT
    };
  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  vector<FeatureReport_judgeDef> judgeList;

  vector<featureDef_auxPoint> auxPointList;
  vector<featureDef_searchPoint> searchPointList;
  vector<featureDef_objDetect> objDetectList;
  int signature_feature_id;
  ContourSignature feature_signature;
  ContourSignature tmp_signature;
  ConstrainMap cm;
  vector<acv_XY>signature_data_buffer;
  ContourFetch edge_grid;

  float matching_angle_margin;
  float matching_angle_offset;
  int matching_face;
  bool matching_without_signature;
  float single_result_area_ratio;

  // Orientation-signature source: 0 = contour_sig (binary silhouette, default,
  // backward compatible) ; 1 = edge_sig (grayscale gradient edge per ray,
  // lighting/exposure/tint robust). Set via def "matching_method".
  int matching_method = 0;
  float edge_sig_min_strength = 8.0f;
  // Ray-cast sampling step (px) for the edge_sig build. Bigger = faster build
  // (the edge is still sub-pixel via the gradient-peak parabola). 1-2deg drift
  // budget tolerates ~1.5-2.0; default 1.5 (~4x faster than 0.5). Speed knob,
  // no signature-format change.
  float edge_sig_ray_step = 1.5f;

  // Orientation-matching algorithm: 1 = v1 (default, brute-force angle scan at
  // fixed centroid, untouched legacy path); 2 = v2 (xrefine wrapped in
  // centroid-iteration: re-samples signature from cos/sin-LSQ-corrected center
  // each iter, typically lifts the match score 0.05-0.25 when seed centroid
  // is biased by scratch/particle). Set via def "matching_version".
  int matching_version = 1;
  int matching_v2_max_iter = 4;
  float matching_v2_tol_mm = 0.002f;

  // --- locating-anchor morph (deformation correction) configuration ---
  // Backward compatible: defaults reproduce the legacy single-pass polar morph.
  // morph_mode    : 0 = legacy convert_polar (default); 1 = directional WLS
  //                 similarity (opt in per def with "wls_similarity").
  // morph_max_iter: locating-pass iterations (1 = single pass; >1 re-locates
  //                 anchors through the current morph until they converge).
  // morph_tol_mm  : per-anchor template-domain convergence tolerance for the loop.
  // morph_outlier_k: >0 enables MAD outlier rejection on anchor residuals (mode 1).
  // morph_min_anchors: below this many valid anchors => identity morph (mode 1).
  int   morph_mode        = 0;
  int   morph_max_iter    = 1;
  float morph_tol_mm      = 0.002f;
  float morph_outlier_k   = 0;
  float morph_reg         = 1e-3f;
  int   morph_min_anchors = 0;
  float morph_tps_lambda  = 0.5f;   // mode 2 (tps) bending stiffness
  // Relaxation / learning-rate for the anchor re-location iteration (def "morph_alpha",
  // default 1 = legacy full warp). <1 places the caliper at lerp(raw, morphed, alpha),
  // damping overshoot so the iteration CONVERGES instead of oscillating on large
  // deformations (only matters when morph_max_iter > 1). morph_place_alpha is the
  // runtime value: set to morph_alpha during the loop, reset to 1 for final measure.
  float morph_alpha       = 1.0f;   // def-configured relaxation factor
  float morph_place_alpha = 1.0f;   // runtime (loop = morph_alpha, final = 1)

  float sig_st1_matching_sim_thres;

  //it's the relativesimilar thres  <= 1- min_error/cur_angle_error
  float sigRelativeMatchSimThres=0.9;
  //it's the absolute similar thres <= 1-normalized error = 1-error/sig.mean
  float sigMatchSimThres=0.8;
  //

  vector<FeatureReport_sig360_circle_line_single> reportDataPool;
  vector<FeatureReport_sig360_circle_line_single> reports;

  cv::Mat p_cropImg_cv;       // currently labeled-image or original-image view
  acv_XY cropOffset;


  vector<ContourFetch::ptInfo > tmp_points;
  vector<ContourFetch::contourMatchSec > m_sections;

  // --- shape-based localizer (opt-in via def "locating_engine":"shape_based") ---
  // 0 = sig360 contour signature (default/legacy); 1 = shape_based (line2Dup +
  // ROI refine). When shape_based is requested but training fails (no template
  // image), we fall back to sig360 so the def still runs.
  int locating_engine = 0;
  bool shape_ready = false;
  std::shared_ptr<sbm::ShapeMatcher> shapeMatcher;
  std::shared_ptr<sbm::FeatureSet> shapeFeatureSet;
  // Magnification portability: the template is extracted at the def's teach pixel
  // scale (def_mmpp). When this def runs on a camera with a different mmpp the part
  // appears at a different pixel size, so the matched model variants must be scaled
  // by def_mmpp/current_mmpp (the SBM analog of sig360 re-deriving its mm signature
  // radius at the live mmpp). buildShapeMatcher() rebuilds the matcher at a given
  // scale from the shape_* members; shape_built_scale = the scale it currently holds.
  float shape_built_scale = 1.0f;
  // Trained feature geometry exposed for UI visualization (the "生成特徵點" round-trip):
  // the line2Dup gradient features and the ROI refine sample points, in OBJECT-FRAME mm
  // (the def's own frame, same as localization_include). Populated by trainShapeMatcher.
  std::vector<acv_XY> shape_feat_mm;   // line2Dup gradient features (finest pyramid level)
  std::vector<acv_XY> shape_roi_mm;    // ROI refine sample points
  // Serialised training result, carried in the def as "__shape_cache".
  //
  // The features are a deterministic function of (reference image, extraction
  // params). Recomputing them on every def load costs the Otsu + connected-
  // components + extractFeatures pass, and -- worse -- silently re-derives the
  // template if the extractor ever changes behaviour. Storing them makes a def
  // describe its own localiser instead of a recipe for rebuilding it.
  //
  // The reference IMAGE is deliberately NOT stored: ROI refine reads it from the
  // sidecar, so the def stays small and the picture stays a picture. That means
  // the sidecar imread still happens on load -- but a def is loaded once per
  // inspection session, so it costs nothing per part.
  //
  // __ prefix: double-underscore keys are stripped from featureSet_sha1, so a
  // cached def still hashes identically to the same def without a cache.
  // (_ref_image_path's single underscore was the bug this avoids.)
  cJSON *shape_cache_in = NULL;        // borrowed from `root`, valid while root is
  std::string shape_cache_fp;          // fingerprint of what produced the live set
  cv::Rect    shape_crop;              // crop of the sidecar that IS the template
  cv::Point2f shape_origin_in_crop{0, 0};
  std::string reference_image_name;   // optional explicit sidecar PNG (relative)
  std::string def_path;               // full path of the .hydef (for <base>.png)
  std::string ref_image_path;         // transient FULL path to the reference image,
                                      // supplied at runtime (e.g. WebUI def-info "_ref_image_path");
                                      // highest priority so the def file stays path-free.
  float shape_min_score = 50.0f;      // line2Dup similarity gate (0-100)
  // How far apart in DEGREES two matches at the same place must be to both
  // survive NMS. 360 keeps the previous behaviour exactly -- no angle
  // difference can exceed it, so every nearby match is suppressed and the
  // matcher returns one pose per location.
  //
  // That default silently disabled the angle-aware branch of the NMS, which was
  // written for precisely the case it then could not serve: a part close to
  // rotationally symmetric, where two orientations score almost the same and
  // the WRONG one can win by noise. Keeping both and letting an
  // orientation-essential judge reject the wrong one is what the downstream
  // loop already does -- it tries every candidate and keeps those that pass.
  // It just never had more than one candidate to try.
  //
  // Set it to a value below the real ambiguity (e.g. 30 for a part whose two
  // plausible poses differ by 180) to opt in. Costs a full measurement pass per
  // extra candidate, which is why it is opt-in rather than a new default.
  float shape_nms_angle = 360.0f;
  float shape_angle_step_deg = 1.0f;  // template rotation granularity
  float shape_match_scale = 1.0f;     // <1 downscales the scene for the coarse
                                      // match (ROI refine restores full-res accuracy)
  // line2Dup feature/pyramid tuning (def-overridable). Applied to BOTH the
  // template extraction and the scene matcher so their edges stay consistent.
  int   shape_num_features = 128;     // max gradient features per template
  std::vector<int> shape_pyramid_T{4, 8}; // pyramid decimation strides (fine->coarse)
  float shape_weak_thres   = 50.0f;   // min gradient magnitude to be an edge
  float shape_strong_thres = 80.0f;   // strong-edge preference magnitude
  int   shape_blur         = 7;       // pre-gradient Gaussian kernel (scene)
  float def_mmpp = 0.0f;              // def's mm-per-pixel (for signature->px mask)
  // Raw sig360 radius signature (mm) captured before the in-place high-pass in
  // sign360_process -- the absolute part silhouette, used to build the shape
  // feature mask. .x = radius(mm), .y = absolute angle(rad).
  vector<acv_XY> raw_sig_radius;
  // Reference origin recorded in the def's sign360 feature (pt1, in mm/ideal) and
  // its orientation -- the "original center location" the signature was sampled
  // around. Used as the shape-localizer origin (def_image_reg overrides if present).
  acv_XY ref_center_mm = {0, 0};
  bool   has_ref_center = false;
  float  ref_orientation = 0;
  // def_image_reg: the part's registered pose (sig360 center+angle) in the saved
  // reference image, recorded by the WebUI at save time. Highest-priority source
  // for the shape-localizer origin + angle (overrides sign360 pt1 when present).
  acv_XY reg_center_mm = {0, 0};   // cx,cy (mm/ideal)
  float  reg_angle_rad = 0;        // angle (rad)
  bool   reg_flipped = false;
  bool   has_reg = false;
  // Optional localization ROI: a polygon (object-frame mm, relative to the
  // registration origin) restricting WHERE the shape locator extracts its
  // gradient features. Localizing only on the rigid (deformation-invariant)
  // region keeps the global pose stable when the rest of the part flexes; the
  // morph then corrects the deformable features. Empty => whole silhouette.
  vector<acv_XY> loc_roi_mm;
  // Pure-SBM native feature-extraction region (object-frame mm, origin-relative). The
  // train-time mask = union(loc_incl_mm) AND-NOT union(loc_excl_mm). This is the
  // SBM-native replacement for the sig360-signature silhouette: a migrated def bakes
  // the signature into loc_incl_mm, a fresh def authors it by hand. Each entry is one
  // polygon. Empty include => fall back to loc_roi_mm, then the sig360 signature, then
  // Otsu (see trainShapeMatcher mask-priority).
  // Why the shape locator has nothing to work with, kept so the REPORT can say
  // it. Empty when training succeeded or the def is not shape_based.
  char shape_untrained_reason[128] = {0};
  char shape_untrained_code[16] = {0};
  // The live feature set came from a cache that stores feature levels but no
  // ROI windows, so the matcher runs the coarse stage only (2-3 px instead of
  // sub-pixel). Set at load, reported on EVERY frame as locate.code
  // "coarse_only" -- an accuracy the operator did not ask for must not look
  // like the one they did.
  bool shape_coarse_only = false;
  vector<vector<acv_XY>> loc_incl_mm;   // include polygons (where to extract features)
  vector<vector<acv_XY>> loc_excl_mm;   // exclude polygons ("avoid generation" areas)
  // Explicit user ROI refine points (object-frame mm). When the def carries the
  // "roi_refine_points" key the localizer uses EXACTLY these (empty => no ROI refine,
  // coarse pose only); when the key is absent the matcher auto-selects (legacy).
  vector<acv_XY> roi_pts_mm;
  bool roi_pts_set = false;

public :
  FeatureManager_sig360_circle_line(const char *json_str);
  ~FeatureManager_sig360_circle_line();
  int reload(const char *json_str) override;
  int FeatureMatching(cv::Mat &img_cv) override;
  // Shape-based locating works on the raw grayscale and needs no binarize/CCL
  // /contour-walk; the legacy sig360 signature path still does. Only skip when
  // the shape matcher actually trained -- otherwise FeatureMatching falls back to
  // the sig360 path, which still needs the labeled image.
  bool needsBinaryPreprocessing() override { return !(locating_engine == 1 && shape_ready); }
  virtual const FeatureReport* GetReport() override;
  virtual void ClearReport() override;
  static const char* GetFeatureTypeName(){return "sig360_circle_line";};
  
protected:

  //int parse_search_key_points_Data(cJSON *kspArr_obj,vector<featureDef_line::searchKeyPoint> &skpsList);
  //float find_search_key_points_longest_distance(vector<featureDef_line::searchKeyPoint> &skpsList);
  int parse_arcData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_auxPointData(cJSON * auxPoint_obj);
  int parse_searchPointData(cJSON * searchPoint_obj);
  int parse_objDetectData(cJSON * objDetect_obj);
  int parse_sign360(cJSON * signature_obj);
  int parse_judgeData(cJSON * judge_obj);
  int parse_jobj() override;


  FeatureReport_judgeReport measure_process(FeatureReport_sig360_circle_line_single &report, 
  float sine,float cosine,float flip_f,
  FeatureReport_judgeDef &judge);


  FeatureReport_searchPointReport searchPoint_process(
  FeatureReport_sig360_circle_line_single &report, acv_XY center,
  float sine,float cosine,float flip_f,float thres,
  featureDef_searchPoint &def,edgeTracking &eT);

  FeatureReport_lineReport LineMatching_ReportGen(
  featureDef_line *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);



  FeatureReport_circleReport CircleMatching_ReportGen(
  featureDef_circle *plineDef,edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);

  // Record a locate MISS, keeping the closest one across candidates.
  void noteLocateMiss(float best, float thres, const char *reason);

  FeatureReport_searchPointReport SPointMatching_ReportGen(
  featureDef_searchPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);

  FeatureReport_objDetectReport ObjDetect_ReportGen(
  featureDef_objDetect *def,
  edgeTracking &eT,
  acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);


  FeatureReport_auxPointReport APointMatching_ReportGen(
  featureDef_auxPoint *def,
  FeatureReport_sig360_circle_line_single &singleReport,
  float sine,float cosine,float flip_f
  );

  FeatureReport_judgeReport Judge_ReportGen(
    FeatureReport_judgeDef *def,
    FeatureReport_sig360_circle_line_single &singleReport,
    float sine,float cosine,float flip_f
  );
  
  int TreeExecution(int id,
    FeatureReport_sig360_circle_line_single &singleReport,
    edgeTracking &eT,
    acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);

  int TreeExecution(
    FeatureReport_sig360_circle_line_single &singleReport,
    edgeTracking &eT,
    acv_XY calibCen,float mmpp,float cached_cos,float cached_sin,float flip_f);


  int FindFeatureDefIndex(int feature_id,FEATURETYPE *ret_type);
  int FindFeatureReportIndex(FeatureReport_sig360_circle_line_single &report,int feature_id,FEATURETYPE *ret_type);
  int ParseMainVector(float flip_f,FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *vec);
  int ParseLocatePosition(FeatureReport_sig360_circle_line_single &report,int feature_id, acv_XY *pt);
  int lineCrossPosition(float flip_f,FeatureReport_sig360_circle_line_single &report,int line1_id,int line2_id, acv_XY *pt);

  acv_XY ParseMainVector(featureDef_searchPoint *def_sp);

  int SingleMatching(int lableIdx,acv_LabeledData *ldData,
  int grid_size, ContourFetch &edge_grid,int scanline_skip, FeatureManager_BacPac *bacpac,
  FeatureReport_sig360_circle_line_single &singleReport,
  vector<ContourFetch::ptInfo > &tmp_points,vector<ContourFetch::contourMatchSec >&m_sections);

  // --- shape-based localizer methods ---
  // Build the line2Dup template + ROI feature set from the <base>.png sidecar.
  // Registration (origin/angle) is derived from the template's own silhouette
  // centroid so it shares the sig360 centroid basis. Returns 0 on success.
  int trainShapeMatcher();
  // (Re)build shapeMatcher from the shape_* members with the model variants pre-scaled
  // by `scale` (1.0 = teach pixel scale). Requires shapeFeatureSet to be set. Returns
  // the variant count (>0) on success, <=0 on failure. Shared by trainShapeMatcher
  // (scale 1.0) and ensureShapeScale (live mmpp ratio).
  int buildShapeMatcher(float scale);
  // Copy the trained feature geometry (object-frame mm) out for UI visualization.
  // Returns false if the shape localizer has not trained (shape_ready == false).
  bool getShapeFeaturePoints(std::vector<acv_XY> &feat_mm, std::vector<acv_XY> &roi_mm) const
  {
    if (!shape_ready) return false;
    feat_mm = shape_feat_mm;
    roi_mm  = shape_roi_mm;
    return true;
  }
  // cJSON variant for the WS "SF" round-trip (see FeatureManager base). NULL if the
  // shape localizer has not trained.
  // The studio's preview of what this def locates with: level-0 features and
  // the ROI sample points, in OBJECT-FRAME mm. A member rather than a lambda
  // inside trainShapeMatcher because BOTH ways of arriving at a trained feature
  // set have to fill it -- extraction, and loading a self-contained def. The
  // second one did not, and the studio, which asks the CORE for these rather
  // than computing them itself, reported "no features extracted" for a def that
  // was working perfectly.
  void liftShapeForUI(const sbm::FeatureSet &fs, const cv::Rect &crop,
                      float reg_sin, float reg_cos, float reg_flip_f,
                      const cv::Point2f &originPx);

  cJSON *getShapeFeaturePointsJson() override;
  // Ensure the shape matcher's template variants are scaled for the live mmpp so a
  // def is portable across camera magnifications. Rebuilds shapeMatcher at
  // scale = def_mmpp/current_mmpp when it differs from shape_built_scale (cached, so
  // a fixed-mmpp deployment only pays the variant-regen once). No-op (keeps the
  // train-time scale) when def_mmpp or current_mmpp is unknown. Returns false only on
  // a rebuild failure. Called at the head of FeatureMatching_shape.
  bool ensureShapeScale(float current_mmpp);
  // Shape-based localization path: match the original grayscale, then run the
  // SAME anchor-morph + caliper measurement per detection. Populates `reports`.
  int FeatureMatching_shape();
  // Per-detection morph + measurement for a shape-match pose. Mirrors the tail
  // of SingleMatching but takes the pose (Center preset on singleReport, plus
  // matched angle/flip/score) directly instead of from the signature refine.
  int SingleMatching_shape(FeatureManager_BacPac *bacpac,
    FeatureReport_sig360_circle_line_single &singleReport,
    float matched_angle, bool isInv, float similarity);
};



#endif
