#pragma once
#include <stddef.h>
#include <stdint.h>
#include "GCodeParser.hpp"

#include "MSteppers.hpp"
using namespace std;


class GCodeParser_M:public GCodeParser{
public:
  xVec pos_offset=(xVec){0};
  GCodeParser_M(MStp *mstp);
  bool unit_is_inch=false;


  MStp *_mstp;
  virtual int MTPSYS_MachZeroRet(uint32_t index,int distance,int speed,void* context);
  virtual bool MTPSYS_VecTo(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  virtual bool MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  virtual bool MTPSYS_AddWait(uint32_t period,int times, void* ctx,MSTP_segment_extra_info *exinfo);
  virtual xVec MTPSYS_getLastLocInStepperSystem();
  virtual float MTPSYS_getMinPulseSpeed();


  float unit2Pulse(float dist,float pulses_per_mm);
  virtual float unit2Pulse_conv(const char* code,float dist);

  int FindFloat(char *prefix,char **blkIdxes,int blkIdxesL,float &retNum);
  int FindInt32(char *prefix,char **blkIdxes,int blkIdxesL,int32_t &retNum);
  int FindGMEnd_idx(char **blkIdxes,int blkIdxesL);
  
  int ReadxVecData(char **blkIdxes,int blkIdxesL,float *retVec);

  bool isAbsLoc=true;
  int ReadxVecData(char **blkIdxes,int blkIdxesL,xVec &retVec);
  float latestF= 1000;
  int ReadG1Data(char **blkIdxes,int blkIdxesL,xVec &vec,float &F);


  bool CheckHead(char *str1,char *str2);

  GCodeParser::GCodeParser_Status parseLine();
  void onError(int code);
};
 
