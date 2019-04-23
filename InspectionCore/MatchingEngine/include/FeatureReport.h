#ifndef FeatureREPORT_HPP
#define FeatureREPORT_HPP


#include "FeatureManager.h"
#include "acvImage_ComponentLabelingTool.hpp"
#include "acvImage_BasicTool.hpp"
#include <vector>
#include <string>


#define FeatureManager_NAME_LENGTH 32


enum FeatureReport_ERROR {
  NONE                            = 0,
  GENERIC                         = 1,
  ONLY_ONE_COMPONENT_IS_ALLOWED   = 2,
  EXTERNAL_INTRUSION_OBJECT       = 3,
  END
};
typedef struct FeatureReport;

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
  acv_Circle circleTar;
  float initMatchingMargin;
  float sAngle;
  float eAngle;
  float outter_inner;
}featureDef_circle;
typedef struct featureDef_line{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  acv_Line lineTar;
  acv_XY searchVec;//The vector to searching the contour edge
  acv_XY searchEstAnchor;//The vector to searching the contour edge
  float initMatchingMargin;
  float MatchingMarginX;
  float searchDist;


  typedef struct searchKeyPoint{
    acv_XY keyPt;
  };
  vector<searchKeyPoint> keyPtList;
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
    }anglefollow;
  }data;
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
    RADIUS
  } measure_type;
  int OBJ1_id;
  int OBJ2_id;
  float targetVal;
  float USL,LSL;
  float UCL,LCL;
  
  union data{
    struct ANGLE{
      int quadrant;
    }ANGLE;
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
  float rotate;
  bool  isFlipped;
  float scale;
  char *targetName;
  
  enum FeatureReport_FeatureStatus{
      STATUS_NA=-128,
      STATUS_SUCCESS=0,
      STATUS_FAILURE=-1,
  } ;
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
  acvRadialDistortionParam calib_param;
  
  
  FeatureReport_ERROR error;
};

typedef struct FeatureReport_camera_calibration{

  acvRadialDistortionParam param;
  
  FeatureReport_ERROR error;
};


typedef struct FeatureReport
{
  enum{
    NONE,
    binary_processing_group,
    sig360_extractor,
    sig360_circle_line,
    camera_calibration,
    END
  } type;
  string name;
  union{
    void* raw;
    FeatureReport_binary_processing_group binary_processing_group;
    FeatureReport_sig360_extractor        sig360_extractor;
    FeatureReport_sig360_circle_line      sig360_circle_line;
    FeatureReport_camera_calibration      camera_calibration;
  }data;
  string info;
}FeatureReport;




#endif
