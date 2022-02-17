#pragma once
#include <stddef.h>
#include <stdint.h>
#include "GCodeParser.hpp"

#include "MSteppers.hpp"
using namespace std;


#define SUBDIV (1600)
#define mm_PER_REV 10.0


class GCodeParser_M:public GCodeParser{
public:
  MStp *_mstp;
  xVec pos_offset=(xVec){0};
  GCodeParser_M(MStp *mstp);
  bool unit_is_inch=false;
  float unit2Pulse(float dist,float pulses_per_mm);
  float unit2Pulse_axis(int axis,float dist);

  float parseFloat(char* str,int strL);

  int ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,float *retVec);

  bool isAbsLoc=true;
  int ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,xVec &retVec);
  float latestF= 1000;
  int ReadG1Data(char* line, int *blkIdxes,int blkIdxesL,xVec &vec,float &F);


  bool CheckHead(char *str1,char *str2);

  GCodeParser::GCodeParser_Status parseLine();
  void onError(int code);
};
 
