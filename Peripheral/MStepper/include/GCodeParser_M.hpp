#pragma once
#include <stddef.h>
#include <stdint.h>
#include "GCodeParser.hpp"

#include "MSteppers.hpp"
#include <ArduinoJson.h>
using namespace std;


class GCodeParser_M:public GCodeParser{
public:
  xVec pulse_offset=(xVec){0};
  GCodeParser_M(MStp *mstp);
  bool unit_is_inch=false;


  MStp *_mstp;
  virtual int MTPSYS_MachZeroRet(uint32_t axis_index,uint32_t sensor_pin,int distance,int speed,void* context);
  virtual bool MTPSYS_VecTo(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  virtual bool MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  virtual bool MTPSYS_AddWait(uint32_t period,int times, void* ctx,MSTP_segment_extra_info *exinfo);
  virtual xVec MTPSYS_getLastLocInStepperSystem();
  virtual float MTPSYS_getMinPulseSpeed();

  //PORT: PORT MASK, P: pin number, S:state 0/1 or 0~255 PWM, T: pin setup (0:input, 1:output, 2:input_pullup, 3:input_pulldown)
  virtual bool MTPSYS_AddIOState(uint32_t PORT,int32_t P, uint32_t S,int32_t T,char* CID,char* TTAG,int TID)=0;

  JsonDocument *p_jnote;
  void putJSONNote(JsonDocument* jnote){this->p_jnote=jnote;}

  float unit2Pulse(float dist,float pulses_per_mm);
  virtual float unit2Pulse_conv(int axisIdx,float dist)=0;
  virtual float Pulse2Unit_conv(int axisIdx,float pulseCount)=0;

  int ReadxVecData(char **blkIdxes,int blkIdxesL,float *retVec);

  bool isAbsLoc=true;
  int ReadxVecData(char **blkIdxes,int blkIdxesL,xVec &retVec);

  int ReadxVecELE_toPulses(char **blkIdxes,int blkIdxesL,xVec &retVec,int axisIdx,const char* axisGIDX);

  float latestF= 1000;
  int ReadG1Data(char **blkIdxes,int blkIdxesL,xVec &vec,float &F);


  GCodeParser::GCodeParser_Status parseCMD(char **blks, char blkCount);
  void onError(int code);
};
 
