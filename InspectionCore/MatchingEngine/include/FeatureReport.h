#ifndef FeatureREPORT_HPP
#define FeatureREPORT_HPP

// Optional debug payloads carried under a report's "extra" object. See the
// registry in FeatureReport_UTIL.cpp for what exists, what it costs, and why
// the archive can strip all of it by removing one key.
struct cJSON;
bool DbgEmit(const char *name);
int  DbgEmitSet(cJSON *cfg);


#include "vis_geom.h"
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

// THE EVIDENCE A THRESHOLD IS SET AGAINST.
//
// edge.min_strength decides which gradient peaks count as edges at all, and it
// is expressed in raw gradient units -- 0..~460 on this station, depending on
// contrast, exposure and lens. That is not a number anybody can arrive at by
// thinking, and the defs show it: 10 is the WebUI's default, the field defs
// carry 30, one carries 0, against real edges measuring 380-460. The floor is
// effectively off, and something else has been silently standing in for it.
//
// So the panel has to show what is actually there. Not the chosen edge -- the
// WHOLE across-edge gradient profile, ungated, including the peaks that fall
// below the current setting. A slider can only be moved down on evidence that
// exists below it, and the gap between the noise peaks and the real one is the
// only thing that says where the floor belongs.
//
// Ungated also means the slider is a local computation: the profile carries
// every candidate, so moving it re-picks in the browser with no round trip.
//
// Signed, because polarity is part of the selection (rising/falling/any) and a
// magnitude cannot express it. Index i is at (-L + i*step) px across the edge,
// measured from the caliper centre along the search direction.
struct CaliperProfiles
{
  std::vector<std::vector<float>> grad;   // one per caliper, nAcross entries
  float step = 0;                         // px between samples across the edge
  float L    = 0;                         // half-span; i=0 sits at -L
};

