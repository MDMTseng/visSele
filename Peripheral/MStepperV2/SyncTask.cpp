
#include "main.hpp"
#include <string.h>
#include "MSteppersV2.hpp"
#include "GCodeParser.hpp"
#include "GCodeParserUtil.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include <Data_Layer_Protocol.hpp>
#include "driver/timer.h"
#include "tinyexpr.h"
#include <array>


extern "C" {
#include "direct_spi.h"
}

#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)


#define BIT_CUT(V32,offset,width)  (   ( (V32)>>(offset) )     &    ((1<<(width))-1)    )

#define BIT_CUT2(V32,LBitPosition,HBitPosition)  (   ( (V32)>>(LBitPosition) )     &    ((1<<(HBitPosition-LBitPosition+1))-1)    )
#define ENDIAN_SWITCH(B32)  (((B32)<<24)|(((B32)&0xFF00)<<8)|(((B32)&0xFF0000)>>8)|((B32)>>24))




void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc);
bool doDataLog=false;
class MData_JR:public Data_JsonRaw_Layer
{
  
  public:
  MData_JR():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {
    doDataLog=false;
    return 0;
  } 
  int recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data=NULL,size_t dataL=0);
  
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  void connected(Data_Layer_IF* ch){}

  int send_data(int head_room,uint8_t *data,int len,int leg_room);
  void disconnected(Data_Layer_IF* ch){}

  int close(){return 0;}

  
  char dbgBuff[500];
  int dbg_printf(const char *fmt, ...);

  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


  GCodeParser::GCodeParser_Status insertCMD(StpGroup *stpG,JsonDocument &cmd);

};
MData_JR djrl;

// struct STPG_CMD_INFO{
//   int cmd_id;
//   int groupIndex;
//   char cmd[100];
// };
// RingBuf_Static <STPG_CMD_INFO,10> STPG_CMD_INFO_Buffer;

int HACK_cur_cmd_id=-1;

void G_LOG(const char* str)
{
  djrl.dbg_printf(str);
}


hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))




//#define _HOMING_DBG_FLAG_ 50

int PIN_LED=2;
int pin_SH_165=17;
int pin_TRIG_595=5;

int PIN_DBG0=32;
int PIN_DBG1=33;


#define GPIOLS32_SET(PIN) GPIO.out_w1ts=1<<(PIN);
#define GPIOLS32_CLR(PIN) GPIO.out_w1tc=1<<(PIN);
  

#define SUBDIV (3200)
#define mm_PER_REV 95

spi_device_handle_t spi1=NULL;

enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,


  
  HALT_UNTIL_TRACKING_SATTLED=14,
  ASSERT_TRACKING_SATTLED=15,

  TRACKING_PROGRESS=50,

  GENERIC=999,
};


enum Mstp2CommInfo_Type{
  trigInfo=1000,
  ext_log=1001,
  respFrame=1002,
};



struct Mstp2CommInfo{//TODO: rename the infoQ to be more versatile
  Mstp2CommInfo_Type type;

  //trigInfo
  char strinfo[40];
  int trig_id;
  float curFreq;
  int curReelLocation;

  //log
  // char log[40];

  //respFrame
  bool isAck;
  int resp_id;
};

RingBuf_Static<struct Mstp2CommInfo,20,uint8_t> Mstp2CommInfoQ;



struct MSTP_SegCtx_IOCTRL{
  uint32_t PORT=0,S=0;
  int32_t P=0,T=0;
};


struct MSTP_SegCtx_INPUTMON{
  uint32_t PINS,PIN_NS;
  uint32_t existField;
  bool doMonitor;
};


struct MSTP_SegCtx_OnTimeReply{
  int id;
  bool isAck;
};


struct MSTP_SegCtx_TrackingInfo{
  float *p_ctrl_progress;
};

struct MSTP_SegCtx_Generic{
  void* obj;
};



struct MSTP_SegCtx_TrackingHoldUntil{
  int id;
};



struct MSTP_SegCtx_TargetTrackingTransition{
  int tar_ENC;
  uint32_t axis_vec;
};

struct MSTP_SegCtx{
  MSTP_SegCtx(){}
  ~MSTP_SegCtx(){}

  bool isProcessed;
  MSTP_SegCtx_TYPE type;
  union {
    struct MSTP_SegCtx_IOCTRL IO_CTRL;
    struct MSTP_SegCtx_INPUTMON INPUT_MON;
    struct MSTP_SegCtx_OnTimeReply ON_TIME_REP;
    struct MSTP_SegCtx_TrackingInfo TRACKING_INFO;
    struct MSTP_SegCtx_Generic Generic;
    struct MSTP_SegCtx_TrackingHoldUntil TRACKING_HOLD_UNTIL;
  };
  // char TTAG[40];
  // int TID;
};


const int SegCtxSize=10;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);



class GCodeParser_M:public GCodeParser{
public:
  xVec pulse_offset=(xVec){0};
  GCodeParser_M()
  {

  }
  
  JsonDocument *p_jnote;
  void putJSONNote(JsonDocument* jnote){this->p_jnote=jnote;}
  StpGroup* p_stpG;
  void putTargetStepperGroup(StpGroup* stpG)
  {
    this->p_stpG=stpG;
  }


  // inline void SetxVec_fToxVec(xVec &vec_dst,xVec_f &vec_src)
  // {
  //   for(int i=0;i<MSTP_VEC_SIZE;i++)
  //   {
  //     if(vec_src.vec[i]!=NAN)
  //       vec_dst.vec[i]=(int32_t)(vec_src.vec[i]);
  //   }
  // }

  GCodeParser::GCodeParser_Status parseCMD(char **blks, char blkCount)
  {

    
    if(blkCount==0)
      return GCodeParser::GCodeParser_Status::LINE_EMPTY;
    GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;



    if(p_stpG)
    {
      auto &stpG=*p_stpG;
      auto ret=stpG.GcodeParse(blks,blkCount,*p_jnote);

      return (GCodeParser::GCodeParser_Status)ret;
    }




    char *cblk=blks[0];
    // int cblkL=blks[1]-blks[0];

    blks++;//move to next block
    blkCount--;


    if(cblk[0]=='G' && p_stpG!=NULL)
    {
      
    }
    else if(cblk[0]=='M')
    {

    }
    else if(cblk[0]!=';' &&cblk[0]!='('  )
    {
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }


    return retStatus;
    // return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }
  void onError(int code)
  {
    
  }
};
 



#define _TICK2SEC_BASE_ (10*1000*1000)

extern void __digitalWrite(uint8_t pin, uint8_t val)
{
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
}


void dbg_msg_sendQ(std::string msg)
{

  struct Mstp2CommInfo tinfo={
  .type=Mstp2CommInfo_Type::ext_log,
  };

  strncpy(tinfo.strinfo,msg.c_str(),sizeof(tinfo.strinfo)-1);

  Mstp2CommInfo* Qhead=NULL;
  while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
  *Qhead=tinfo;
  Mstp2CommInfoQ.pushHead();
}


uint64_t SystemTick=0;
inline uint64_t getCurTick()
{
  return SystemTick+timerRead(timer);
}



static SemaphoreHandle_t SeqCalcTaskLock;

class PulseGenerator_M :public PulseGenerator
{
public:
  PulseGenerator_M( ):PulseGenerator()
  {
  }



  int ttt=0;
  void bufferEmpty()
  {
    BaseType_t xHigherPriorityTaskWoken= pdFALSE;
    xSemaphoreGiveFromISR(SeqCalcTaskLock,&xHigherPriorityTaskWoken);

    
    digitalWrite(PIN_DBG0, ttt);
    ttt=!ttt;
  }
  void timerNext(uint32_t period_us)
  {
    uint64_t T=period_us*10;
    timer_set_alarm_value(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, (uint64_t)T);
  }


  uint32_t static_Pin_info0=0;
  uint32_t static_Pin_info1=0;

  uint32_t _latest_stp_pins=0;//info in the register
  uint32_t _latest_dir_pins=0;
  uint32_t latest_stp_pins=0;//info that really on pins
  uint32_t latest_dir_pins=0;

  uint32_t latest_input_pins=0;
  uint32_t pre_f_step=0;
  uint32_t pre_f_dir=0;


  uint32_t g_step_trigger_edge=0;//0xFFFFFFF;//each bits means trigger edge setting on each axis, 0 for posedge 1 for negedge
  uint32_t g_dir_inv=0;//^(1<<AXIS_IDX_Z1)^(1<<AXIS_IDX_Z2)^(1<<AXIS_IDX_Z3)^(1<<AXIS_IDX_Z4);
  void pinInfoSet(uint32_t stp,uint32_t dir)
  {
    _latest_stp_pins=stp;
    _latest_dir_pins=dir;



    // uint32_t Seg1=V_CUT(dir,0,3)<<8|V_CUT(step,0,3)<<8;


    // uint32_t Seg2=V_CUT(dir,0,3)<<8|V_CUT(step,0,3)<<8;
    uint32_t m_dir=dir^g_dir_inv;
    uint32_t m_step=stp^g_step_trigger_edge;
    pre_f_dir=m_dir;
    pre_f_step=m_step;
    

    uint32_t portPins=(BIT_CUT2(m_step,0,4)<<4)|(BIT_CUT2(m_dir,0,4)<<9);


    int groupCount=3;
    int groupIdx=groupCount-1;
    spi1->host->hw->data_buf[groupIdx--]=ENDIAN_SWITCH(portPins^static_Pin_info0);
    spi1->host->hw->data_buf[groupIdx--]=ENDIAN_SWITCH(static_Pin_info1);
    
    GPIOLS32_SET(pin_SH_165);//switch to keep in 165 register(stop 165 load pin to reg)
    GPIOLS32_CLR(pin_TRIG_595);//
    direct_spi_transfer(spi1,32*(groupCount));

    pre_f_step=stp;
    pre_f_dir=dir;
  }
  void pinUpdate()
  {

    while (direct_spi_in_use(spi1));//wait for SPI bus available
    GPIOLS32_CLR(pin_SH_165);//switch to load(165 keeps load pin to internal reg)

    latest_input_pins=spi1->host->hw->data_buf[0];
    GPIOLS32_SET(pin_TRIG_595);//trigger 595 internal register update to 595 phy pin
    // digitalWrite(M0_STP, pre_f_step&(1<<0));
    // digitalWrite(M0_DIR, pre_f_dir&(1<<0));

    // digitalWrite(M1_STP, pre_f_step&(1<<1));
    // digitalWrite(M1_DIR, pre_f_dir&(1<<1));

    // digitalWrite(M2_STP, pre_f_step&(1<<2));
    // digitalWrite(M2_DIR, pre_f_dir&(1<<2));

    // digitalWrite(M3_STP, pre_f_step&(1<<3));
    // digitalWrite(M3_DIR, pre_f_dir&(1<<3));

    latest_stp_pins=_latest_stp_pins;
    latest_dir_pins=_latest_dir_pins;
  }
};

PulseGenerator_M PG_M;

MStpV2 mstpV2(&PG_M);

void IRAM_ATTR onTimer()
{
  // static uint32_t cp0_regs[18];
  // enable FPU
  // xthal_set_cpenable(1);
  // // Save FPU registers
  // xthal_save_cp0(cp0_regs);
  // // uint32_t nextT=100;
  // // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);

  PG_M.nextStep();


  // Restore FPU
  // xthal_restore_cp0(cp0_regs);
  // // and turn it back off
  // xthal_set_cpenable(0);
  // 


}



StaticJsonDocument <1024>doc;
StaticJsonDocument  <500>retdoc;


static int CB_Count=0;


class StpGroup_PLATE:public StpGroup
{ 
  typedef  xnVec_f<1> RXVec;

