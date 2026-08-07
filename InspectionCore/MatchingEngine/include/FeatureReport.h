#ifndef FeatureREPORT_HPP
#define FeatureREPORT_HPP


#include "acvImage_ComponentLabelingTool.hpp"
#include "acvImage_BasicTool.hpp"
#include <vector>
#include <string>
#include "cJSON.h"
#include "ImageSampler.h"
#include "FeatureManager.h"
#include "ContourGrid.h"

// Per-caliper hit, populated by the caliper_locate_* helpers and rebased to
// image coords by the per-primitive caller (LineMatching_caliper /
// CircleMatching_caliper). Defined HERE rather than Caliper.h to avoid a
// circular include (Caliper.h pulls FeatureManager.h, which pulls this).
//   status: 0=missed (no peak found), 1=outlier (MAD-rejected), 2=inlier.
//   pt    : sub-pixel edge position in image coords; undefined when status==0.
//   strength: per-caliper confidence (strength * unambiguity * sharpness),
//             also used as the WLS/Kasa fit weight; 0 when status==0.
struct CaliperHit
{
  acv_XY pt;
  int status;
  float strength;
};


#define FeatureManager_NAME_LENGTH 32

class FeatureManager_BacPac;
enum FeatureReport_ERROR {
  NONE                            = 0,
  GENERIC                         = 1,
  ONLY_ONE_COMPONENT_IS_ALLOWED   = 2,
  // Reserved, no longer raised. The intrusionSizeLimitRatio gate that set it was
  // removed 2026-08-07 (obj_detect clean-space regions replace it). The value
  // stays so the codes after it do not shift under anything holding an old report.
  EXTERNAL_INTRUSION_OBJECT       = 3,
  DIRTY_BACKGROUND                = 4,
  END
};
typedef struct FeatureReport;

typedef struct {
  int type;
  int setup;
  
  float rough_threshold;
} Roughness_INFO;

typedef struct {
  vector<acv_LabeledData> *labeledData;
  vector<const FeatureReport*> *reports;
  FeatureReport_ERROR error;
  char *subFeatureDefSha1;
  float mmpp;
} FeatureReport_binary_processing_group;

typedef struct featureDef_circle{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  acv_XY pt1,pt2,pt3;//three points arc, the root of all info
  float initMatchingMargin;
  float outter_inner;
  // caliper/section locating (docs/caliper_primitive_locating_design.md).
  // Width/length/step are stored here in the SAME unit as on the wire
  // (def-file mm); the per-primitive *_ReportGen function converts them to
  // px (* ppmm or / mmpp) before constructing CaliperParams.
  int locating;            // 0=contour(default), 1=caliper
  int cal_count;           // # radial calipers along the arc
  float cal_width;         // projection width (mm at def-level; px at use)
  float cal_length;        // radial search half-length (mm); <=0 => use initMatchingMargin
  float cal_step;          // across-edge sampling step (mm); <=0 => 1px
  int   cal_min_inliers;   // <=0 ⇒ engine default (3 for circle)
  float cal_max_error;     // mm at def-level; <=0 ⇒ no cap on MAD threshold
  // Envelope-fit mode: keep the LS-fit center, recompute the radius as
  //   0=ls    (default, no change)
  //   1=outer (max |center - hit|; min-circumscribed-radius assuming LS center)
  //   2=inner (min |center - hit|; max-inscribed-radius assuming LS center)
  // Applies to both caliper and contour modes; uses inlier hits / s_points.
  // NOTE: this is the LS-center variant, NOT true Welzl min-enclosing.
  int   fit_mode;
  int edge_method;
  int edge_polarity;
  int edge_nth;
  float edge_min_strength;
  vector <ContourFetch::ptInfo> tmp_pt;
  Roughness_INFO ri;
}featureDef_circle;
typedef struct featureDef_line{
  int id;
  char name[FeatureManager_NAME_LENGTH];

  
  acv_XY p0,p1;
  float cache_r0,cache_r1;
  
  float initMatchingMargin;//It's the matching margin

  acv_Line lineTar;
  acv_XY searchVec;//The vector to searching the contour edge
  acv_XY searchEstAnchor;//The vector to searching the contour edge
  float MatchingMarginX;//the length of the line itself
  bool vertex_touch_searching;

  // Caliper/section locating (docs/caliper_primitive_locating_design.md).
  // locating: 0=contour(default,legacy), 1=caliper. edge_* feed EdgeSelectParams.
  // Width/length/step are def-file mm; LineMatching_ReportGen converts them
  // to px (/= mmpp) before the matching engine consumes them.
  int locating;            // default 0
  int cal_count;           // # calipers along the line
  float cal_width;         // projection width (mm at def-level; px at use)
  float cal_length;        // search half-length across the edge (mm); <=0 => use initMatchingMargin
  float cal_step;          // across-edge sampling step (mm); <=0 => 1px
  int   cal_min_inliers;   // <=0 ⇒ engine default (2 for line)
  float cal_max_error;     // mm at def-level; <=0 ⇒ no cap on MAD threshold
  int edge_method;         // EdgeSelectParams::Method
  int edge_polarity;       // EdgeSelectParams::Polarity
  int edge_nth;
  float edge_min_strength;
  /*

  We will rotate the picture to let image line contour pixel lie on horizontal position
                |MatchingMarginX-->

                 Y
                 ^
     ____________|_____________
     |           |            |          ^
  ---|-----------|------------|--->x     | initMatchingMargin
     |___________|____________|          v
  
  */

  vector <ContourFetch::ptInfo> tmp_pt;
  Roughness_INFO ri;
}featureDef_line;



