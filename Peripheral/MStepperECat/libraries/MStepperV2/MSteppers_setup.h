#pragma once


#include <stdint.h>



// #define AXIS_IDX_X 3
// #define AXIS_IDX_Y 4
// #define AXIS_IDX_Z 10

// #define AXIS_IDX_Z1 5
// #define AXIS_IDX_R1 6

// #define AXIS_IDX_Z2 1
// #define AXIS_IDX_R2 2
// #define AXIS_IDX_Z3 0
// #define AXIS_IDX_R3 7
// #define AXIS_IDX_Z4 8
// #define AXIS_IDX_R4 9


#define AXIS_IDX_MAX 13



#ifndef MSTP_VEC_SIZE
#define MSTP_VEC_SIZE (AXIS_IDX_MAX+1)



#define AXIS_IDX_FEEDRATE 777
#define AXIS_GDX_FEEDRATE "F"


#define AXIS_IDX_FEED_ON_AXIS 1250
#define AXIS_GDX_FEED_ON_AXIS "AXF_"

#define AXIS_IDX_ACCELERATION 1290
#define AXIS_GDX_ACCELERATION "ACC"


#define AXIS_IDX_DEACCELERATION 1291
#define AXIS_GDX_DEACCELERATION "DEA"



#endif


