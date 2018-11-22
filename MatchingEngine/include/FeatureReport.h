#ifndef FeatureREPORT_HPP
#define FeatureREPORT_HPP


#include "FeatureManager.h"
#include "acvImage_ComponentLabelingTool.hpp"
#include <vector>
#include <string>


#define FeatureManager_NAME_LENGTH 32


typedef struct FeatureReport;

typedef struct {
  vector<acv_LabeledData> *labeledData;
  vector<const FeatureReport*> *reports;
} FeatureReport_binary_processing_group;



typedef struct featureDef_circle{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  acv_Circle circleTar;
  float initMatchingMargin;
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
    acv_XY searchStart;
    acv_XY searchVec;
    float searchDist;
  };
  vector<searchKeyPoint> skpsList;
}featureDef_line;



typedef struct featureDef_auxPoint{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  enum{
    lineCross
  }subtype;
  
  union{
    struct{
      int line1_id;
      int line2_id;
    }lineCross;
  }data;
}featureDef_auxPoint;


typedef struct featureDef_searchPoint{
  int id;
  char name[FeatureManager_NAME_LENGTH];
  enum{
    anglefollow
  }subtype;
  
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
    AREA,
    SIGMA,
    ANGLE,
    DISTANCE,
    RADIUS
  } measure_type;
  int OBJ1_id;
  int OBJ2_id;
  float targetVal;
  float targetVal_margin;
}FeatureReport_judgeDef;


typedef struct FeatureReport_judgeReport{

  FeatureReport_judgeDef *def;
  float measured_val;

}FeatureReport_judgeReport;


typedef struct FeatureReport_lineReport{
  featureDef_line *def;
  acv_LineFit line;
}FeatureReport_lineReport;


typedef struct FeatureReport_circleReport{
  featureDef_circle *def;
  acv_CircleFit circle;
}FeatureReport_circleReport;



typedef struct FeatureReport_auxPointReport{
  featureDef_auxPoint *def;
  acv_XY pt;
}FeatureReport_auxPointReport;


typedef struct FeatureReport_searchPointReport{
  featureDef_searchPoint *def;
  acv_XY pt;
}FeatureReport_searchPointReport;



typedef struct FeatureReport_sig360_circle_line_single{
  vector<FeatureReport_circleReport> *detectedCircles;
  vector<FeatureReport_lineReport> *detectedLines;
  vector<acv_Line> *detectedAuxLines;
  vector<FeatureReport_auxPointReport> *detectedAuxPoints;
  vector<FeatureReport_searchPointReport> *detectedSearchPoints;
  vector<FeatureReport_judgeReport> *judgeReports;

  acv_XY LTBound;
  acv_XY RBBound;
  acv_XY Center;
  int area;
  float rotate;
  bool  isFlipped;
  float scale;
  char *targetName;
};


typedef struct FeatureReport_sig360_circle_line{
  vector<FeatureReport_sig360_circle_line_single> *reports;
  enum{
    NONE,
    ONLY_ONE_COMPONENT_IS_ALLOWED,
    END
  } error;
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
  float scale;
  
  enum{
    NONE,
    ONLY_ONE_COMPONENT_IS_ALLOWED,
    END
  } error;
};

typedef struct FeatureReport
{
  enum{
    NONE,
    binary_processing_group,
    sig360_extractor,
    sig360_circle_line,
    END
  } type;
  string name;
  union{
    void* raw;
    FeatureReport_binary_processing_group binary_processing_group;
    FeatureReport_sig360_extractor        sig360_extractor;
    FeatureReport_sig360_circle_line      sig360_circle_line;
  }data;
  string info;
}FeatureReport;




#endif