  typedef  xnVec_f<1> CMDVec;//X Y Z
  typedef  xnVec_f<1> MotVec;//R Z X
  typedef  xnVec<1> MotVec_i;

  // std::array<struct MSTP_segment,1> segsBuffer;


  // MSTP_axisSetup _axisSetup[1];

  CMDVec latestAdvLocation={{0}};

  MotVec_i preMotloc_pls={0};

  std::array<struct MSTP_segment,10> segsBuffer;
  
public:
  float cx,cy;
  StpGroup_PLATE():StpGroup()
  {
    
    segs.RESET(segsBuffer.data(),segsBuffer.size());

    // memset(outputHist.buff,0,outputHist.capacity()*sizeof(RXVec));
    

    // axisSetup=_axisSetup;

    // segs.RESET(segsBuffer.data(),segsBuffer.size());


  }

  float* getLatestLocation()
  {
    return latestAdvLocation.vec;
  }

  float tar_speed=0*3.14159/180;//5 deg/s
  float acc=10*3.14159/180;//5 deg/s
  float cur_speed=0;//5 deg/s
  void update()//every system tick, update the location
  {
    if(cur_speed<tar_speed)
    {
      cur_speed+=acc*mstpV2.updatePeriod_s;
      if(cur_speed>tar_speed)cur_speed=tar_speed;
    }
    if(cur_speed>tar_speed){
      cur_speed-=acc*mstpV2.updatePeriod_s;
      if(cur_speed<tar_speed)cur_speed=tar_speed;
    }
    latestAdvLocation.vec[0]+=cur_speed*mstpV2.updatePeriod_s;//in rad
  }



  void copyCMDVec(float*dst,float*src)
  {
    memcpy(dst,src,sizeof(CMDVec));
  }

  float reduction=60;
  void motUnit2PulseConversion(int32_t *pulse_dst,float *unit_src)
  {
    pulse_dst[0]=(int32_t)(unit_src[0]*1600*reduction/(2*3.14159));
  }

  void motPulse2UnitConversion(float *unit_dst,int32_t *pulse_src)
  {
    unit_dst[0]=((float)pulse_src[0]*2*3.14159/1600/reduction);//R  steps to arcDeg angle
  }


  int turnCount=0;
  virtual void getMotMoveVec(xVec_f *mot_vec_dst)
  { 

    CMDVec curMotLoc_unit={0};
    inverse(curMotLoc_unit.vec,latestAdvLocation.vec);

    MotVec_i _curMotLoc_pls={0};
    // _curMotLoc_pls.vec[0]=(int32_t)(curMotLoc_unit.vec[0]*1600*4.5/(73*2*3.14159));//R  arc angle to steps

    motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);



    mot_vec_dst->vec[3]=-(_curMotLoc_pls.vec[0]-preMotloc_pls.vec[0]);//invert polarity



    bool recalc=false;
    if(latestAdvLocation.vec[0]>2*3.14159)//warp around
    {
      latestAdvLocation.vec[0]-=2*3.14159;
      turnCount++;
      recalc=true;
    }
    else if(latestAdvLocation.vec[0]<0)//warp around
    {
      latestAdvLocation.vec[0]+=2*3.14159;
      turnCount--;
      recalc=true;
    }
    if(recalc)
    {
      inverse(curMotLoc_unit.vec,latestAdvLocation.vec);
      motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);
    }

    preMotloc_pls=_curMotLoc_pls;

  }

  void inverse(float *mot_vec_dst,const float* loc_vec_src){
    mot_vec_dst[0]=loc_vec_src[0];
  }
  void forward(float* loc_vec_dst,const float *mot_vec_src)
  {
    loc_vec_dst[0]=mot_vec_src[0];
  }


  int GcodeParse(char **blkIdxes,int blkIdxesL,JsonDocument& cmd_ret)
  {
    return  GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }


  int JCMDParse(JsonDocument& cmd,JsonDocument& cmd_ret)
  {

    JsonObject cmdData=cmd["cmd"];
    if(cmdData.isNull())
    {
      return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
    }

    if(!cmdData["type"].is<const char*>())
    {
      return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
    }
    const char* type = cmdData["type"];


    if(strcmp(type,"cur_angle")==0)
    {
      cmd_ret["angle"]=latestAdvLocation.vec[0];
      return GCodeParser::GCodeParser_Status::TASK_OK;
    }

    if(strcmp(type,"set_speed")==0)
    {
      if(!cmdData["speed"].is<float>())
      {
        return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
      }
      tar_speed=cmdData["speed"];
      return GCodeParser::GCodeParser_Status::TASK_OK;
    }


    if(strcmp(type,"set_centre")==0)
    {
      if(!cmdData["x"].is<float>()||!cmdData["y"].is<float>())
      {
        return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
      }
      cx=cmdData["x"];
      cy=cmdData["y"];
      return GCodeParser::GCodeParser_Status::TASK_OK;
    }



    if(strcmp(type,"get_centre")==0)
    {
      cmd_ret["cx"]=cx;
      cmd_ret["cy"]=cy;
      return GCodeParser::GCodeParser_Status::TASK_OK;
    }

    cmd_ret["type"]=type;

    
    return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;


  }
};

StpGroup_PLATE SG_PLATE;

// Function to solve quadratic equation ax^2 + bx + c = 0
int solveQuadratic(float a, float b, float c, float *x1, float *x2) {
    float discriminant = b*b - 4*a*c;
    if (discriminant < 0) {
        return 0; // No real roots
    } else if (discriminant == 0) {
        *x2 =*x1 = -b / (2*a);
        // *x2 = *x1;
        return 1; // One real root
    } else {
        *x1 = (-b + sqrt(discriminant)) / (2*a);
        *x2 = (-b - sqrt(discriminant)) / (2*a);
        return 2; // Two real roots
    }
}


int solveQuadratic_real(float a, float b, float c, float *x1, float *x2) {
    float discriminant = b*b - 4*a*c;
    if (discriminant <= 0) {
        *x2 =*x1 = -b / (2*a);
        // *x2 = *x1;
        return 1; // One real root
    } else {
        *x1 = (-b + sqrt(discriminant)) / (2*a);
        *x2 = (-b - sqrt(discriminant)) / (2*a);
        return 2; // Two real roots
    }
}

/**
 * Finds and prints the intersection points between a line and a circle.
 * The line is represented by a direction vector (vx, vy) and passes through the origin.
 * The circle is defined by its center (h, k) and radius r.
 * This function first calculates the coefficients for the quadratic equation
 * derived from substituting the parametric line equation into the circle's equation.
 * It then solves the quadratic equation to find the values of the parameter t
 * that correspond to the intersection points. Using these t values,
 * it calculates and prints the coordinates of the intersection points.
 * 
 * @param vx Direction vector x-component of the line.
 * @param vy Direction vector y-component of the line.
 * @param cx  x-coordinate of the circle's center.
 * @param cy  y-coordinate of the circle's center.
 * @param r  Radius of the circle.
 * @param *t1 Pointer to the first intersection point parameter.
 * @param *t2 Pointer to the second intersection point parameter.
 * @return   Number of intersection points.
 * 
 */

// Function to find intersection points
int findIntersection(float vx, float vy, float cx, float cy, float r, float *t1,float *t2) {
    *t1=*t2=NAN;

    float a = vx*vx + vy*vy;
    float b = -2*(cx*vx + cy*vy);
    float c = cx*cx + cy*cy - r*r;
    return solveQuadratic_real(a, b, c, t1, t2);
}




float get_var(void *context,float idx) {
  if(idx<0)return NAN;
  return *( ((float*)context) + (int)idx );
}

float set_var(void *context,float idx,float var) {
  if(idx<0)return NAN;
  return *( ((float*)context) + (int)idx )=var;
}




void PlateObjLocParamExtract(float*params,float ox,float oy,float oz,float plateAngle)
{

  float dx=ox-SG_PLATE.cx;
  float dy=oy-SG_PLATE.cy;

  float r=sqrt(dx*dx+dy*dy);

  float offset_angle=atan2(dy,dx)-plateAngle;

  params[0]=offset_angle;
  params[1]=r;
  params[2]=oz;
}

void PlateObjLocGet(float* retLoc, float*params)
{
  float offset_angle=params[0];
  float r=params[1];
  float oz=params[2];
  
  float curPlate_angle=SG_PLATE.getLatestLocation()[0];
  // retLoc[0]=r*cos(theda);
  // retLoc[1]=r*sin(theda);
  retLoc[0]=r*cosf(offset_angle+curPlate_angle)+SG_PLATE.cx;
  retLoc[1]=r*sinf(offset_angle+curPlate_angle)+SG_PLATE.cy;
  retLoc[2]=oz;
}


class StpGroup_SIMP:public StpGroup
{ 
  typedef  xnVec_f<3> CMDVec;//X Y Z
  typedef  xnVec_f<3> MotVec;//R Z X
  typedef  xnVec<3> MotVec_i;

  // static const int OUTPUT_HIST_DIV=1;
  // RingBuf_Static<RXVec,1024/OUTPUT_HIST_DIV> outputHist;

  std::array<CMDVec,40> startPtBuffer;
  std::array<CMDVec,40> vecBuffer;
  std::array<CMDVec,40> aux_pt2;
  std::array<CMDVec,40> aux_pt3;
  std::array<CMDVec,40> aux_pt4;

