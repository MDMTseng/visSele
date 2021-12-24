#pragma once





#ifndef MSTP_VEC_SIZE
#define MSTP_VEC_SIZE 20
#endif

struct MSTP_axis_setup{
  float mmpp;//mm per pulse
  float maxAcc;
  float minSpeed;
  bool dirFlip;
  int zeroDir;


};


struct MSTP_setup{
  MSTP_axis_setup axis_setup[MSTP_VEC_SIZE];
};