typedef struct featureDef_auxPoint{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  enum{
    lineCross,
    centre
  }subtype;
  
  union{
    struct{
      int line1_id;
      int line2_id;
    }lineCross;
    struct{
      int obj1_id;
    }centre;


  }data;
}featureDef_auxPoint;


typedef struct featureDef_searchPoint{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  enum{
    anglefollow
  }subtype;
  float width;
  float margin;
  // Caliper/section locating (docs/caliper_primitive_locating_design.md).
  // locating: 0=contour(default,legacy), 1=caliper (single caliper_measure along
  // the search vector). edge_* feed EdgeSelectParams. caliper geometry reuses
  // margin(=search half-length across edge) and width(=projection width).
  int locating;            // default 0
  int edge_method;
  int edge_polarity;
  int edge_nth;
  float edge_min_strength;
  // Caliper-mode (locating==1) only:
  //   include_range: perpendicular band (mm) below the top-most strength-gated
  //     edge used in the WLS apex average. 0 → core default (~2 px).
  //   manual_offset: bias (mm) applied to the final pt along the search
  //     direction AFTER the algorithm finds the edge. Positive → further
  //     along the scan (toward search_far if set, else toward the part).
  float include_range;     // default 0
  float manual_offset;     // default 0
  // search_point_cv tuning knobs (JSON-overridable, all caliper-mode only):
  //   blur:        gaussian kernel along the edge before X-sobel.
  //   alpha_keep:  outlier-prune fraction in the WLS apex average (0 = none).
  //   mask_dilate: object-label mask dilation (px) — keep border edges.
  // 0 / sentinels mean "use the algorithm's tuned default".
  int   blur;              // default 0 → 3
  float alpha_keep;        // default 0
  int   mask_dilate;       // default 0 → 8
  union data{
    struct anglefollow{
      acv_XY position;
      int target_id;
      float angleDeg;
      bool search_far;
      bool locating_anchor;
      // User tag for the TPS morph (mode 2): true = this anchor is 2D-localized
      // (a corner) -> constrains both axes; false = edge -> constrains only along
      // its search normal. Default false (edge). Ignored by morph modes 0/1.
      bool anchor_corner;
    }anglefollow;
    data() : anglefollow{} {}
  }data;
  vector <ContourFetch::ptInfo> tmp_pt;
  Roughness_INFO ri;
}featureDef_searchPoint;