  std::array<struct MSTP_segment,40> segsBuffer;



  const int CMD_VEC_DIM=sizeof(CMDVec)/sizeof(float);


  //XYZ
  CMDVec latestCMDLocation={{0}};//updated by the latest command
  CMDVec latestAdvLocation={{0}};//updated by update(), every update, the segAdvance will give a new progress/location for this tick  
  CMDVec latestAdvLocation_postTracking={{0}};
  // @....o....o....o....o....o....o....@....o....o....o....o....o....o....@
  // A                                  B                                  C
  // latestCMDLocation=A                 =B                                  =C  //A,B C are the location from user command G01 for example
  //AdvLocation is every "o" 




  //RZX
  //the function getMotMoveVec needs move vector to feed to pulse generator
  //Threrefore, we need a preMotloc to substrace to get motor move diff


  //curMotLoc_unit is a unit based number, could be mm or rad 
  //inverse() and forward() are unit based conversion function

  //curMotLoc_unit is the current location of the motor, should be produced by inverse() from latestAdvLocation()
  //curMotLoc_unit is also used by homing process. Every motor do homing independently,
  //so homing precess will touch the curMotLoc_unit directly and bypass the inverse() function
  //(since the motor location is uncertain, the conversion is not valid)
  MotVec curMotLoc_unit={0};


  //preMotloc_pls is a pulse based discrete number, used to prevent float error accumulation
  MotVec_i preMotloc_pls={0};
  //BTW, when curMotLoc_unit[n]==0, preMotloc_pls[n] should be 0 too, cannot have offset be involved
  





/*

User cmd0 -> latestCMDLocation=cmd0, push to segsBuffer
User cmd1 -> latestCMDLocation=cmd1, push to segsBuffer


system tick -> update() -> latestAdvLocation=segAdvance() -> curMotLoc_unit=inverse(latestAdvLocation) 
            -> getMotMoveVec-> _curMotLoc_pls=convert(curMotLoc_unit )->  pulseGenerator.feed(_curMotLoc_pls-preMotloc_pls) -> preMotloc_pls=_curMotLoc_pls



*/



  //add homing state 
  enum HomingStatus{
    HNOP=0,
    Start,
    Back,
    Forward,
    FineBack,
    End
  };


  bool homingFlag=false;
  int homingCMDID=-1;
  struct HomingSeq{
    HomingStatus status;
    int axisIdx;
    float speed;
    float speed_fine;
    int sensorPin;

    int sensorSense;
  };

  std::vector<HomingSeq> homingSeq;
  // HomingSeq homingSeq[sizeof(RXVec)/sizeof(float)]={{HNOP,0,0,0,0,0}};

  bool DBG_allowSkipHoming=false;
  MSTP_axisSetup _axisSetup[sizeof(CMDVec)/sizeof(float)];




  struct CTX_TrackingInfo{
    CMDVec Pr;
    void (*PtF)(float* retLoc, float*params);
    float param[5];
    float acc,deacc,vcen;
    StpGroup_SIMP *self;
  };

  std::array<ResourcePool<CTX_TrackingInfo>::ResourceData,10> trackingInfoCTXBuffer;


  struct TrackingInfo{

    //there are two sets of tracking targets

    //P=Pc+(Pt0-Pr0)*(1-progress)+(Pt1-Pr1)*progress
    //Pt0=PtF0(param0), Pt1=PtF1(param1)
    //if PtF0==NULL Pt0=Pr0 vice versa


    MSTP_segment_adv_info advInfo;
    MSTP_segment movParam;

    //-1 means not in tracking transition mode, a new transition schedule can be set, and the real progress is 0
    float progress;


    CMDVec Pr0;
    void (*PtF0)(float* retLoc, float*params);
    float param0[5];



    CMDVec Pr1;
    void (*PtF1)(float* retLoc, float*params);
    float param1[5];



  };
public:

  ResourcePool <CTX_TrackingInfo>trackingInfoCTX_pool;

  TrackingInfo trackingInfo;



  StpGroup_SIMP():StpGroup(),trackingInfoCTX_pool(trackingInfoCTXBuffer.data(),trackingInfoCTXBuffer.size())
  {

    // memset(outputHist.buff,0,outputHist.capacity()*sizeof(RXVec));


    axisSetup=_axisSetup;
    for(int i=0;i<segsBuffer.size();i++)//setup buffer
    {
      segsBuffer[i].vec=(vecBuffer[i].vec);
      segsBuffer[i].sp=(startPtBuffer[i].vec);

      segsBuffer[i].aux_pt2=(aux_pt2[i].vec);
      segsBuffer[i].aux_pt3=(aux_pt3[i].vec);
      segsBuffer[i].aux_pt4=(aux_pt4[i].vec);


    }


    segs.RESET(segsBuffer.data(),segsBuffer.size());

    axisSetup[0].A_Factor=1;
    axisSetup[0].V_Factor=1;
    axisSetup[0].MaxVJump=1;
    axisSetup[0].V_Max=2000;
    axisSetup[0].A_Max=100000;



    axisSetup[1]=axisSetup[2]=
    axisSetup[0];






    adv_info.minSpeed=5;


    if(DBG_allowSkipHoming)
    {
      {//set homing actual location
        curMotLoc_unit.vec[0]=ArmHomingAngle;//R
        curMotLoc_unit.vec[1]=20;//Z
        curMotLoc_unit.vec[2]=-20;//X
        motUnit2PulseConversion(preMotloc_pls.vec,curMotLoc_unit.vec);
      }

      forward(latestAdvLocation.vec,curMotLoc_unit.vec);
      latestCMDLocation=latestAdvLocation;

      homingFlag=true;

    }


    trackingInfo.progress=-1;
    trackingInfo.PtF0=NULL;
    trackingInfo.PtF1=NULL;

    trackingInfo.advInfo.minSpeed=5;
    trackingInfo.advInfo.magicSpace=0.1;

    // design_lowpass_filter(10,1000);
  }

  // void print(const char* str)
  // {
  //   djrl.dbg_printf(str);
  // }
  float* getLatestLocation()
  {
    return latestCMDLocation.vec;
  }

  uint32_t preCount=0;
  uint32_t updateCount=0;




  float XVecAngle=46*3.14159/180;
  float ArmHomingAngle=(120)*3.14159/180;
  float XVecX=cosf(XVecAngle), XVecY=sinf(XVecAngle),Arm_r=178;







