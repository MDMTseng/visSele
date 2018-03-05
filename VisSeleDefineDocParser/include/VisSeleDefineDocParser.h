#ifndef VISSELEDEFINEDOCPARSER_HPP
#define VISSELEDEFINEDOCPARSER_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_BasicTool.hpp"



class visSeleDefineDocParser {
  class featureDef_circle{
    acv_Circle circleTar;
    float initMatchingMargin;
  };
  class featureDef_line{
    acv_Circle circleTar;
    float initMatchingMargin;
  };


  vector<featureDef_circle> featureCircleList;
  vector<featureDef_line> featureLineList;
  cJSON *root;
public :
  visSeleDefineDocParser(const char *json_str);
  int reload(const char *json_str);

protected:
  int parse_jobj();


};

#endif
