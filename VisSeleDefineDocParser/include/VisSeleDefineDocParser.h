#ifndef VISSELEDEFINEDOCPARSER_HPP
#define VISSELEDEFINEDOCPARSER_HPP
using namespace std;
#include <vector>
#include <cstdlib>
#include <ctime>
#include "cJSON.h"
#include "acvImage_BasicTool.hpp"


class visSeleDefineDocParser {

  vector<acv_CircleFit> circleList;
  vector<acv_LineFit> detectedLines;
  cJSON *root;
public :
  visSeleDefineDocParser(const char *json_str);

};

#endif