  void homingACT()
  {



    static int ccc=0;
    HomingSeq &hP=homingSeq[0];
    float T=mstpV2.updatePeriod_s;
    switch (hP.status) 
    {
    case Start:
      hP.status=Back;
      break;
    case Back:
        // if(ccc--<0)
        if(((PG_M.latest_input_pins>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=Forward;
          ccc=0;
        }
        else
        {
          curMotLoc_unit.vec[hP.axisIdx]+=hP.speed*T;
        }
      break;

    case Forward:
        // if(ccc--<0)
        if(((PG_M.latest_input_pins>>hP.sensorPin)&1)!=hP.sensorSense)
        {
          if(ccc++<200)//overshoot slowly
          {
            curMotLoc_unit.vec[hP.axisIdx]-=hP.speed_fine*T;
          }
          else
          {
            hP.status=FineBack;
          }
        }
        else
        {
          curMotLoc_unit.vec[hP.axisIdx]-=hP.speed*T;
        }
      break;

    case FineBack:
        // if(ccc--<0)
        if(((PG_M.latest_input_pins>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=End;
        }
        else
        {
          curMotLoc_unit.vec[hP.axisIdx]+=hP.speed_fine*T;
        }
      break;


    case HNOP:
    case End:


      preMotloc_pls.vec[hP.axisIdx]=0;
      curMotLoc_unit.vec[hP.axisIdx]=0;

      homingSeq.erase(homingSeq.begin());//remove the first element
      break;
    }


  }

  struct DBGInfo{
    int PtF0_count;
    int PtF1_count;

    int progress_count;
    int progress_reach_count;

    int advSt;
    float distStart;
    float distWent;
    float distEnd;

    CMDVec loc;
    CMDVec loc2;


  };
  DBGInfo dbgInfo={0};
  void update()//every system tick, update the location
  {
    updateCount=(updateCount+1)%100000;

    // CTX_TrackingInfo *ti=TrackingInfoQ.getTail();
    do{
      if(homingCMDID!=-1)
      {
        homingACT();
        if(homingSeq.size()==0)
        {

          {//set homing preset location
            curMotLoc_unit.vec[0]=ArmHomingAngle;//R
            curMotLoc_unit.vec[1]=20;//Z
            curMotLoc_unit.vec[2]=-20;//X
            motUnit2PulseConversion(preMotloc_pls.vec,curMotLoc_unit.vec);
          }

          forward(latestAdvLocation.vec,curMotLoc_unit.vec);
          latestCMDLocation=latestAdvLocation;




          struct Mstp2CommInfo tinfo={
          .type=Mstp2CommInfo_Type::respFrame,
          .isAck=true,
          .resp_id=homingCMDID
          };
          sprintf(tinfo.strinfo,"%.3f %.3f %.3f",latestAdvLocation.vec[0],latestAdvLocation.vec[1],latestAdvLocation.vec[2]);
          Mstp2CommInfo* Qhead=NULL;
          while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
          *Qhead=tinfo;
          Mstp2CommInfoQ.pushHead();
          homingCMDID=-1;
          homingFlag=true;
        }

        break;
      }
      
      if(homingFlag==false)break;//
      
      










      float T=0;
      float timeLeft=mstpV2.updatePeriod_s;

      while(T!=timeLeft)//keep update until the time is used up
      {
        timeLeft-=T;
        T=timeLeft;
        MSTP_segment*curSeg= segAdvance(T);//The T will be updated to the time that used


        // if(ti!=NULL)
        // {
        //   if(ti->targetSeg==curSeg)//could be NULL match as well
        //   {
        //     //load the tracking info
        //     loadNextTracking(ti);
        //     TrackingInfoQ.consumeTail();//tail remove
        //     ti=TrackingInfoQ.getTail();//load next
        //   }
        // }
        if(curSeg==NULL)
        {
          // adv_info.dstanceWent=0;
          break;
        }

        if(curSeg->type==MSTP_segment_type::seg_wait)
        {
          if(curSeg->ctx!=NULL)
          {
            MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
            if(p_res->type==MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED )
            {
              if(trackingInfo.progress==-1)//sattled
              {
                adv_info.dstanceWent=curSeg->distanceEnd=-1;//this will end the seg_wait, next update will get a new seg
              }
            }
          }

          break;

        }
        else if(curSeg->type==MSTP_segment_type::seg_line)//linear interpolation
        {
          float ratio=adv_info.dstanceWent/(curSeg->Edistance);
          for(int i=0;i<CMD_VEC_DIM;i++)
          {
            latestAdvLocation.vec[i]=curSeg->vec[i]*(ratio)+curSeg->sp[i];
          }


        }
        else  if(curSeg->type==MSTP_segment_type::seg_arc)//cubic bezier interpolation
        {
          cubicBezier_comp(latestAdvLocation.vec,CMD_VEC_DIM,curSeg,adv_info.dstanceWent);

        }


      }

    }while(0);


    if(homingFlag==false)return;//




    latestAdvLocation_postTracking=latestAdvLocation;


    {//tracking process
      //P=Pc+(Pt0-Pr0)*(1-progress)+(Pt1-Pr1)*progress
      //progress goes friom 0 to 1
      //when progress=0, P=Pc+(Pt0-Pr0)
      //when progress goes up
      //when progress=1, P=Pc+(Pt1-Pr1) 
      // AND, set progress=0, Pt0=Pt1 Pr0=Pr1
      // in practice progress=-1, meaning no tracking in progress, a new tracking schedule can be set by CMD

    
      if(trackingInfo.progress>=0)//in tracking progress
      {//advancing the progress

        float T = mstpV2.updatePeriod_s;

        dbgInfo.progress_count++;

        StpGroup::MSTP_segment_adv_state  st=segAdvance(T,&trackingInfo.movParam,&trackingInfo.advInfo);

        dbgInfo.advSt=st;
        float progress=trackingInfo.advInfo.dstanceWent/trackingInfo.movParam.distanceEnd;

        dbgInfo.distEnd=trackingInfo.movParam.distanceEnd;
        dbgInfo.distWent=trackingInfo.advInfo.dstanceWent;
        dbgInfo.distStart=trackingInfo.movParam.distanceStart;




        if(progress>=1)//progress reaches the end
        {

          dbgInfo.progress_reach_count++;
          progress=1;

          //move target 1 to target 0, and set target 1 to NULL
          trackingInfo.Pr0=trackingInfo.Pr1;
          trackingInfo.PtF0=trackingInfo.PtF1;
          memcpy(trackingInfo.param0,trackingInfo.param1,sizeof(trackingInfo.param0));
          trackingInfo.PtF1=NULL;

          progress=-1;
        }
        trackingInfo.progress=progress;
      }


      float trackingProgress=trackingInfo.progress;
      if(trackingProgress<0)
      {
        trackingProgress=0;
      }

      float efDown=1;
      if(trackingInfo.PtF0!=NULL)
      {
        dbgInfo.PtF0_count++;
        CMDVec tarLoc0=latestAdvLocation_postTracking;
        trackingInfo.PtF0(tarLoc0.vec,trackingInfo.param0);

        for(int i=0;i<CMD_VEC_DIM;i++)
        {
          float diff=(tarLoc0.vec[i]-trackingInfo.Pr0.vec[i]);
          latestAdvLocation_postTracking.vec[i]+=(1-trackingProgress)*diff*efDown;

        }
      }
      if(trackingInfo.PtF1!=NULL)
      {
        dbgInfo.PtF1_count++;
        CMDVec tarLoc1;
        trackingInfo.PtF1(tarLoc1.vec,trackingInfo.param1);

        for(int i=0;i<CMD_VEC_DIM;i++)
        {

          float diff=(tarLoc1.vec[i]-trackingInfo.Pr1.vec[i]);
          dbgInfo.loc.vec[i]=diff;
          dbgInfo.loc2.vec[i]=latestAdvLocation_postTracking.vec[i];
          latestAdvLocation_postTracking.vec[i]+=(trackingProgress)*diff*efDown;
          

        }
      }

    }



  }


  void copyCMDVec(float*dst,float*src)
  {
    memcpy(dst,src,sizeof(CMDVec));
  }


  std::string vec_to_string(float*dst)
  {
    std::string ret="[";
    for(int i=0;i<CMD_VEC_DIM;i++)
    {
      ret+=std::to_string(dst[i]);

      if(i<CMD_VEC_DIM-1)
        ret+=",";
    }

    return ret+"]";
  }



  void motUnit2PulseConversion(int32_t *dst,float *src)
  {
    
    dst[0]=(int32_t)(src[0]*1600*5/(2*3.14159));//R  arcDeg angle to steps
    dst[1]=(int32_t)(src[1]*1600/60);//Z  mm to steps
    dst[2]=(int32_t)(src[2]*3200/17);//X  mm to steps
  }

  void motPulse2UnitConversion(float *dst,int32_t *src)
  {
    dst[0]=((float)src[0]*2*3.14159/5/1600);//R  steps to arcDeg angle
    dst[1]=((float)src[1]*60/1600);//Z  steps to mm
    dst[2]=((float)src[2]*17/3200);//X  steps to mm
  }

  void EM_STOP()
  {
    homingFlag=false;
    trackingInfo.PtF0=trackingInfo.PtF1=NULL;
    segs.clear();



    dbg_msg_sendQ("EM_STOP");
  }

  virtual void getMotMoveVec(xVec_f *mot_vec_dst)
  {

    if(homingCMDID==-1 && homingFlag==true)
    {
      // xVec_f curMotLoc_unit={0};
      inverse(curMotLoc_unit.vec,latestAdvLocation_postTracking.vec);
    }
    else//in homing mode
    {

    }
    MotVec_i _curMotLoc_pls={0};
    // _curMotLoc_pls.vec[0]=(int32_t)(curMotLoc_unit.vec[0]*1600*4.5/(73*2*3.14159));//R  arc angle to steps

    motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);



    mot_vec_dst->vec[0]=-(_curMotLoc_pls.vec[0]-preMotloc_pls.vec[0]);//invert polarity
    mot_vec_dst->vec[1]=-(_curMotLoc_pls.vec[1]-preMotloc_pls.vec[1]);//Z
    mot_vec_dst->vec[2]=-(_curMotLoc_pls.vec[2]-preMotloc_pls.vec[2]);//invert polarity
    // mot_vec_dst->vec[3]=_curMotLoc_stp.vec[3]-preMotloc.vec[3];
    // mot_vec_dst->vec[4]=_curMotLoc_stp.vec[4]-preMotloc.vec[4];
    preMotloc_pls=_curMotLoc_pls;

  }

  // float 

  void inverse(float *mot_vec_dst,const float* loc_vec_src){

    // for(int i=0;i<CMD_VEC_DIM;i++)
    // {
    //   mot_vec_dst->vec[i]=loc_vec_src[i];
    // }




    float cx=loc_vec_src[0],cy=loc_vec_src[1];

    float x1,x2;
    volatile int res=findIntersection(XVecX,XVecY,cx,cy,Arm_r,&x1,&x2);




    float mx=x1<x2?x1:x2;

    float vx=cx-mx*XVecX;
    float vy=cy-mx*XVecY;
    
    mot_vec_dst[0]=atan2f(vy,vx);//R

    mot_vec_dst[1]=loc_vec_src[2];//Z

    mot_vec_dst[2]=mx;//X

    // volatile int res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
  }
  void forward(float* loc_vec_dst,const float *mot_vec_src)
  {
    // for(int i=0;i<CMD_VEC_DIM;i++)
    // {
    //   loc_vec_dst[i]=mot_vec_src->vec[i];
    // }


    float R= mot_vec_src[0];//R
    float Z= mot_vec_src[1];//R
    float X= mot_vec_src[2];//R
    
    float ArmX=cosf(R)*Arm_r+X*XVecX;
    float ArmY=sinf(R)*Arm_r+X*XVecY;

    
    loc_vec_dst[0]=ArmX;
    loc_vec_dst[1]=ArmY;
    loc_vec_dst[2]=Z;
  }




  static int instEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }
  static int IOCtrlEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=100;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    if(p_res->IO_CTRL.S){
      PG_M.static_Pin_info0|=((uint32_t)1)<<p_res->IO_CTRL.P;
    }
    else
    {
      PG_M.static_Pin_info0&=~(((uint32_t)1)<<p_res->IO_CTRL.P);
    }

    
    // if(p_res->IO_CTRL.S){
    //   GPIOLS32_SET(p_res->IO_CTRL.P);
    // }
    // else
    // {
    //   GPIOLS32_CLR(p_res->IO_CTRL.P);
    // }
    
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int OnTimeRepEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=10000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;



    struct Mstp2CommInfo tinfo={
    .type=Mstp2CommInfo_Type::respFrame,
    .isAck=p_res->ON_TIME_REP.isAck,
    .resp_id=p_res->ON_TIME_REP.id
    };

    {
      Mstp2CommInfo* Qhead=NULL;
      while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
      *Qhead=tinfo;
      Mstp2CommInfoQ.pushHead();
    }


    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int instTrackingEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    *(p_res->TRACKING_INFO.p_ctrl_progress)=0;

    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int instTrackingCMDEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    CTX_TrackingInfo *ti=(CTX_TrackingInfo *)p_res->Generic.obj;
    
    StpGroup_SIMP::TrackingInfo &trackingInfo=ti->self->trackingInfo;


    ti->self->loadNextTracking(ti);
    
  
    ti->self->trackingInfoCTX_pool.returnResource(ti);
    p_res->Generic.obj=NULL;
    sctx_pool.returnResource(p_res);
    seg->ctx=NULL;
    return 0;
  }





  void loadNextTracking(CTX_TrackingInfo *ti)
  {

    do{
      if(trackingInfo.progress!=-1)
      {
        ti->self->EM_STOP();
        break;
      }


      {//Init trackingInfo movParam & advInfo

        trackingInfo.movParam.type=MSTP_segment_type::seg_line;


        trackingInfo.movParam.vcur=
        trackingInfo.movParam.vto=0;

        trackingInfo.movParam.distanceStart=0;

        trackingInfo.movParam.ctx=NULL;
        trackingInfo.movParam.startCB=
        trackingInfo.movParam.endCB=NULL;




        trackingInfo.advInfo.deaWeagle=1.2;
        trackingInfo.advInfo.magicSpace=0,
        trackingInfo.advInfo.minSpeed=5,
        trackingInfo.advInfo.inInDAcc=false,
        trackingInfo.advInfo.dstanceWent=0;
      }

      

      trackingInfo.PtF1=ti->PtF;
      trackingInfo.Pr1=ti->Pr;
      memcpy(trackingInfo.param1,ti->param,sizeof(trackingInfo.param1));
      trackingInfo.movParam.vcen=ti->vcen;
      trackingInfo.movParam.deacc=ti->deacc;
      trackingInfo.movParam.acc=ti->acc;

      trackingInfo.movParam.Edistance=
      trackingInfo.movParam.distanceEnd=1000;


      trackingInfo.progress=0;//kick start the tracking process



    }while(0);
  }




  static int CtxRecycleEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=1000000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    
    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }



  static int TrackingHoldUntilEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=10000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;


    if(p_res->TRACKING_HOLD_UNTIL.id>=0)
    {

      struct Mstp2CommInfo tinfo={
      .type=Mstp2CommInfo_Type::respFrame,
      .isAck=true,
      .resp_id=p_res->TRACKING_HOLD_UNTIL.id
      };

      {
        Mstp2CommInfo* Qhead=NULL;
        while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
        *Qhead=tinfo;
        Mstp2CommInfoQ.pushHead();
      }

    }


    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }

  static int AssertTrackingSattledEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=10000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    StpGroup_SIMP *self=(StpGroup_SIMP *)p_res->Generic.obj;

    do{
      if(self->trackingInfo.progress!=-1)
      {
        self->EM_STOP();
        break;
      }
    }while(0);


    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  MSTP_segment_extra_info latestExtInfo={
    .speed=100,
    .speedOnAxisIdx=-1,
    .acc=1000,
    .deacc=-1000,
    .cornorR=0,

  };
  int GcodeParse(char **blkIdxes,int blkIdxesL,JsonDocument& cmd_ret)
  {

    
    if(blkIdxesL==0)
      return GCodeParser::GCodeParser_Status::LINE_EMPTY;
    GCodeParser::GCodeParser_Status retStatus=GCodeParser::GCodeParser_Status::LINE_EMPTY;




    char *cblk=blkIdxes[0];
    // int cblkL=blks[1]-blks[0];

    blkIdxes++;//move to next block
    blkIdxesL--;


    if(cblk[0]=='G')
    {
      if(CheckHead(cblk, "G28"))//G28 GO HOME!!!:
      {
        // homingStatus=HomingStatus::Start;
        // homingAxisIdx=0;
        homingSeq.clear();
        float fineSpeed=1;
        homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=1,.speed=50,.speed_fine=fineSpeed,.sensorPin=1,.sensorSense=1});
        homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=0,.speed=50*5/Arm_r,.speed_fine=fineSpeed*5/Arm_r,.sensorPin=0,.sensorSense=0});
        homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=-50,.speed_fine=-fineSpeed,.sensorPin=2,.sensorSense=0});
        // homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=1,.speed=5,.speed_fine=1,.sensorPin=1,.sensorSense=0});
        // homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=5,.speed_fine=1,.sensorPin=2,.sensorSense=0});

    

        homingCMDID=HACK_cur_cmd_id;

        return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
      }
      if(homingFlag==false)
      {
        cmd_ret["msg"]="homingFlag==false";
        return GCodeParser::GCodeParser_Status::TASK_FATAL_FAILED;
      }
      if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))
      {
        __PRT_D_("G1 baby!!!\n");


        CMDVec vec_coord=latestCMDLocation;
        float Tmp=NAN;



        Tmp=NAN;
        FindFloat("X",blkIdxes,blkIdxesL,Tmp);
        if(Tmp==Tmp)vec_coord.vec[0]=Tmp;

        Tmp=NAN;
        FindFloat("Y",blkIdxes,blkIdxesL,Tmp);
        if(Tmp==Tmp)vec_coord.vec[1]=Tmp;
        
        Tmp=NAN;
        FindFloat("Z",blkIdxes,blkIdxesL,Tmp);
        if(Tmp==Tmp)vec_coord.vec[2]=Tmp;





        // if(z==z)vec_coord.vec[2]=z;


        // xVec_f vec_mot=foward(vec_coord);

        MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);

        if(exinfo.acc!=exinfo.acc)exinfo.acc=latestExtInfo.acc;
        if(exinfo.deacc!=exinfo.deacc)exinfo.deacc=latestExtInfo.deacc;
        if(exinfo.speed!=exinfo.speed)exinfo.speed=latestExtInfo.speed;

        if(exinfo.cornorR!=exinfo.cornorR)exinfo.cornorR=latestExtInfo.cornorR;
        if(exinfo.speedOnAxisIdx!=exinfo.speedOnAxisIdx)exinfo.speedOnAxisIdx=latestExtInfo.speedOnAxisIdx;

        // __UPRT_I_("vec_coord:%f %f %f  exinfo:f:%f a:%f d:%f aidx:%d\n",vec_coord.vec[0],vec_coord.vec[1],vec_coord.vec[2],
        // exinfo.speed,exinfo.acc,exinfo.deacc,exinfo.speedOnAxisIdx);
        

        CMDVec moveVec;
        vecSub(moveVec.vec,vec_coord.vec,latestCMDLocation.vec,CMD_VEC_DIM);
        
        // __UPRT_I_("vec:%s",vec_to_string(moveVec.vec).c_str());
        pushInMoveVec(moveVec.vec,&exinfo,CMD_VEC_DIM,NULL,NULL,NULL);
        latestCMDLocation=vec_coord;

        latestExtInfo=exinfo;
                // ReadGVecData(blks,blkCount,vec_f,&exinfo);

        // ConvUnitVecToPulseVec(&vec_f,&exinfo);
        
        // xVec newLoc=stpG.pulse_latestLoc;
        // SetxVec_fToxVec(newLoc,vec_f);
        // // xVec goVec=vecSub(newLoc,stpG.pulse_latestLoc);
        
        // stpG.MoveTo(newLoc,NULL,&exinfo);

        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "G4 ") || CheckHead(cblk, "G04 "))//pause
      {

        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        if(P==P && P>=0)
        {
          pushInPause(P,NULL,NULL,NULL);
          return  GCodeParser::GCodeParser_Status::TASK_OK;
        }
        else
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
      }
    }
    else if(cblk[0]=='M')
    {
      if(homingFlag==false)
      {
        cmd_ret["msg"]="homingFlag==false";
        return GCodeParser::GCodeParser_Status::TASK_FATAL_FAILED;
      }
      if(CheckHead(cblk, "M42 "))//IO ctrl
      { 
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float S=NAN;
        FindFloat("S",blkIdxes,blkIdxesL,S);
        if(P!=P || S!=S)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }


        MSTP_SegCtx *p_res;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;

        p_res->IO_CTRL.P=P;
        p_res->IO_CTRL.S=S;
        pushInInstant(NULL,IOCtrlEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }


      else if(CheckHead(cblk, "M42.MODE "))//IO ctrl
      { 
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float M=NAN;
        FindFloat("M",blkIdxes,blkIdxesL,M);
        if(P!=P || M!=M)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
        pinMode(P,M);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "M400.BLOCKING"))
      { 
        
        while(segs.size())
        {
          yield();
        }

        // while( trackingState!=TrackingState::InSync && trackingState!=TrackingState::NOP) 
        // {
        //   yield();
        // }

        // cmd_ret["R"]=curMotLoc_unit.vec[0];
        // cmd_ret["Z"]=curMotLoc_unit.vec[1];
        // cmd_ret["X"]=curMotLoc_unit.vec[2];
        
        // cmd_ret["Rp"]=preMotloc_pls.vec[0];
        // cmd_ret["Zp"]=preMotloc_pls.vec[1];
        // cmd_ret["Xp"]=preMotloc_pls.vec[2];

        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "M400"))
      { 

        MSTP_SegCtx *p_res;
        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return GCodeParser::GCodeParser_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::ON_TIME_REPLY;

        p_res->ON_TIME_REP.id=HACK_cur_cmd_id;
        p_res->ON_TIME_REP.isAck=true;
        pushInInstant(NULL,OnTimeRepEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
      }

      else if(CheckHead(cblk, "M119"))
      {
        cmd_ret["input"]=PG_M.latest_input_pins;
        
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
    }
    return  GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }



  float vars[20]={0};
  int JCMDParse(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    JsonObject cmdData=cmd["cmd"];
    if(cmdData.isNull())
    {
      return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
    }

    if(!cmdData["type"].is<const char*>())
    {
      return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
    }

    int test_runCount=0;
    if(cmdData["runCount"].is<float>())
      test_runCount=cmdData["runCount"];

    int ret_vars_count=9999;
    if(cmdData["ret_vars_count"].is<float>())
      ret_vars_count=(int)cmdData["ret_vars_count"];


    const char* type = cmdData["type"];
    if(strcmp(type,"G01")==0)
    {
      
      cmd_ret["this"]="G01 yay~";
    }


    if(strcmp(type,"COMP")==0)
    {

      if(!cmdData["expr"].is<const char*>())
      {
        return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
      }

      const char* expr = cmdData["expr"];


      do{
        //Get arduinoJson array "vars"
        JsonArray arr = cmdData["vars"];
        if(arr.isNull())break;

        int len = sizeof(vars)/sizeof(vars[0]);
        if(arr.size()<len)len=arr.size();

        for (size_t i = 0; i <len; i++) {
          //check is number
          if(arr[i].is<float>())
          {
            vars[i] = arr[i].as<float>(); // Convert each element to float and store it.
          }
          else 
          {
            vars[i]=NAN;
          }
        }
        //fill vars with cmdData["vars"] number array


      }while(0);



      te_variable te_vars[] = {
       
        {"g", (void*)get_var, TE_CLOSURE1, vars},
        {"s", (void*)set_var, TE_CLOSURE2, vars}};
    


      auto stime=millis();
      int err;
      te_expr *n = te_compile(expr, te_vars, sizeof(te_vars)/sizeof(te_vars[0]), &err);

      auto etime=millis();
      cmd_ret["compileTime"]=etime-stime;
      stime=etime;


      if (n) {




        float result = te_eval(n);
        for(int i=1;i<test_runCount;i++)
        {
          result = te_eval(n);
        }
        etime=millis();
        cmd_ret["exeTime"]=etime-stime;
        cmd_ret["result"]=result;

        //put vars back to cmd_ret
        JsonArray arr = cmd_ret.createNestedArray("vars");
        if(ret_vars_count>sizeof(vars)/sizeof(vars[0])) 
        {
          ret_vars_count=sizeof(vars)/sizeof(vars[0]);
        }
        for (size_t i = 0; i < ret_vars_count; i++) {
          arr.add(vars[i]);
        }


        te_free(n);

        return GCodeParser::GCodeParser_Status::TASK_OK;
      } else {
        cmd_ret["test2_error"]=err;
      }
      return GCodeParser::GCodeParser_Status::TASK_FAILED;

    }

    
    if(strcmp(type,"LC_intersect")==0)
    {

      //call findIntersection

      int test_runCount=1;
      if(cmdData["runCount"].is<float>())
        test_runCount=cmdData["runCount"];
        
      float vx=cmdData["vx"].as<float>();
      float vy=cmdData["vy"].as<float>();
      float cx=cmdData["cx"].as<float>();
      float cy=cmdData["cy"].as<float>();
      float r=cmdData["r"].as<float>();

      auto stime=millis();
      float t1,t2;
      volatile int res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
      for(int i=1;i<test_runCount;i++)
      {
        res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
      }
      auto etime=millis();
      cmd_ret["exeTime"]=etime-stime;
      //put result to cmd_ret


      JsonArray arr = cmd_ret.createNestedArray("pt_params");
      if(res>0)
      {
        arr.add(t1);
        if(res>1)
          arr.add(t2);
      }
      return GCodeParser::GCodeParser_Status::TASK_OK;

    }

    if(strcmp(type,"FORWARD")==0)//motor location to head location
    {
      float R=cmdData["R"].as<float>();
      float Z=cmdData["Z"].as<float>();
      float X=cmdData["X"].as<float>();

      float mot_vec[3]={R,Z,X};
      float loc[3]={0};

      forward(loc,mot_vec);

      cmd_ret["X"]=loc[0];
      cmd_ret["Y"]=loc[1];
      cmd_ret["Z"]=loc[2];
    }

    if(strcmp(type,"INVERSE")==0)
    {
      float X=cmdData["X"].as<float>();
      float Y=cmdData["Y"].as<float>();
      float Z=cmdData["Z"].as<float>();

      float loc[3]={X,Y,Z};
      float mot_vec[3]={0};

      inverse(mot_vec,loc);

      cmd_ret["R"]=mot_vec[0];
      cmd_ret["Z"]=mot_vec[1];
      cmd_ret["X"]=mot_vec[2];
    }


    if(homingFlag==false)
    {
      cmd_ret["msg"]="homingFlag==false";
      return GCodeParser::GCodeParser_Status::TASK_FATAL_FAILED;
    }

    //return trackingInfo.progress
    if(strcmp(type,"TRACKING_PROGRESS")==0)
    {
      cmd_ret["progress"]=trackingInfo.progress;

      cmd_ret["l0"]=dbgInfo.loc.vec[0];
      cmd_ret["l1"]=dbgInfo.loc.vec[1];
      cmd_ret["l2"]=dbgInfo.loc.vec[2];
      cmd_ret["L0"]=dbgInfo.loc2.vec[0];
      cmd_ret["L1"]=dbgInfo.loc2.vec[1];
      cmd_ret["L2"]=dbgInfo.loc2.vec[2];
      
      return GCodeParser::GCodeParser_Status::TASK_OK;
    }



    if(strcmp(type,"TargetTrack")==0)
    {


      int trackIdx=-1;
      if(cmdData["trackIdx"].is<float>())
        trackIdx=cmdData["trackIdx"];
      else
      {
        cmd_ret["msg"]="trackIdx is needed";
        return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
      }
      

      //get the latst seq pointer in the queue


      CTX_TrackingInfo tinfo;

      if(trackIdx==0)
      {
        tinfo.PtF=NULL;
      }
      else if(trackIdx==1)
      {
        float plat_angle=NAN;
        if(cmdData["plat_angle"].is<float>())
          plat_angle=cmdData["plat_angle"];
        
        float obj_x=NAN;
        if(cmdData["obj_x"].is<float>())
          obj_x=cmdData["obj_x"]; 

        float obj_y=NAN;
        if(cmdData["obj_y"].is<float>())
          obj_y=cmdData["obj_y"];

        if(plat_angle!=plat_angle || obj_x!=obj_x || obj_y!=obj_y)
        {
          cmd_ret["msg"]="plat_angle obj_x obj_y are needed";
          return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
        }

        PlateObjLocParamExtract(tinfo.param,obj_x,obj_y,latestCMDLocation.vec[2],plat_angle);

        tinfo.PtF=PlateObjLocGet;

      }

      {
        float gox=NAN;
        if(cmdData["X"].is<float>())
          gox=cmdData["X"]; 

        float goy=NAN;
        if(cmdData["Y"].is<float>())
          goy=cmdData["Y"];

        if(goy!=goy || gox!=gox)
        {
          cmd_ret["msg"]="target X,Y are needed";
          return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;
        }


        tinfo.Pr=latestCMDLocation;
        tinfo.Pr.vec[0]=gox;
        tinfo.Pr.vec[1]=goy;
      }


      MSTP_segment_adv_info _advInfo;
      MSTP_segment _movParam;
      {

        _movParam.type=MSTP_segment_type::seg_line;

        // float transitionTime=1000;
        float acc=1000;
        float deacc=-1000;
        float vcen=1000;

        if(cmdData["acc"].is<float>())
          acc=cmdData["acc"];
        if(acc<0)acc=-acc;


        deacc=-1000;
        if(cmdData["deacc"].is<float>())
          deacc=cmdData["deacc"];
        else
          deacc=acc;
        if(deacc>0)deacc=-deacc;

        vcen=1000;
        if(cmdData["f"].is<float>())
          vcen=cmdData["f"];

        tinfo.acc=acc;
        tinfo.deacc=deacc;
        tinfo.vcen=vcen;

        tinfo.self=this;

      }

      CTX_TrackingInfo *p_tinfo;
      // while( (p_tinfo=TrackingInfoQ.getHead()) ==NULL)yield();//wait until the queue is available
      while((p_tinfo=trackingInfoCTX_pool.applyResource())==NULL)yield();
      *p_tinfo=tinfo;


      MSTP_SegCtx *p_res;
      while((p_res=sctx_pool.applyResource())==NULL)yield();



      
      p_res->type=MSTP_SegCtx_TYPE::GENERIC;
      p_res->Generic.obj=(void*)p_tinfo;

      pushInInstant(NULL,instTrackingCMDEndCB,(void*)p_res);

      return  GCodeParser::GCodeParser_Status::TASK_OK;





  
    }

    if(strcmp(type,"TargetTrack.d")==0)
    {
      //The TargetFollow is to let effector follow the moving target location(Pt)
      //it will take the latest CMD location as the refrence point (Pr),
      // TargetFollow will consider the refrence point is on the Pt, so the effector will move to the Pt
      //The CMD location (Pc) of the effector will be the P=Pc+(Pt-Pr)*progress    progress is from 0(before) to 1(in sync)
      //You may consider the moving target "freezed"(virtually) at the refrence point


      //P=Pc+(Pt0-Pr0)*(1-progress)+(Pt1-Pr1)*progress
      //Pt0=PtF0(param0)


      if(trackingInfo.progress!=-1)
      {
        //still in privious tracking progress
        retdoc["progress"]=trackingInfo.progress;
        return GCodeParser::GCodeParser_Status::TASK_FAILED;
        // 
      }




      int trackIdx=NAN;
      if(cmdData["trackIdx"].is<float>())
        trackIdx=cmdData["trackIdx"];


      if(trackIdx==0)
      {
        trackingInfo.PtF1=NULL;
      }
      else if(trackIdx==1)
      {
        float plat_angle=NAN;
        if(cmdData["plat_angle"].is<float>())
          plat_angle=cmdData["plat_angle"];
        
        float obj_x=NAN;
        if(cmdData["obj_x"].is<float>())
          obj_x=cmdData["obj_x"]; 

        float obj_y=NAN;
        if(cmdData["obj_y"].is<float>())
          obj_y=cmdData["obj_y"];

        if(plat_angle!=plat_angle || obj_x!=obj_x || obj_y!=obj_y)
          return GCodeParser::GCodeParser_Status::PARAM_PARSE_ERROR;



        

        PlateObjLocParamExtract(trackingInfo.param1,obj_x,obj_y,latestCMDLocation.vec[2],plat_angle);

        // cmd_ret["p0"]=trackingInfo.param1[0];
        // cmd_ret["p1"]=trackingInfo.param1[1];
        // cmd_ret["p2"]=trackingInfo.param1[2];
        // cmd_ret["p3"]=trackingInfo.param1[3];
        trackingInfo.PtF1=PlateObjLocGet;

      }
      trackingInfo.Pr1=latestCMDLocation;


      {

        trackingInfo.movParam.type=MSTP_segment_type::seg_line;

        // float transitionTime=1000;
        trackingInfo.movParam.acc=1000;

        if(cmdData["acc"].is<float>())
          trackingInfo.movParam.acc=cmdData["acc"];
        if(trackingInfo.movParam.acc<0)trackingInfo.movParam.acc=-trackingInfo.movParam.acc;


        trackingInfo.movParam.deacc=-1000;
        if(cmdData["deacc"].is<float>())
          trackingInfo.movParam.deacc=cmdData["deacc"];
        else
          trackingInfo.movParam.deacc=trackingInfo.movParam.acc;
        if(trackingInfo.movParam.deacc>0)trackingInfo.movParam.deacc=-trackingInfo.movParam.deacc;


        trackingInfo.movParam.vcen=1000;
        if(cmdData["f"].is<float>())
          trackingInfo.movParam.vcen=cmdData["f"];



        trackingInfo.movParam.vcur=
        trackingInfo.movParam.vto=0;

        trackingInfo.movParam.Edistance=
        trackingInfo.movParam.distanceEnd=1000;
        trackingInfo.movParam.distanceStart=0;

        trackingInfo.movParam.ctx=NULL;
        trackingInfo.movParam.startCB=
        trackingInfo.movParam.endCB=NULL;
      }

      {
        trackingInfo.advInfo=adv_info;
        trackingInfo.advInfo.deaWeagle=1.2;


        trackingInfo.advInfo.magicSpace=0,
        trackingInfo.advInfo.minSpeed=5,
        trackingInfo.advInfo.inInDAcc=false,
        trackingInfo.advInfo.dstanceWent=0;
      }

      

      int retryCount=0;
      MSTP_SegCtx *p_res;

      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::TRACKING_PROGRESS;
      p_res->TRACKING_INFO.p_ctrl_progress=&trackingInfo.progress;

      pushInInstant(NULL,instTrackingEndCB,(void*)p_res);

      return  GCodeParser::GCodeParser_Status::TASK_OK;

    }



    if(strcmp(type,"ASSERT_TargetTrack_sattled")==0)
    {



      MSTP_SegCtx *p_res;

      while((p_res=sctx_pool.applyResource())==NULL) yield();

      p_res->type=MSTP_SegCtx_TYPE::GENERIC;
      p_res->Generic.obj=(void*)this;

      
      pushInInstant(NULL,AssertTrackingSattledEndCB,(void*)p_res);

      return  GCodeParser::GCodeParser_Status::TASK_OK;
    }

    if(strcmp(type,"TargetTrack_sattled")==0)
    {
      MSTP_SegCtx *p_res;

      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED;

      p_res->TRACKING_HOLD_UNTIL.id=-1;
      //-1 means halt forever
      pushInPause(-1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

      return  GCodeParser::GCodeParser_Status::TASK_OK;
    }



    if(strcmp(type,"TargetTrack_sattled.BLOCKING")==0)
    {
      MSTP_SegCtx *p_res;

      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED;

      p_res->TRACKING_HOLD_UNTIL.id=cmdData["id"];

      //-1 means halt forever
      pushInPause(-1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

      return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
    }
      
    return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }
};

StpGroup_SIMP SG_SIMP;


GCodeParser_M gcpm;

bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK);