// The same question for a SEARCH POINT, which needs a different answer.
//
// A caliper averages along the edge and picks a peak out of one profile, so the
// profile is the evidence. A search point does not average: it finds a peak per
// ROW independently and then takes the one NEAREST the origin along the search
// direction. What a threshold acts on there is the candidate set -- every
// per-row peak in the window -- and two things decide the answer: how strong a
// candidate is, and how far along the search it sits.
//
// So this carries the candidates themselves, ungated, rather than a curve. pos
// is the distance along the search direction in px, increasing away from the
// origin, so the first hit is the smallest. str is the peak height BEFORE the
// selector's own 0.40-of-the-strongest gate, which is the whole point: a
// threshold cannot be moved down onto candidates that were filtered out before
// anyone could see them.
struct SearchPointPeaks
{
  std::vector<float> pos;
  std::vector<float> str;
  // Where along the BAR the candidate sits. Not a measurement -- the scan
  // localizes one axis -- but it is what makes the candidate cloud a SHAPE
  // rather than a list, and the shape is the point when the edge being found is
  // curved.
  //
  // Locating the apex of a shallow arc is the case: each row's first hit varies
  // with the square of its distance from the apex, and the algorithm returns
  // the weighted centroid of everything within include_range of the nearest
  // hit -- which sits DEEPER than the apex, by more the wider that band is.
  // Widening it for noise and then dialling manual_offset back by eye is the
  // workflow that follows, and with `along` the offset is a fit rather than a
  // judgement.
  std::vector<float> along;
  float span = 0;           // how far the search reaches (px)
  float mmpp = 0;           // px -> mm, so an offset can be suggested in def units
  // THE ANSWER, in the same frame as the candidates.
  //
  // Without it a panel wanting to say "the result sits N px from the apex" has
  // to re-implement the selection -- the band, the alpha taper, the weighting --
  // and then drifts from it at the first change. sel_pos is what the scan
  // returned along the search direction, before manual_offset is applied, so an
  // offset suggested against it is the whole correction and not part of one.
  float sel_pos = 0, sel_along = 0;
  bool  sel_ok = false;
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
  // The inspection region the operator drew is smaller than the part it has to
  // contain, so the locator has nowhere to place the template and CANNOT find
  // anything -- at any angle, on any frame. Raised instead of returning an
  // empty report, because "region too small" and "no part present" are the same
  // silence otherwise, and the fix (drag the box bigger) is only obvious once
  // the machine says which one it is.
  INSP_REGION_TOO_SMALL           = 5,
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
  //   (blur was removed: search_point_cv never read it -- the fused 3x3 Sobel
  //    subsumes it -- so it was a knob that did not exist. 2026-08-26)
  //   alpha_keep:  outlier-prune fraction in the WLS apex average (0 = none).
  //   (mask_dilate was removed for the same reason as blur, 2026-08-26: it
  //    dilated an object-label mask that had had no producer since 2026-05-29,
  //    so it was honoured all the way into search_point_cv and then had nothing
  //    to act on. A def that still carries the key is warned about, not
  //    silently obeyed.)
  // NO LONGER "0 means use the tuned default" -- see edge_set below. 0 is a
  // value the def can mean, and it is honoured.
  float alpha_keep;
  //   rel_strength: THE RULE THAT USED TO HAVE NO NAME.
  //
  //     search_point_cv keeps candidates whose peak is at least this fraction
  //     of the STRONGEST peak anywhere in the window, then takes the nearest
  //     survivor. It was a hard-coded 0.40 with no setting and no display, and
  //     it is load-bearing: measured on the 10155 def, three of nine search
  //     points are held on their edge by it alone -- one of them 13.8px (192um)
  //     from the nearest candidate that min_strength admits.
  //
  //     That is a threshold relative to whatever else happens to be in the
  //     window, so a neighbouring part or a burr raises it and the measured
  //     point can move to a different edge with nothing said. It should not
  //     exist once min_strength is set against real evidence -- which is what
  //     the edge-profile panel is for -- but deleting it would silently change
  //     every def in the field that is currently leaning on it.
  //
  //     So it becomes a number: default 0.40, exactly today's behaviour, and 0
  //     turns it off. Removal is now a per-def decision somebody makes on
  //     purpose, and the core says out loud when a def is relying on it.
  float rel_strength;      // default 0.40
  // WHICH of the edge knobs the def actually said something about.
  //
  // Every read used to be `(x > 0) ? x : default`, which makes "absent" and
  // "the operator deliberately wrote 0" the same thing. They are different
  // intentions: 0 for a strength floor means "no floor", and the machine was
  // quietly running 10 instead. A number typed on a screen was not the number
  // the machine used, with nothing anywhere saying so.
  //
  // Present -- INCLUDING 0 -- is now honoured exactly. Absent is answered per
  // knob, because the knobs are not the same kind of thing:
  //
  //   REQUIRED (min_strength, in caliper mode): the measurement is NA and says
  //     which knob was missing. A base value the result depends on must not be
  //     guessed; guessing it is deciding a verdict.
  //
  //     Note WHY that can be required at all, because it constrains every
  //     field added after this one: requiredness only works inside an opt-in
  //     container. `edge` is that container -- absent means contour mode and
  //     nothing is required, present means somebody chose caliper mode. A bare
  //     new scalar could never be required, because every def written before
  //     it existed would go NA. See the contract in HANDOVER_2026-08-26.
  //   OPTIONAL (include_range): absent, or an explicit 0, means the step is
  //     not applied. That was already the guard inside search_point_cv; only
  //     the def-side `? : 2.0` hid it.

  uint32_t edge_set;
  enum EdgeSetBit {
    EDGE_SET_MIN_STRENGTH = 1u << 0,
    EDGE_SET_INCLUDE_RANGE= 1u << 1,
    EDGE_SET_MANUAL_OFFSET= 1u << 2,
    EDGE_SET_ALPHA_KEEP   = 1u << 4,
    EDGE_SET_REL_STRENGTH = 1u << 6,
    // 1u << 5 was EDGE_SET_MASK_DILATE. Left as a hole rather than reused: a
    // new knob taking that bit would read as "set" on nothing, but the number
    // is in dumps and logs going back months and a reused bit makes those lie.
  };
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
  // Only when DEBUG_EMIT edge_profile is on; nothing is sampled for it
  // otherwise. See CaliperProfiles.
  CaliperProfiles cal_prof;
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
  CaliperProfiles cal_prof;    // see FeatureReport_lineReport
}FeatureReport_circleReport;