// "Object detect" region: an axis-aligned (object-frame) rectangle whose brightness and
// Sobel-edge mean/max are measured at the located pose and self-judged. pt1/pt2 are
// opposite corners in object-frame mm. ignore_rotation keeps it axis-aligned; ignore_
// translation pins it to the teach/absolute image position. Each bound is NAN when
// unset (= no limit on that side).
typedef struct featureDef_objDetect{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  acv_XY pt1, pt2;
  bool ignore_rotation;
  bool ignore_translation;
  int downsample;   // >1 = INTER_AREA-downsample the region before stats (speed; note:
                    // mean stays ~constant, but max shrinks and Sobel scale changes)
  // Dark-area check: how much of the region is darker than dark_thresh. NAN = the
  // whole dark measurement is off and dark_ratio/dark_area_mm2 are not computed.
  //
  // This is the "is anything sitting here" statistic, and neither mean nor max is
  // it: on a clean bright field a 0.3mm speck moves bright_mean by a fraction of a
  // grey level, while bright_max fires on one noisy pixel. Counting the pixels
  // under a threshold answers the question that was actually asked.
  float dark_thresh;                            // grey level; pixel < thresh = dark
  float dark_ratio_min,   dark_ratio_max;       // dark px / region px, 0..1
  float dark_area_min,    dark_area_max;        // dark area in mm^2
  // What a violated bound means for the PART. STATUS_NA (default) says the
  // measurement environment is untrustworthy -- do not eject, let it come round
  // again; STATUS_FAILURE says the part itself is bad. See the on_fail comment in
  // ObjDetect_ReportGen for why the default is the cautious one.
  int on_fail;
  float bright_mean_min, bright_mean_max;
  float bright_max_min,  bright_max_max;
  float edge_mean_min,   edge_mean_max;
  float edge_max_min,    edge_max_max;
}featureDef_objDetect;


typedef struct FeatureReport_judgeDef{
  int id;

  char name[FeatureManager_NAME_LENGTH];

  enum{
    NA,
    AREA,
    SIGMA,
    ANGLE,
    DISTANCE,
    RADIUS,
    CALC,
    CIRCLE_INFO,
    ROUGHNESS,
  } measure_type;
  int OBJ1_id;
  int OBJ2_id;
  int ref_baseLine_id;
  float targetVal;
  float USL,LSL;
  float UCL,LCL;
  float value_A,value_B, value_X,value_Y;
  float targetVal_b;
  float USL_b,LSL_b;
  float UCL_b,LCL_b;
  
  bool quality_essential;
  bool orientation_essential;
  
  bool NGasNA;
  bool NAasNG;

  struct data{
    struct ANGLE{
      int quadrant;
      acv_XY pt;
    }ANGLE;
    struct CALC{
      string exp;
      vector<string> post_exp;
    }CALC;
    struct CIRCLE_INFO{
      
      enum{
        NONE,
        MAX_DIAMETER,
        MIN_DIAMETER,
        ROUGHNESS_MAX,
        ROUGHNESS_MIN,
        ROUGHNESS_RMSE,
      } info_type;
      

    }CIRCLE_INFO;
  }data;
}FeatureReport_judgeDef;


typedef struct FeatureReport_judgeReport{

  FeatureReport_judgeDef *def;
  float measured_val;
  int status;
}FeatureReport_judgeReport;


typedef struct FeatureReport_lineReport{
  featureDef_line *def;
  acv_LineFit line;
  int status;
  // Per-caliper hits when locating==1 (caliper). Empty in the contour path.
  // Length == def->cal_count when populated. See Caliper.h CaliperHit.
  std::vector<CaliperHit> cal_hits;
}FeatureReport_lineReport;


typedef struct FeatureReport_circleReport{
  featureDef_circle *def;
  acv_CircleFit circle;
  int status;
  float maxD,minD;
  float roughness_MAX;
  float roughness_MIN;
  float roughness_RMSE;

  acv_XY pt1,pt2,pt3;//mapped 3 pts on circle
  // Per-caliper hits when locating==1 (caliper); empty in the contour path.
  std::vector<CaliperHit> cal_hits;
}FeatureReport_circleReport;



typedef struct FeatureReport_auxPointReport{
  featureDef_auxPoint *def;
  acv_XY pt;
  int status;
}FeatureReport_auxPointReport;


typedef struct FeatureReport_objDetectReport{
  featureDef_objDetect *def;
  float bright_mean, bright_max;
  float edge_mean, edge_max;
  float dark_ratio, dark_area_mm2;   // NAN when def->dark_thresh is unset
  acv_XY corner[4];   // measured region corners, OBJECT-FRAME mm (for the UI overlay)
  int status;
}FeatureReport_objDetectReport;