int MData_JR::recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data,size_t dataL)
{
  for(int i=0;i<buffIdx;i++)
  {
    if(dataBuff[i]=='"')
      dataBuff[i]='\'';
  }  
  dataBuff[buffIdx]='\0';
  // doDataLog=true;

  // if(recv_data)
  //   dbg_printf("recv_ERROR:%d %s dat:%s",errorcode,dataBuff,string((char*)recv_data,0,9).c_str());
  // else 
  //   dbg_printf("recv_ERROR:%d %s",errorcode,dataBuff);

  return 0;
}


int GCMD_Magic_SafeSpace=3;

GCodeParser::GCodeParser_Status MData_JR::insertCMD(StpGroup *stpG,JsonDocument &cmd)
{


  GCodeParser::GCodeParser_Status res=GCodeParser::GCodeParser_Status::TASK_FAILED;
  // check if the key "code" exists
  if(cmd["code"].is<const char*>())
  {
    gcpm.putJSONNote(&retdoc);
    gcpm.putTargetStepperGroup(stpG);

    const char* code = cmd["code"];

      // __PRT_I_("insertCMD:%s",code);
    res= gcpm.runGCode(code);

    gcpm.putJSONNote(NULL);
    gcpm.putTargetStepperGroup(NULL);
  }
  else
  {
    res=(GCodeParser::GCodeParser_Status)stpG->JCMDParse(cmd,retdoc);
  }

  // retdoc["cmd"]=cmd;

  return res;
}


