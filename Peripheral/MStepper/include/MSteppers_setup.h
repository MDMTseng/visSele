#pragma once






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



#define AXIS_IDX_X 0
#define AXIS_IDX_Y 1


#define AXIS_IDX_Z1 2
#define AXIS_IDX_Z2 3
#define AXIS_IDX_Z3 4
#define AXIS_IDX_Z4 5
#define AXIS_IDX_G1_RS 6


#define AXIS_IDX_R1 7
#define AXIS_IDX_R2 8
#define AXIS_IDX_R3 9
#define AXIS_IDX_R4 10
#define AXIS_IDX_G2_RS 11


#define AXIS_IDX_Z 12
#define AXIS_IDX_MAX AXIS_IDX_Z



#ifndef MSTP_VEC_SIZE
#define MSTP_VEC_SIZE (AXIS_IDX_MAX+1)
#endif
// #define AXIS_IDX_Z1 3
// #define AXIS_IDX_R1 4

// #define AXIS_IDX_Z2 5
// #define AXIS_IDX_R2 6

// #define AXIS_IDX_Z3 7
// #define AXIS_IDX_R3 8
// #define AXIS_IDX_Z4 9
// #define AXIS_IDX_R4 10





#define AXIS_GDX_X "X"
#define AXIS_GDX_Y "Y"
#define AXIS_GDX_Z "Z"

#define AXIS_GDX_Z1 "Z1_"
#define AXIS_GDX_R1 "R1_"

#define AXIS_GDX_Z2 "Z2_"
#define AXIS_GDX_R2 "R2_"

#define AXIS_GDX_Z3 "Z3_"
#define AXIS_GDX_R3 "R3_"

#define AXIS_GDX_Z4 "Z4_"
#define AXIS_GDX_R4 "R4_"




#define AXIS_IDX_FEEDRATE 777
#define AXIS_GDX_FEEDRATE "F"


#define AXIS_IDX_FEED_ON_AXIS 1250
#define AXIS_GDX_FEED_ON_AXIS "AXF_"

#define AXIS_IDX_ACCELERATION 1290
#define AXIS_GDX_ACCELERATION "ACC"


#define AXIS_IDX_DEACCELERATION 1291
#define AXIS_GDX_DEACCELERATION "DEA"


#define EXT_IO_AXIS_SENSOR_BASE 0
#define PIN_X_SEN1 (4+EXT_IO_AXIS_SENSOR_BASE)
#define PIN_X_SEN2 (5+EXT_IO_AXIS_SENSOR_BASE)

#define PIN_Y_SEN1 (6+EXT_IO_AXIS_SENSOR_BASE)
#define PIN_Y_SEN2 (7+EXT_IO_AXIS_SENSOR_BASE)

#define PIN_Z1_SEN2 (0+EXT_IO_AXIS_SENSOR_BASE)
#define PIN_Z2_SEN2 (1+EXT_IO_AXIS_SENSOR_BASE)
#define PIN_Z3_SEN2 (2+EXT_IO_AXIS_SENSOR_BASE)
#define PIN_Z4_SEN2 (3+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_Z1_SEN2 (5+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_R1_SEN1 (6+EXT_IO_AXIS_SENSOR_BASE)


// #define PIN_Z2_SEN1 (5)
// #define PIN_Z2_SEN2 (9+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_R2_SEN1 (10+EXT_IO_AXIS_SENSOR_BASE)

// #define PIN_Z3_SEN1 (12+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_Z3_SEN2 (13+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_R3_SEN1 (14+EXT_IO_AXIS_SENSOR_BASE)


// #define PIN_Z4_SEN1 (16+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_Z4_SEN2 (17+EXT_IO_AXIS_SENSOR_BASE)
// #define PIN_R4_SEN1 (18+EXT_IO_AXIS_SENSOR_BASE)







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