typedef struct FeatureReport_searchPointReport{
  featureDef_searchPoint *def;
  acv_XY pt;
  int status;
  // Per-edge points produced by the caliper-mode scan (one per strength-gated
  // row edge). status: 2 = within considerRange of the top (used in the final
  // average), 1 = strength-gated edge outside the consider band. Empty in
  // contour mode. Coords converted to OBJECT-FRAME mm by SPointMatching_ReportGen.
  std::vector<CaliperHit> cal_hits;
}FeatureReport_searchPointReport;



typedef struct FeatureReport_sig360_circle_line_single{
  vector<FeatureReport_circleReport> *detectedCircles;
  vector<FeatureReport_lineReport> *detectedLines;
  vector<FeatureReport_auxPointReport> *detectedAuxPoints;
  vector<FeatureReport_searchPointReport> *detectedSearchPoints;
  vector<FeatureReport_objDetectReport> *detectedObjDetects;
  vector<FeatureReport_judgeReport> *judgeReports;

  acv_XY LTBound;
  acv_XY RBBound;
  acv_XY Center;
  float area;
  int pix_area;
  int labeling_idx;
  float rotate;
  float similarity;
  bool  isFlipped;
  float scale;
  char *targetName;
  
  enum FeatureReport_FeatureStatus{
      STATUS_UNSET=-100,
      STATUS_NA=-128,
      STATUS_BAD=-127,
      STATUS_SUCCESS=0,
      STATUS_FAILURE=-1,
  } ;
};




typedef struct FeatureReport_gen{
  // vector<FM_gen_colorInfo> *detectedCircles;
  cJSON *jsonReport;
};


typedef FeatureReport_sig360_circle_line_single FeatureReport_SCLS;


typedef struct FeatureReport_sig360_circle_line{
  vector<FeatureReport_sig360_circle_line_single> *reports;
  FeatureReport_ERROR error;
};


//typedef struct FeatureReport_binary_processing_group;
typedef struct FeatureReport_sig360_extractor{
  vector<acv_XY> *signature;
  vector<acv_CircleFit> *detectedCircles;
  vector<acv_LineFit> *detectedLines;

  acv_XY LTBound;
  acv_XY RBBound;
  acv_XY Center;
  int area;
  float rotate;
  bool  isFlipped;
  float mmpp;
  FeatureReport_ERROR error;
};

typedef struct FeatureReport_camera_calibration{

  FeatureReport_ERROR error;
};

typedef struct stage_light_grid_node_info{
  acv_XY nodeLocation;
  acv_XY nodeIndex;
  float backLightMax;
  float backLightMin;
  float backLightMean;
  float backLightSigma;

  float imageMax;
  float imageMin;
  float sampRate;
  int error;
}stage_light_grid_node_info;

typedef struct FeatureReport_nop{

};

typedef struct FeatureReport_cjson_report{
  cJSON *cjson;
};


typedef struct FeatureReport_custom_report{
  cJSON *cjson;
  void* data;
  int (*func)(struct FeatureReport_custom_report report);
};

typedef struct FeatureReport_stage_light_report{
  vector<stage_light_grid_node_info> *gridInfo;
  int targetImageDim[2];
}FeatureReport_stage_light_report;

typedef struct FeatureReport
{
  enum{
    NONE,
    nop,
    binary_processing_group,
    sig360_extractor,
    sig360_circle_line,
    camera_calibration,
    stage_light_report,
    cjson,
    custom,

    END
  } type;
  string name;
  FeatureManager_BacPac *bacpac;
  union Data {
    void* raw;
    FeatureReport_nop                     nop;
    FeatureReport_binary_processing_group binary_processing_group;
    FeatureReport_sig360_extractor        sig360_extractor;
    FeatureReport_sig360_circle_line      sig360_circle_line;
    FeatureReport_camera_calibration      camera_calibration;
    FeatureReport_stage_light_report      stage_light_report;
    FeatureReport_cjson_report                  cjson_report;
    FeatureReport_custom_report                  custom_report;
    // After Phase 3b made acv_XY a cv::Point2f (non-trivial default ctor),
    // some union members carry non-trivial ctors so the union's implicit
    // default ctor is deleted. Provide an explicit zero-init that just
    // clears the raw bits.
    Data() : raw(nullptr) {}
  }data;
  string info;
}FeatureReport;




#endif
