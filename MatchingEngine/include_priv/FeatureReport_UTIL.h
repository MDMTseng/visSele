#ifndef FEATUREREPORT_UTIL_HPP
#define FEATUREREPORT_UTIL_HPP

#include "MatchingEngine.h"


cJSON* acv_LineFit2JSON(cJSON* Line_jobj, const acv_LineFit line, acv_XY center_offset );

cJSON* acv_CircleFit2JSON(cJSON* Circle_jobj,const acv_CircleFit cir , acv_XY center_offset);


cJSON* JudgeReport2JSON(const FeatureReport_judgeReport judge , acv_XY center_offset );

cJSON* JudgeReportVector2JSON(const vector< FeatureReport_judgeReport> &judges , acv_XY center_offset);

cJSON* acv_CircleFitVector2JSON(const vector< FeatureReport_circleReport> &vec, acv_XY center_offset);


cJSON* acv_CircleFitVector2JSON(const vector< acv_CircleFit> &vec, acv_XY center_offset);


cJSON* acv_AuxPointReport2JSON(const vector< FeatureReport_auxPointReport> &vec, acv_XY center_offset);


cJSON* acv_SearchPointReport2JSON(const vector< FeatureReport_searchPointReport> &vec, acv_XY center_offset);

cJSON* acv_LineFitVector2JSON(const vector< FeatureReport_lineReport> &vec, acv_XY center_offset);

cJSON* acv_LineFitVector2JSON(const vector< acv_LineFit> &vec, acv_XY center_offset);

cJSON* acv_Signature2JSON(const vector< acv_XY> &signature);

cJSON* acv_FeatureReport_sig360_circle_line_single2JSON(const FeatureReport_sig360_circle_line_single report );

cJSON* MatchingReport2JSON(const FeatureReport *report );

#endif