int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    doc.clear();
    retdoc.clear();
    DeserializationError error = deserializeJson(doc, (const char*)raw);
    bool rspAck=false;
    bool doRsp=false;

    const char* type = doc["type"];
    // const char* id = doc["id"];

    if(strcmp(type,"G")==0)
    {
      HACK_cur_cmd_id=-1;
      doRsp=true;
      
      do
      {

        if(doc["id"].is<int>()==false)break;
        if(doc["group"].is<int>()==false)break;

        int groupIdx=doc["group"];

        if(groupIdx<0)break;
        if(groupIdx>=mstpV2.stepperGroup.size())break;

        auto &stpG=*(mstpV2.stepperGroup[groupIdx]);

        if(stpG.bufferJCMD_ID>=0)
        {
          rspAck=false;
          break;
        }


        int cmd_id=HACK_cur_cmd_id=doc["id"];


        if(stpG.segs.space()>GCMD_Magic_SafeSpace)//give it a magic safe space
        {

          doRsp=true;
          auto retSt=insertCMD(&stpG,doc);


          
          if(retSt==GCodeParser::GCodeParser_Status::TASK_OK)
          {
            rspAck=true;
          }
          else if(retSt==GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP)
          {
            rspAck=true;
            doRsp=false;
          }
          else
          {
            rspAck=false;
          }
        }
        else
        {
          
          doRsp=false;
          rspAck=true;
          // strcpy(stpG.bufferGCMD,code);
          //deep copy json "doc" to  stpG.bufferJCMD

          // stpG.bufferJCMD.clear();
          // strcpy(stpG.bufferJCMD_raw,(const char*)raw);
          memcpy(stpG.bufferJCMD_raw,raw,rawL);

          stpG.bufferJCMD_ID=cmd_id;

        }





        // rspAck=true;
      } while (false);
      

    }
    
    if(strcmp(type,"TTTT")==0)
    {
      doRsp=true;
      if(doc["vec"].is<JsonArray>() && doc["seg_time_us"].is<int>())
      {
        xVec vec={0};
        JsonArray array = doc["vec"].as<JsonArray>();
        int i=0;
        for(JsonVariant v : array) {
          vec.vec[i++]=v.as<int>();
        }

        while(PG_M.segSteps_next!=0)//wait until the buffer is empty
        {
          yield();
        }
        PG_M.loadNext(doc["seg_time_us"],vec);
        rspAck=true;

      }
    }
    else if(strcmp(type,"get_running_stat")==0)
    {

      // {
      //   JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");

      //   for(int i=0;i<ERROR_HIST.size();i++)
      //   {
      //     jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
      //   }
      // }


      // JsonObject jCountInfo  = retdoc.createNestedObject("count");
      // jCountInfo["SEL1"]=SEL1_Count;
      // jCountInfo["SEL2"]=SEL2_Count;
      // jCountInfo["SEL3"]=SEL3_Count;
      // jCountInfo["NA"]=NA_Count;

      //current state
      // retdoc["state"]=(int)sysinfo.state;

      // retdoc["plateFreq"]=SYS_TAR_FREQ;//SYS_CUR_FREQ;


      // retdoc["plateFreq"]=NA_Count;


      doRsp=rspAck=true;

    }
    else if(strcmp(type,"RESET")==0)
    {
      
      return msg_printf("RESET_OK","");
    }
    else if(strcmp(type,"ask_JsonRaw_version")==0)
    {
      
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return this->rsp_JsonRaw_version();
    }
    else if(strcmp(type,"rsp_JsonRaw_version")==0)
    {
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return 0;
    }
    else if(strcmp(type,"PING")==0)
    {
      retdoc["type"]="PONG"; 
      doRsp=rspAck=true;
    }
    else if(strcmp(type,"get_setup")==0)
    {

      retdoc["type"]="get_setup";
      retdoc["ver"]="0.5.0";
      retdoc["name"]="CNC_1";






      
      genMachineSetup(retdoc);

      
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"set_setup")==0)
    {
      retdoc["type"]="set_setup";
      
      setMachineSetup(doc);
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"PIN_CONF")==0)
    {
      
      if(doc["pin"].is<int>())
      {
        int pinNo = doc["pin"];

      
        if(doc["mode"].is<int>())
        {
          int mode= doc["mode"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(mode)
          {
            case 0:pinMode(pinNo, INPUT);break;
            case 1:pinMode(pinNo, OUTPUT);break;
            case 2:pinMode(pinNo, INPUT_PULLUP);break;
            case 3:pinMode(pinNo, INPUT_PULLDOWN);break;
          }
        }
        else if(doc["output"].is<int>())
        {
          int output= doc["output"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(output)
          {
            case -2://analog
            {
              int value=analogRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;

              break;
            }
            case -1://digital
            {
              int value=digitalRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;
              break;
            }
            case 0:digitalWrite(pinNo, LOW);break;
            case 1:digitalWrite(pinNo, HIGH);break;
          }
        }
      }
      else
      {

      }
      


    
      // retdoc["type"]="DBG_PRT";
      // // retdoc["msg"]=doc;
      // retdoc["error"]=error.code();
      // retdoc["id"]=doc["id"];
      // uint8_t buff[300];
      // int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      // send_json_string(0,buff,slen,0);
    }
    else if(strcmp(type,"BYE")==0)
    {
      doRsp=rspAck=true;

    }      
    
    
    else if(AUX_Task_Try_Read(doc,type,retdoc,doRsp,rspAck))
    {
    }


    if(doRsp)
    {
      retdoc["id"]=(int)doc["id"];
      retdoc["ack"]=rspAck;
      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      send_json_string(0,buff,slen,0);

    }
  }


  return 0;


}
int MData_JR::send_data(int head_room,uint8_t *data,int len,int leg_room){
  Serial.write(data,len);
  return 0;
}

int MData_JR::dbg_printf(const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"dbg\":\"");
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

int MData_JR::msg_printf(const char *type,const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"type\":\"%s\",\"data\":\"",type);
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

void MData_JR::loop()
{
  for(int i=0;i<mstpV2.stepperGroup.size();i++)
  {//check for buffered G cmd

    StpGroup *stpG=mstpV2.stepperGroup[i];
    if(stpG->bufferJCMD_ID<0)continue;
    if(stpG->segs.space()<GCMD_Magic_SafeSpace)continue;

    bool rspAck=false;
    bool doRsp=true;

    int cmd_id=stpG->bufferJCMD_ID;
    stpG->bufferJCMD_ID=-1;


    DeserializationError error = deserializeJson(doc, (char*)stpG->bufferJCMD_raw);
    auto retSt=insertCMD(stpG,doc);
    if(retSt==GCodeParser::GCodeParser_Status::TASK_OK)
    {
      rspAck=true;
    }
    else if(retSt==GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP)
    {
      rspAck=true;
      doRsp=false;
    }
    else
    {
      rspAck=false;
    }


    if(doRsp)
    {
      retdoc["id"]=cmd_id;
      retdoc["ack"]=rspAck;
      
      // uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)stpG->bufferJCMD_raw,sizeof(stpG->bufferJCMD_raw));
      send_json_string(0,(uint8_t*)stpG->bufferJCMD_raw,slen,0);

    }
  }
}

#define AUX_COUNT 5

enum AUX_TASK_INFO_TYPE{
  AUX_DELAY=1,
  AUX_IO_CTRL=2,
  AUX_WAIT_FOR_ENC=3,
  AUX_WAIT_FOR_FINISH=1000,

};


struct AUX_TASK_INFO_WAIT_FOR_FINISH{
  int cmd_id;

};

struct AUX_TASK_INFO_WAIT_FOR_ENC{
  int value;

};
struct AUX_TASK_INFO_DELAY{
  int time;

};
struct AUX_TASK_INFO_IO_CTRL{
  
  int pin;
  int state;

  char CID[50];
  char TTAG[100];
  int TID;


};