typedef struct FeatureReport_auxPointReport{
  featureDef_auxPoint *def;
  acv_XY pt;
  int status;
  // Same field, same reason, as the search point's: an NA that cannot say why
  // is as unhelpful as a silent substitution. Empty unless there is a reason.
  char na_reason[48];
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
  // Why this point is NA, when the answer is "the recipe did not say".
  //
  // An NA with no reason is the same disease as a silent substitution: the
  // screen cannot explain what the machine did. Empty unless a required knob
  // was missing.
  char na_reason[48];
  // Per-edge points produced by the caliper-mode scan (one per strength-gated
  // row edge). status: 2 = within considerRange of the top (used in the final
  // average), 1 = strength-gated edge outside the consider band. Empty in
  // contour mode. Coords converted to OBJECT-FRAME mm by SPointMatching_ReportGen.
  std::vector<CaliperHit> cal_hits;
  // How many of the configured scan columns had image under them, and how many
  // were configured. A window that hangs off the frame measures from the
  // fraction that is left and used to report SUCCESS exactly like a full one --
  // see the coverage check in the caliper branch of SPointMatching.
  // 0/0 = not a caliper-mode scan.
  int cal_used = 0;
  int cal_total = 0;
  // Only when DEBUG_EMIT edge_profile is on. See SearchPointPeaks.
  SearchPointPeaks cal_peaks;
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
  // Objects the working region threw away before any measurement ran.
  //
  // It lives on the CONTAINER because it is the one number still meaningful
  // when `reports` is empty -- and empty is exactly the case that needs
  // explaining. Without it, "the region rejected everything" and "nothing was
  // found" are the same reply, and the difference was visible only in a log
  // line nobody reads until they already suspect it.
  int region_dropped;

  // WHICH LOCALIZER ACTUALLY RAN, which is not what the def asked for.
  //
  // `locating_engine: "shape_based"` is a request. The core honours it only
  // when shape training succeeded; otherwise it falls THROUGH to the sig360
  // path and measures anyway (FeatureManager_sig360_circle_line.cpp: the
  // `locating_engine == 1 && shape_ready` gate). Both outcomes produce a normal
  // report, and nothing on screen distinguished them -- so a def could be
  // localized by the engine its author had migrated away from, correctly, for
  // as long as nobody looked.
  //
  // Recorded per inspection rather than read from the def, because the def
  // cannot know. 0 = sig360 signature, 1 = shape_based (line2Dup + ROI refine).
  int locator_used;

  // WHY THERE IS NO OBJECT, when there is no object.
  //
  // Same argument as region_dropped one field up, applied to the step before
  // it. A locator that rejects everything and a scene with nothing in it give
  // the identical reply -- an empty `reports` -- and the number that separates
  // them (how close the best candidate came) was computed, compared, and then
  // thrown away into a log line.
  //
  // That number is the one to tune against: "0.87 against a 0.90 floor" is a
  // threshold or a lighting problem, "no candidate at all" is a training or a
  // framing problem, and they are not fixed the same way. Without it the screen
  // can only say the part was not found.
  struct LocateOutcome {
    float best;        // best score any candidate reached; NaN = none computed
    float thres;       // the floor it was measured against
    int   candidates;  // raw candidates the matcher produced, before thresholding
    // 160, not 64: the first version truncated
    // "...open the SBM studio, press g" mid-word, and this string is the whole
    // point of the field -- it is what the operator reads instead of a blank
    // "not found". A reason that stops mid-instruction is worse than none.
    char  reason[160]; // empty when the locate succeeded
    // The same thing as a STABLE TOKEN, because `reason` is prose meant for a
    // human and a screen that branches on prose breaks the day somebody
    // rewords it -- silently, since a regex that stops matching just shows
    // nothing. One of: untrained | train_failed | no_candidate | below_thres |
    // no_region. Empty when there is nothing to say.
    char  code[16];
  } locate;
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
