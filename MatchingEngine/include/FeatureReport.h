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

//typedef struct FeatureReport_binary_processing_group;
typedef struct FeatureReport_sig360_extractor{
  vector<acv_XY> *signature;
  vector<acv_CircleFit> *detectedCircles;
  vector<acv_LineFit> *detectedLines;
  enum{
    NONE,
    ONLY_ONE_COMPONENT_IS_ALLOWED,
    END
  } error;
};


typedef struct FeatureReport_judgeDef{

  char name[FeatureManager_NAME_LENGTH];

  enum{
    AREA,
    SIGMA,
    ANGLE,
    DISTANCE,
  } measure_type;
  enum{
    NONE,
    LINE,
    CIRCLE,
    AUX_LINE,
    AUX_CIRCLE,
    AUX_POINT
  } OBJ1_type,OBJ2_type;
  int OBJ1_idx;
  int OBJ2_idx;
  char OBJ1[FeatureManager_NAME_LENGTH];
  char OBJ2[FeatureManager_NAME_LENGTH];
  bool swap;


  float targetVal;
  float targetVal_margin;
}FeatureReport_judgeDef;

typedef struct FeatureReport_judgeReport{

  FeatureReport_judgeDef def;
}FeatureReport_judgeReport;


typedef struct FeatureReport_sig360_circle_line_single{
  vector<acv_CircleFit> *detectedCircles;
  vector<acv_LineFit> *detectedLines;
  vector<acv_Line> *detectedAuxLines;
  vector<acv_XY> *detectedAuxPoints;
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
