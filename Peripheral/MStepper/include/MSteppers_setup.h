#pragma once





#ifndef MSTP_VEC_SIZE
#define MSTP_VEC_SIZE 4
#endif


#define AXIS_IDX_X 0
#define AXIS_IDX_Y 1

#define AXIS_IDX_Z1 2
#define AXIS_IDX_R11 3
#define AXIS_IDX_R12 4

#define AXIS_GDX_X "X"
#define AXIS_GDX_Y "Y"

#define AXIS_GDX_Z1 "Z1_"
#define AXIS_GDX_R11 "R11_"
#define AXIS_GDX_R12 "R12_"



#define AXIS_IDX_FEEDRATE 777
#define AXIS_GDX_FEEDRATE "F"


#define AXIS_IDX_ACCELERATION 1290
#define AXIS_GDX_ACCELERATION "ACC"


#define AXIS_IDX_DEACCELERATION 1291
#define AXIS_GDX_DEACCELERATION "DEA"

struct MSTP_axis_setup{
  float mmpp;//mm per pulse
  float maxAcc;
  float minSpeed;
  float maxSpeed;
  bool dirFlip;
  bool zeroDir;
};


struct MSTP_setup{
  MSTP_axis_setup axis_setup[MSTP_VEC_SIZE];
};