struct AUX_TASK_INFO {
  AUX_TASK_INFO(){}
  ~AUX_TASK_INFO(){}
  AUX_TASK_INFO_TYPE type;
  


  union {
    AUX_TASK_INFO_DELAY delayInfo;
    AUX_TASK_INFO_IO_CTRL ioCtrl;
    AUX_TASK_INFO_WAIT_FOR_ENC wait_enc;
    AUX_TASK_INFO_WAIT_FOR_FINISH wait_fin;
  }; 

  //Just for ioCtrl
  // string CID;
  // string TTAG;
};

// static QueueHandle_t SeqCalcTaskQueue;
static QueueHandle_t AUXTaskQueue[AUX_COUNT];

bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK)
{
  int AUX_THREAD_ID=(doc["aid"].is<int>())?doc["aid"]:0;
  if(AUX_THREAD_ID>=AUX_COUNT)
  {
    return false;
  }
  if(strcmp(type,"AUX_TEST")==0)
  {
    ret_doc["msg"]="Try more";
    doRsp=true;
    isACK=false;
    return true;
  }


  if(strcmp(type,"AUX_DELAY")==0)
  {


    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_DELAY;


    task.delayInfo.time=(doc["P"].is<int>())?doc["P"]:1000;

    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    doRsp=true;
    isACK=true;
    return true;
  }
  if(strcmp(type,"AUX_WAIT_FOR_ENC")==0)
  {

    doRsp=true;



    if(doc["value"].is<int>()==false)
    {
      isACK=false;
      return true;
    }
    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_ENC;
    task.wait_enc.value=doc["value"];

    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    isACK=true;
    return true;
  }
  if(strcmp(type,"AUX_WAIT_FOR_FINISH")==0)
  {



    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH;
    task.wait_fin.cmd_id=doc["id"];


    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    doRsp=false;
    isACK=true;
    return true;
  }



  if(strcmp(type,"AUX_IO_CTRL")==0)
  {


    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_IO_CTRL;


    task.ioCtrl.pin=(doc["pin"].is<int>())?doc["pin"]:-1;
    task.ioCtrl.state=(doc["state"].is<int>())?doc["state"]:-1;

    if(task.ioCtrl.pin==-1 || task.ioCtrl.state==-1)
    {
      isACK=false;

    }
    else
    {
      task.ioCtrl.CID[0]='\0';
      task.ioCtrl.TTAG[0]='\0';
      task.ioCtrl.TID=-1;
      if(doc["CID"].is<const char*>()  )
      { 
        strncpy(task.ioCtrl.CID,(const char*)doc["CID"],sizeof(task.ioCtrl.CID));

        if(doc["TTAG"].is<const char*>() )
        {
          strncpy(task.ioCtrl.TTAG,(const char*)doc["TTAG"],sizeof(task.ioCtrl.TTAG));
        }


        task.ioCtrl.TID=(doc["TID"].is<int>() )?doc["TID"]:-1;

      }

      


      xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
      isACK=true;
    }
    doRsp=true;
    return true;
  }

  return false;
}



RingBuf_Static<struct Mstp2CommInfo,20,uint8_t> AUX2CommInfoQ;
static SemaphoreHandle_t AUX2Comm_Lock;

void AUX_task(void *pvParameter)
{
  QueueHandle_t &Q=*(QueueHandle_t *)pvParameter;
    while(1) {
      AUX_TASK_INFO info; 
      if (xQueueReceive(Q, (void *)&info, portMAX_DELAY) == pdTRUE) {

        switch(info.type)
        {

          case AUX_TASK_INFO_TYPE::AUX_DELAY:
            vTaskDelay(info.delayInfo.time / portTICK_RATE_MS);
            // G_LOG(">>>>");
          break;

          case AUX_TASK_INFO_TYPE::AUX_IO_CTRL :

          break;


          case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH :

              struct Mstp2CommInfo tinfo={
              .type=Mstp2CommInfo_Type::respFrame,
              .isAck=true,
              .resp_id=info.wait_fin.cmd_id
              };

              xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);//LOCK
              Mstp2CommInfo* Qhead=NULL;
              while( (Qhead=AUX2CommInfoQ.getHead()) ==NULL);
              *Qhead=tinfo;
              AUX2CommInfoQ.pushHead();
              xSemaphoreGive(AUX2Comm_Lock);//UNLOCK
          break;
        }
      }
    }
}

//float 100 add,sub 5.5us
//float 100 mult,div 24us
//float 100 sin 48us

//int 100 add,mult,div 5.5us
//int 100 div 5.8us


void SEG_CALC_task(void *pvParameter)
{

  // xVec pre_mot_iloc={0};
  while(1)
  { 
    xSemaphoreTake(SeqCalcTaskLock, portMAX_DELAY);//LOCK

    digitalWrite(PIN_DBG1, 1);
    xVec_f mot_loc={0};
    for(int i=0;i<mstpV2.stepperGroup.size();i++)//calc next step
    {
      mstpV2.stepperGroup[i]->update();

      mstpV2.stepperGroup[i]->getMotMoveVec(&mot_loc);
    }

    xVec mot_ivec={0};
    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      mot_ivec.vec[i]=(uint32_t)mot_loc.vec[i];//-pre_mot_iloc.vec[i]);

      // pre_mot_iloc.vec[i]=(uint32_t)mot_loc.vec[i];
    }

 
    PG_M.loadNext(mstpV2.updatePeriod_s*1000000,mot_ivec);
    digitalWrite(PIN_DBG1, 0);
  }

}



int rzERROR=0;
void setup()
{
  SeqCalcTaskLock = xSemaphoreCreateBinary();

  
  // mstpV2.stepperGroup.push_back(&SG_RX);
  mstpV2.stepperGroup.push_back(&SG_SIMP);
  mstpV2.stepperGroup.push_back(&SG_PLATE);

  
  // noInterrupts();
  // Serial.begin(921600);//230400);
  Serial.begin(115200);//230400);
  // Serial.begin(460800);
  Serial.setRxBufferSize(500);
  // // setup_comm();
  timer = timerBegin(0, 80*1000*1000/_TICK2SEC_BASE_, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);


  AUX2Comm_Lock = xSemaphoreCreateMutex();
  for(int i=0;i<AUX_COUNT;i++)
  {
    AUXTaskQueue[i] = xQueueCreate(20 /* Number of queue slots */, sizeof(AUX_TASK_INFO));
    xTaskCreatePinnedToCore(&AUX_task, "AUX_task", 2048, (void*)&AUXTaskQueue[i], 1, NULL, 0);

  }

  // SeqCalcTaskQueue= xQueueCreate(2, sizeof(uint32_t));
  xTaskCreatePinnedToCore(&SEG_CALC_task, "SEGCalcTask", 1024,NULL, configMAX_PRIORITIES-1, NULL, 0);

  pinMode(pin_TRIG_595, OUTPUT);
  pinMode(pin_SH_165, OUTPUT);
  pinMode(PIN_LED, OUTPUT);


  pinMode(PIN_DBG0, OUTPUT);
  pinMode(PIN_DBG1, OUTPUT);

  spi1= direct_spi_init(1,40*1000*1000,PIN_NUM_MOSI,PIN_NUM_MISO,PIN_NUM_CLK,PIN_NUM_CS);
  dspi_device_select(spi1,1);

}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    yield();
  }
}

// MSTP_SegCtx ctx[10];


// string toFixed(float num,int powNum=100)
// {
//   int ipnum=round(num*powNum);
//   int inum=ipnum/powNum;

//   string istr=to_string(inum);
//   int pnum=(ipnum%powNum);

//   string pstr=to_string(pnum+powNum);

//   string resStr=istr+pstr;
//   resStr[istr.length()]='.';
//   return resStr;
// }


bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


static uint8_t recvBuf[20];
void loop()
{
  djrl.loop();
  {
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.read(recvBuf,sizeof(recvBuf-1));
      //
      // djrl.dbg_printf("recvLen:%d",recvLen);
      djrl.recv_data((uint8_t*)recvBuf,recvLen);
      if(doDataLog)
      {
        for(int i=0;i<recvLen;i++) 
        {
          if(recvBuf[i]=='"')
            recvBuf[i]='\'';
        }     
        recvBuf[recvLen]='\0';
        djrl.dbg_printf(">%s",recvBuf);
      }

    }
    if(recvF)
    {
      // djrl.dbg_printf("recv DONE");
    }
  }


  {



    uint8_t buff[700];
    while(1)
    {
      bool hasNewInfo=false;
      Mstp2CommInfo info;
      if(hasNewInfo ==false && 0!=(Mstp2CommInfoQ.size()))
      {
        info=*Mstp2CommInfoQ.getTail();
        Mstp2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }


      xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);
      if(hasNewInfo ==false && 0!=(AUX2CommInfoQ.size()))
      {
        info=*AUX2CommInfoQ.getTail();
        AUX2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }
      xSemaphoreGive(AUX2Comm_Lock);




      if(hasNewInfo==false)break;

      retdoc.clear();
      // retdoc["tag"]="s_Step_"+std::to_string((int)info.step);
      // retdoc["trigger_id"]=info.step;
      switch (info.type)
      {
        case Mstp2CommInfo_Type::ext_log :
        {

          djrl.dbg_printf(info.strinfo);

          break;
        }
      
        case Mstp2CommInfo_Type::respFrame :
        {


          retdoc["id"]=info.resp_id;
          retdoc["ack"]=info.isAck;
          if(info.strinfo[0]!='\0')
          {
            retdoc["msg"]=info.strinfo;
          }
          
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }
      }
    }
  }


  // static unsigned long startMillis=0; 
  // unsigned long currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  // if (currentMillis - startMillis >= 100)  //test whether the period has elapsed
  // {
  //   startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.

  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>24));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>16));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>8));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins));
  //   Serial.printf("\n");
  // }

}





int intArrayContent_ToJson(char *jbuff, uint32_t jbuffL, int16_t *intarray, int intarrayL)
{
  uint32_t MessageL = 0;

  for (int i = 0; i < intarrayL; i++)
    MessageL += sprintf((char *)jbuff + MessageL, "%d,", intarray[i]);
  MessageL--; //remove the last comma',';

  return MessageL;
}


void genMachineSetup(JsonDocument &jdoc)
{

  jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  // auto obj=jdoc.createNestedObject("obj");
}

#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}

void setMachineSetup(JsonDocument &jdoc)
{
  // JSON_SETIF_ABLE(g_cam_trig_delay,jdoc,"cam_trig_delay");
}





