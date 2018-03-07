#ifndef VISSELEDEFINEDOCPARSER_HPP
#define VISSELEDEFINEDOCPARSER_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_BasicTool.hpp"



class VisSeleDefineDocParser {
  typedef struct featureDef_circle{
    acv_Circle circleTar;
    float initMatchingMargin;
  }featureDef_circle;
  typedef struct featureDef_line{
    acv_Circle circleTar;
    float initMatchingMargin;
  }featureDef_line;


  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  cJSON *root;
public :
  VisSeleDefineDocParser(const char *json_str);
  int reload(const char *json_str);

protected:
  int parse_circleData(cJSON * circle_obj);
  int parse_lineData(cJSON * line_obj);
  int parse_jobj();


};

#endif
