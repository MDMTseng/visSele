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


#define FeatureManager_NAME_LENGTH 32

class FeatureManager_BacPac;
enum FeatureReport_ERROR {
  NONE                            = 0,
  GENERIC                         = 1,
  ONLY_ONE_COMPONENT_IS_ALLOWED   = 2,
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
  /*

  We will rotate the picture to let image line contour pixel lie on horizontal position
                |MatchingMarginX-->

                 Y
                 ^
     ____________|_____________
     |           |            |          ^
  ---|-----------|------------|--->X     | initMatchingMargin
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
  union data{
    struct anglefollow{
      acv_XY position;
      int target_id;
      float angleDeg;
      bool search_far;
      bool locating_anchor;
    }anglefollow;
  }data;
  vector <ContourFetch::ptInfo> tmp_pt;
  Roughness_INFO ri;
}featureDef_searchPoint;


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
  float targetVal,value_adjust;
  float USL,LSL;
  float UCL,LCL;
  
  float targetVal_b,value_adjust_b;
  float USL_b,LSL_b;
  float UCL_b,LCL_b;
  
  bool quality_essential;
  bool orientation_essential;
  
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
}FeatureReport_circleReport;



typedef struct FeatureReport_auxPointReport{
  featureDef_auxPoint *def;
  acv_XY pt;
  int status;
}FeatureReport_auxPointReport;


typedef struct FeatureReport_searchPointReport{
  featureDef_searchPoint *def;
  acv_XY pt;
  int status;
}FeatureReport_searchPointReport;



typedef struct FeatureReport_sig360_circle_line_single{
  vector<FeatureReport_circleReport> *detectedCircles;
  vector<FeatureReport_lineReport> *detectedLines;
  vector<FeatureReport_auxPointReport> *detectedAuxPoints;
  vector<FeatureReport_searchPointReport> *detectedSearchPoints;
  vector<FeatureReport_judgeReport> *judgeReports;

  acv_XY LTBound;
  acv_XY RBBound;
  acv_XY Center;
  float area;
  int pix_area;
  int labeling_idx;
  float rotate;
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
    END
  } type;
  string name;
  FeatureManager_BacPac *bacpac;
  union{
    void* raw;
    FeatureReport_nop                     nop;
    FeatureReport_binary_processing_group binary_processing_group;
    FeatureReport_sig360_extractor        sig360_extractor;
    FeatureReport_sig360_circle_line      sig360_circle_line;
    FeatureReport_camera_calibration      camera_calibration;
    FeatureReport_stage_light_report      stage_light_report;
    FeatureReport_cjson_report                  cjson_report;
  }data;
  string info;
}FeatureReport;




#endif
