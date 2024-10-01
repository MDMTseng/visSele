
// #include <Test.h>
#include <MSteppersV2.h>
#include <string>
#undef min
#undef max
#include "Arduino.h"
#include "TestGround.h"
#include "ArduinoJson.h"
#include <Data_Layer_Protocol.h>
#include "SCoop.h"
//#include "tinyexpr.h"
/* QEC CiA402 CSP Mode Example */
#include "Ethercat.h" // Include the EtherCAT Library
#include "MotorCtrlCiA402.h"

#include "StateMachine.h"


#include "ValueMapping.h" // Include the EtherCAT Library

#define IO0_ECAT_STATION_ID 0
#define IO1_ECAT_STATION_ID 1

#define PPU_ECAT_STATION_ID 2
#define ARM_ECAT_STATION_ID 4
#define PLAT_ECAT_STATION_ID 3

#include "MSteppersV2.cpp"



bool enable_periodic_print=true;
float GetPlateObjLocOnArmRZV_DBG[3];

int motorMonitor_dbgCounter=0;
bool motorMonitor_enableRecord=true;
int motorMonitor_recordCD=0;
int motorMonitor_recordSkipMod=1;

RingBuf_Static<int,500> motorMonitor;

float updatePeriod_s=0.001;
uint64_t SYS_TIME_ms()
{
  static uint32_t carryTime=0;

  static uint32_t pre_=0;
  uint32_t cur_=millis();

  if(cur_<pre_)//overflow happens every 49.7 days,so SYS_TIME_ms has to be called within 25 days
  {
    carryTime++;
  }
  
  pre_=cur_;
  return (    ( ((uint64_t)carryTime)  <<32)   |cur_);//64bit overflow happens every 584.9 years fix it after that
}


int32_t dbgInfo=0;

enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,
  HALT_UNTIL_TRACKING_SATTLED=14,
  ASSERT_TRACKING_SATTLED=15,

  SEG_HALT=20,
  SEG_HALT_FOR_CHECKPOINT=21,
  TRACKING_PROGRESS=50,

  GENERIC=999
};


enum Mstp2CommInfo_Type{
  trigInfo=1000,
  ext_log=1001,
  respFrame=1002,
  respCheckpointFrame=1003,
};



struct Mstp2CommInfo{//TODO: rename the infoQ to be more versatile
  Mstp2CommInfo_Type type;

  //trigInfo
  int trig_id;
  int cam_id;

  int checkPointInterval;
  int segQSpace;
  char strinfo[60];
  //log
  // char log[40];

  //respFrame
  bool isAck;
  int resp_id;
};

RingBuf_Static<struct Mstp2CommInfo,20> Mstp2CommInfoQ;

#define AUX_COUNT 5

struct MSTP_SegCtx_IOCTRL{
  uint32_t bank=0,mask=0,pout=0;
  int32_t tid,cid,ttagbf;
};


struct MSTP_SegCtx_INPUTMON{
  uint32_t _PINS,_PIN_NS;
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


struct MSTP_SegCtx_SegHalt{
  bool haltFlag;
};


struct MSTP_SegCtx_SegHalt_for_CheckPoint{
  int target_checkpoint;
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
    struct MSTP_SegCtx_SegHalt SEG_HALT;
    struct MSTP_SegCtx_SegHalt_for_CheckPoint SEG_HALT_FOR_CHECKPOINT;
  };
  // char TTAG[40];
  // int TID;
};




bool jgetBool(JsonDocument& json,std::string key,bool defaultValue=false)
{
  bool tmpValue=defaultValue;
  if(json[key].is<bool>())
  {
    tmpValue=json[key].as<bool>();
  }
  return tmpValue;
}
int32_t jgetInt32(JsonDocument& json,std::string key,int32_t defaultValue=-1)
{
  int32_t tmpValue=defaultValue;
  if(json[key].is<int32_t>())
  {
    tmpValue=json[key].as<int32_t>();
  }
  return tmpValue;
}

float jgetNum(JsonDocument& json,std::string key,float defaultValue=NAN)
{
  float tmpValue=defaultValue;
  if(json[key].is<float>())
  {
    tmpValue=json[key].as<float>();
  }
  return tmpValue;
}


bool isECatStarted=false;
bool startECat();
bool stopECat();

const int SegCtxSize=40;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);


int CB_Count=0;

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


uint32_t TMP_HACK_IO_Input0=0;
uint32_t TMP_HACK_IO_Input1=0;
uint32_t TMP_HACK_IO_Output0=0;
uint32_t TMP_HACK_IO_Output1=0;

StaticJsonDocument <1024>doc;
StaticJsonDocument  <500>retdoc;

class MData_JR:public Data_JsonRaw_Layer
{
  
  public:
  MData_JR():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {
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
  int async_printf(int resource_ask_retry,const char *fmt, ...);
  
  int dbg_DIRECT_PRINT(const char *fmt, ...)
  {
    char *str=dbgBuff;
    int restL=sizeof(dbgBuff);

    {
      va_list aptr;
      int ret;
      va_start(aptr, fmt);
      ret = vsnprintf (str, restL, fmt, aptr);
      va_end(aptr); 
      str+=ret;
      restL-=ret;


    }
    return Serial.write((uint8_t*)dbgBuff,str-dbgBuff);
    // return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
    
  }
  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


};
MData_JR djrl;



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



int MData_JR::send_data(int head_room,uint8_t *data,int len,int leg_room){
  Serial.write(data,len);
  Serial.write("\n",1);
  return 0;
}

int MData_JR::dbg_printf(const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"d\":\"");
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

int MData_JR::async_printf(int resource_ask_retry,const char *fmt, ...)
{
  
  Mstp2CommInfo* Qhead=NULL;
  while(resource_ask_retry>=0 && (Qhead=Mstp2CommInfoQ.getHead()) ==NULL)resource_ask_retry--;
  if(Qhead==NULL)return -1;

  Qhead->type=Mstp2CommInfo_Type::ext_log;


  char *str=Qhead->strinfo;
  int restL=sizeof(Qhead->strinfo);

  // {//start head
  //   int len=sprintf(str,"{\"d\":\"");
  //   str+=len;
  //   restL-=len;

  // }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  // {//end
  //   int len=sprintf(str,"\"}");
  //   str+=len;
  //   restL-=len;
  // }




  Mstp2CommInfoQ.pushHead();



  return 0;
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






void G_LOG(const char* str)
{
  djrl.dbg_DIRECT_PRINT(str);
}



typedef enum { 

  NOP= -899999999, 


  LINE_EMPTY= 9001,
  LINE_INCOMPLETE= 9000,


  TASK_WARN_OK= 100, 
  TASK_WAIT_HOLD_RSP= 10, 
  TASK_OK_HOLD_RSP= 1, 
  TASK_OK= 0, 
  TASK_FAILED= -1, 

  TASK_UNSUPPORTED= -3000, 


  PARAM_PARSE_ERROR= -6000, 
  TASK_FATAL_FAILED= -9000, 

} MCMD_Status;


std::array<std::string,32> camTriggerTagArr;


class XY_Coord_Conv
{

  std::array<float,3> device_pt1;
  std::array<float,3> domain_pt1;


  std::array<float,3> device_pt2;
  std::array<float,3> domain_pt2;

  public:
  XY_Coord_Conv()
  {
    resetMapping();

  }

  
  virtual int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    
    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }
    const char* cmd_type = cmd["cmd"];

    
    if(strcmp(cmd_type,"Domain and Device XY points pair")==0)
    {
      //check if all the required fields are there
      if( !cmd["device_pt1"].is<JsonObject>() ||
          !cmd["domain_pt1"].is<JsonObject>() ||
          !cmd["device_pt2"].is<JsonObject>() ||
          !cmd["domain_pt2"].is<JsonObject>()  )
      {
        return MCMD_Status::TASK_FAILED;
      }
      //{"PlatPt1":{"x":793.8087655075475,"y":1410.0607219746882},
      // "ArmPt1":{"x":66.08000183,"y":58.90000153,"z":-27.70000458},
      // "PlatPt2":{"x":1286.467497695725,"y":733.4756994119911},
      // "ArmPt2":{"x":119.2210007,"y":132.0590057,"z":-27.10000038}}
      //set device_pt1
      device_pt1[0]=cmd["device_pt1"]["x"].as<float>();
      device_pt1[1]=cmd["device_pt1"]["y"].as<float>();
      device_pt1[2]=cmd["device_pt1"]["z"].as<float>();

      //set domain_pt1
      domain_pt1[0]=cmd["domain_pt1"]["x"].as<float>();
      domain_pt1[1]=cmd["domain_pt1"]["y"].as<float>();
      domain_pt1[2]=cmd["domain_pt1"]["z"].as<float>();

      //set device_pt2
      device_pt2[0]=cmd["device_pt2"]["x"].as<float>();
      device_pt2[1]=cmd["device_pt2"]["y"].as<float>();
      device_pt2[2]=cmd["device_pt2"]["z"].as<float>();

      //set domain_pt2
      domain_pt2[0]=cmd["domain_pt2"]["x"].as<float>();
      domain_pt2[1]=cmd["domain_pt2"]["y"].as<float>();
      domain_pt2[2]=cmd["domain_pt2"]["z"].as<float>();



      return MCMD_Status::TASK_OK;
    }
    return MCMD_Status::TASK_UNSUPPORTED;
  }

  virtual void resetMapping()
  {
    domain_pt1[0]=domain_pt1[1]=domain_pt1[2]=
    device_pt1[0]=device_pt1[1]=device_pt1[2]=0;


    domain_pt2[0]=domain_pt2[1]=domain_pt2[2]=
    device_pt2[0]=device_pt2[1]=device_pt2[2]=1;//default 1 to 1 mapping

  }


  virtual void coord_dev2dom(float *dom_corrd_dst,const float* dev_corrd_src)
  {
    for(int i=0;i<3;i++)
    {
      dom_corrd_dst[i]=
        domain_pt1[i]+
        (domain_pt2[i]-domain_pt1[i])/(device_pt2[i]-device_pt1[i])*(dev_corrd_src[i]-device_pt1[i]);
    }

  } 


  
  virtual void coord_dom2dev(float *dev_corrd_dst,const float* dom_corrd_src)
  {
    for(int i=0;i<3;i++)
    {
      dev_corrd_dst[i]=
        device_pt1[i]+
        (device_pt2[i]-device_pt1[i])/(domain_pt2[i]-domain_pt1[i])*(dom_corrd_src[i]-domain_pt1[i]); 
    }
  } 
};





template<int N>
class Motor_run_data
{
  
  typedef StpGroup<N,1> StpGroupN_1;
  
  public:
  typedef typename StpGroupN_1::CMDVec Vec; // X Y Z
  typedef typename StpGroupN_1::CMDiVec Vec_i; // Use typename for dependent types




  MSTP_segment_extra_info speed_exinfo={.speed=100,.acc=100,.deacc=100,.cornorR=0};

  //DATA flow
  //CMD from G code etc...
  //latestCMDLocation   = (planner)  > 
  //latestRunLocation = (adjust/tracking...) > 
  //latestRunLocation_post  = (inverse) > 
  //latestMotLoc  

  // motor adv steps:
  // MOTOR_TO_ADVANCING =around(latestMotLoc)-preMotLoc
  // preMotLoc=around(latestMotLoc)



  Vec latestCMDLocation={{0}};

  Vec latestRunLocation={{0}};
  Vec latestRunLocation_post={{0}};

  Vec latestMotLoc={{0}};
  Vec_i vec_round(const Vec &v)
  {
    Vec_i ret={0};
    for(int i=0;i<StpGroupN_1::vec_dim;i++)
    {
      ret.vec[i]=(int)v.vec[i];
    }
    return ret;
  }
  
  Vec_i preMotLoc;


  public:

  void RESET()
  {
    latestMotLoc={0};

    forward(latestRunLocation_post,latestMotLoc);


    latestCMDLocation=
    latestRunLocation=
    latestRunLocation_post;

    preMotLoc=vec_round(latestMotLoc);

  }


  const Vec_i Motor_Pulse_Adv_Update(const Vec_i &to_mot_pulse)
  {
    Vec_i tmp_adv_vec;
    for(int i=0;i<StpGroupN_1::vec_dim;i++)
    {
      tmp_adv_vec.vec[i]=to_mot_pulse.vec[i]-preMotLoc.vec[i];
    }
    preMotLoc=to_mot_pulse;
    return tmp_adv_vec;
  }


  

  const Vec_i Motor_Unit_Adv_Update(const Vec &to_mot_unit)
  {
    latestMotLoc=to_mot_unit;
    Vec to_mot_pulse;
    unit2pulse(to_mot_pulse,to_mot_unit);

    auto to_mot_pulse_i=vec_round(to_mot_pulse);
    return Motor_Pulse_Adv_Update(to_mot_pulse_i);
  }

  const Vec_i Loc_Adv_Update(const Vec &to_loc)
  {
    
    inverse(latestMotLoc,to_loc);
    return Motor_Unit_Adv_Update(latestMotLoc);
  }
  
  //Corrdinate to motor pulse 
  //  xyz-(inverse)>angle1,angle2,angle3-(unit2pulse)> pulse0,pulse1,pulse2

  virtual void inverse(Vec &mot_vec_dst,const Vec &loc_vec_src){//no conversion
    mot_vec_dst=loc_vec_src;
  }
  
  
  virtual void unit2pulse(Vec &pulse_vec_dst,const Vec &unit_vec_src){//no conversion
    pulse_vec_dst=unit_vec_src;
  }
  



  //motor pulse to coordinate
  //  pulse0,pulse1,pulse2-(pulse2unit)> angle1,angle2,angle3-(forward)> xyz
  

  
  virtual void pulse2unit(Vec &unit_vec_dst,const Vec &pulse_vec_src){
    unit_vec_dst=pulse_vec_src;
  }
  virtual void forward(Vec & loc_vec_dst,const Vec &mot_vec_src)
  {
    loc_vec_dst=mot_vec_src;
  }

};


class Motor_run_data_1Axis: public Motor_run_data<1>
{
  typedef StpGroup<1,1> StpGroupN_1;
  float UNIT_TO_PULSE_FACTOR=1.0f;
  public:
  typedef typename Motor_run_data<1>::Vec Vec; // X Y Z
  typedef typename Motor_run_data<1>::Vec_i Vec_i; // Use typename for dependent types

  Motor_run_data_1Axis(float unit_to_pulse_factor=1.0f): Motor_run_data<1>(),UNIT_TO_PULSE_FACTOR(unit_to_pulse_factor)
  {
    
  }
  
  void unit2pulse(Vec &pulse_vec_dst,const Vec &unit_vec_src) override {
    pulse_vec_dst.vec[0]=unit_vec_src.vec[0]*UNIT_TO_PULSE_FACTOR;
  }
  void pulse2unit(Vec &unit_vec_dst,const Vec &pulse_vec_src) override {
    unit_vec_dst.vec[0]=pulse_vec_src.vec[0]/UNIT_TO_PULSE_FACTOR;
  }
};





MotorCtrlCiA402_position_deltaB3 motorPPU(PPU_ECAT_STATION_ID,-1,30*60,updatePeriod_s*1000000000);

MotorCtrlCiA402_speedControled_FeedBackStepper_position motorPLATE(PLAT_ECAT_STATION_ID,0,100000,updatePeriod_s*1000000000);
  
MotorCtrlCiA402_FeedBackStepper_position 
    motorR(ARM_ECAT_STATION_ID,0,30*60,updatePeriod_s*1000000000),
    motorZ(ARM_ECAT_STATION_ID,1,30*60,updatePeriod_s*1000000000),
    motorV(ARM_ECAT_STATION_ID,2,30*60,updatePeriod_s*1000000000);

  
MotorCtrlCiA402_FeedBackStepper_position 
    motorS1(PLAT_ECAT_STATION_ID,2,30*60,updatePeriod_s*1000000000),
    motorReelAdv(PLAT_ECAT_STATION_ID,1,30*60,updatePeriod_s*1000000000);

class MotionGroup_PPU:public XY_Coord_Conv
{ 
  protected:
  typedef StpGroup<1,5> StpGroup1_5;
  StpGroup1_5 MotorGroup;


  Motor_run_data_1Axis ppu_run_data;
  public:
  

  bool homedFlag=false;
  int homingCMDID=-1;

  void motor_attach(EthercatMaster &master)
  {
    motorPPU.attach(master);
  }
  void motor_detach()
  {
    motorPPU.detach();
  }
  void motor_enable(bool en=true)
  {
    homedFlag=true;
    motorMonitor_enableRecord=en;
    motorPPU.enable(en);
  }
  


  uint32_t getlimitSensor()//on motor digital IO is not working, use IO module instead
  {
    // return TMP_HACK_IO_Input0;

    return motorPPU.getDigitalInput();


  }

  // StpGroup1_5::CMDVec latestCMDLocation={{0}};//updated by the latest command
  // StpGroup1_5::CMDVec latestRunLocation={{0}};//updated by update(), every update, the segAdvance will give a new progress/location for this tick  

  // StpGroup1_5::CMDiVec preMotloc_pls={0};

  float progressiveOffset=NAN;
  void RESET_Queue()
  {
    progressiveOffset=NAN;
    homedFlag=false;
    MotorGroup.RESET();

    ppu_run_data.RESET();


    homingSeq.resize(0);
  }
  
  enum HomingStatus{
    HNOP=0,
    Start,
    Back,
    Forward,
    FineBack,
    End
  };

  struct HomingSeq{
    HomingStatus status;
    int axisIdx;
    float speed;
    float speed_fine;
    int sensorPin;

    int sensorSense;
  };


  std::vector<HomingSeq> homingSeq;
  int latest_homestate=-1;
  int counterddd=0;
  uint32_t CCCx=0;
  void homingACT()
  {



    static int ccc=0;
    HomingSeq &hP=homingSeq[0];
    uint32_t sensorReading=getlimitSensor();
    float T=updatePeriod_s;
    counterddd++;
    switch (hP.status) 
    {
    case Start:
      hP.status=Back;
      break;
    case Back:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=Forward;
          ccc=0;
        }
        else
        {

          ppu_run_data.latestMotLoc.vec[hP.axisIdx]+=hP.speed*T;
        }
      break;

    case Forward:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)!=hP.sensorSense)
        {
          if(ccc++<200)//overshoot slowly
          {
            ppu_run_data.latestMotLoc.vec[hP.axisIdx]-=hP.speed*T;
          }
          else
          {
            hP.status=FineBack;
          }
        }
        else
        {
          ppu_run_data.latestMotLoc.vec[hP.axisIdx]-=hP.speed*T;
        }
      break;

    case FineBack:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=End;
        }
        else
        {
          ppu_run_data.latestMotLoc.vec[hP.axisIdx]+=hP.speed_fine*T;
        }
      break;


    case HNOP:
    case End:

      
      ppu_run_data.latestMotLoc.vec[hP.axisIdx]=0;
      ppu_run_data.preMotLoc.vec[hP.axisIdx]=0;

      homingSeq.erase(homingSeq.begin());//remove the first element
      break;
    }
    latest_homestate=hP.status;

  }

  void EM_STOP()
  {
    motorPPU.enable(false);
    RESET_Queue();

  }

  float progressiveProgress=0;
  uint16_t DO_pdo_reg=0;
  
  int dbgCounter2=0;
  float softLimit_min=NAN;
  float softLimit_max=NAN;
  void update()//every system tick, update the location
  {
    // R_Enc=(int32_t)((uint32_t)(motorR.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    // Z_Enc=(int32_t)((uint32_t)(motorZ.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    // V_Enc=(int32_t)((uint32_t)(motorV.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    


    // CCCx=motorPPU.motdrv.pdoReadOutput32(0x60FD);
    CCCx=motorPPU.motdrv.driveGetDigitalInputs();

    // DO_pdo_reg=CCCx>>16;
    do{
      if(homingCMDID!=-1)
      {
        if(homingSeq.size()!=0)
        {

          homingACT();
          if(homingSeq.size()==0)
          {

            // ppu_run_data.unit2pulse(ppu_run_data.preMotLoc,curMotLoc_unit);
            ppu_run_data.forward(ppu_run_data.latestRunLocation,ppu_run_data.latestMotLoc);
            ppu_run_data.latestRunLocation_post=
            ppu_run_data.latestCMDLocation=ppu_run_data.latestRunLocation;

            struct Mstp2CommInfo tinfo={
            .type=Mstp2CommInfo_Type::respFrame,
            .isAck=true,
            .resp_id=homingCMDID
            };
            sprintf(tinfo.strinfo,"PPU home done:%.3f",ppu_run_data.latestRunLocation.vec[0]);
            Mstp2CommInfo* Qhead=NULL;
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
            *Qhead=tinfo;
            Mstp2CommInfoQ.pushHead();
            homingCMDID=-1;
            homedFlag=true;

          }
        }
        break;
      }
      
      if(homedFlag==false)break;//


      int latestSegID=-1;
      float latestSegRatio=-1;


      float T=0;
      float timeLeft=updatePeriod_s;

      while(T!=timeLeft)//keep update until the time is used up
      {
        timeLeft-=T;
        T=timeLeft;
        StpGroup1_5::MSTP_segment*curSeg= MotorGroup.segAdvance(T);//The T will be updated to the time that used

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
        
        latestSegRatio=-1;
        if(curSeg==NULL)
        {
          
          latestSegID=-1;
          // adv_info.dstanceWent=0;
          break;
        }

        latestSegID=curSeg->id;

        if(curSeg->type==MSTP_segment_type::seg_wait)
        {
          if(curSeg->ctx!=NULL)
          {
            MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
            if(p_res->type==MSTP_SegCtx_TYPE::SEG_HALT )
            {
              if(p_res->SEG_HALT.haltFlag ==false)//isInSegHoldState is released by the user command
              {
                // djrl.async_printf(0,"haltFlag==false");
                MotorGroup.adv_info.dstanceWent=curSeg->distanceEnd=-1;//this will end the seg_wait, next update will get a new seg
              }
              else
              {
                MotorGroup.adv_info.dstanceWent=0;
              }
            }
          }

          break;

        }
        else if(curSeg->type==MSTP_segment_type::seg_line || curSeg->type==MSTP_segment_type::seg_arc)//linear interpolation
        {
          bool ret=MotorGroup.getSegLocation(curSeg, ppu_run_data.latestRunLocation);
          
    
        }


      }

    }while(0);
    dbgCounter2++;
    ppu_run_data.latestRunLocation_post=ppu_run_data.latestRunLocation;
    if(progressiveOffset==progressiveOffset)
    {
      ppu_run_data.latestRunLocation_post.vec[0]+=progressiveProgress*progressiveOffset;
    }

    // if(ppu_run_data.latestRunLocation_post.vec[0]>softLimit_max || 
    //   ppu_run_data.latestRunLocation_post.vec[0]<softLimit_min
    // )//if limit min max is NAN then, ignore check
    // {
    //   djrl.async_printf(0,"EM_STOP:soft limit hit %0.3f>%0.3f:%0.3f>%0.3f",
    //   softLimit_max,
    //   latestRunLocation.vec[0],
    //   ppu_run_data.latestRunLocation_post.vec[0],
    //   softLimit_min);
    //   EM_STOP();
    // }

    if(homedFlag==false)return;//

  }

      
  MSTP_axisSetup _axisSetup[1];
      
  std::vector<int32_t>mot_step_vec;

  MotionGroup_PPU():XY_Coord_Conv(),
   mot_step_vec(1),ppu_run_data(6400/80)
  {



    


    ppu_run_data.speed_exinfo={
      .speed=7.0,
      .acc=10.0,
      .deacc=-10.0,
      .cornorR=0,

    };

    // motorV.encoder_check=false;
    /*
    
    axis unit
    axis1: R unit deg
    axis2: Z unit mm
    axis3: V unit mm
    */
   
    MotorGroup.axisSetup[0].P_PreMult=1;
    MotorGroup.axisSetup[0].A_Factor=1;
    MotorGroup.axisSetup[0].V_Factor=1;
    MotorGroup.axisSetup[0].MaxVJump=1;
    MotorGroup.axisSetup[0].V_Max=10000;
    MotorGroup.axisSetup[0].A_Max=200000;





    //to make each speed and acc even, we need to adjust the R speed V_factor x2*PI*R/360deg




    MotorGroup.adv_info.minSpeed=1;


    bool DBG_allowSkipHoming=false;

    if(DBG_allowSkipHoming)
    {
      // {//set homing actual location
      //   curMotLoc_unit.vec[0]=ArmHomingAngle;//R
      //   curMotLoc_unit.vec[1]=20;//Z
      //   curMotLoc_unit.vec[2]=-20;//X
      //   motUnit2PulseConversion(preMotloc_pls.vec,curMotLoc_unit.vec);
      // }

      // forward(latestRunLocation.vec,curMotLoc_unit.vec);
      // latestCMDLocation=latestRunLocation;

      homedFlag=true;

    }


    // design_lowpass_filter(10,1000);
  }







  virtual const std::vector<int32_t>& getMotMoveVec()
  { 

    if(homingCMDID==-1 && homedFlag==true)//motor position was drived by latestRunLocation_postTracking
    {
      // xVec_f curMotLoc_unit={0};
      ppu_run_data.inverse(ppu_run_data.latestMotLoc,ppu_run_data.latestRunLocation_post);
    }
    else//in homing mode, we control the motor directly
    {

    }

    
    auto adv_vec=ppu_run_data.Motor_Unit_Adv_Update(ppu_run_data.latestMotLoc);

    mot_step_vec[0]=adv_vec.vec[0];
    return mot_step_vec;
  }






  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    {
      auto ret = XY_Coord_Conv::CMDProcess(cmd,cmd_ret);
      if(ret!=MCMD_Status::TASK_UNSUPPORTED)
      {
        return ret;
      }
    }



    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }
    const char* type = cmd["cmd"];


    int HACK_LATEST_CMD_ID= cmd["id"];

    if(strcmp(type,"G1")==0)//X
    {

      auto vec_coord=ppu_run_data.latestCMDLocation;
      vec_coord.vec[0]=jgetNum(cmd,"X",vec_coord.vec[0]);

      


      MSTP_segment_extra_info exinfo = ppu_run_data.speed_exinfo;
      exinfo.acc=jgetNum(cmd,"ACC",exinfo.acc);
      exinfo.deacc=jgetNum(cmd,"DEA",exinfo.deacc);
      exinfo.speed=jgetNum(cmd,"F",exinfo.speed);
      ppu_run_data.speed_exinfo=exinfo;


      auto moveVec=MotorGroup.sub(vec_coord,ppu_run_data.latestCMDLocation);
      MotorGroup.pushInMoveVec(HACK_LATEST_CMD_ID,moveVec,ppu_run_data.latestCMDLocation,exinfo,NULL,NULL,NULL);
      ppu_run_data.latestCMDLocation=vec_coord;
      return  MCMD_Status::TASK_OK;


    }
    if(strcmp(type,"G28")==0)//X
    {

      homingSeq.clear();
      float fineSpeed=jgetNum(cmd,"s",0.1);


      float speedMult=jgetNum(cmd,"S",10);
      if(speedMult<0)
      {
        speedMult=-speedMult;
      }
      if(fineSpeed<0)
      {
        fineSpeed=-fineSpeed;
      }
     

      homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=0,.speed= speedMult,.speed_fine= fineSpeed,.sensorPin=0,.sensorSense=1});
  

      homingCMDID=HACK_LATEST_CMD_ID;

      return  MCMD_Status::TASK_OK_HOLD_RSP;


    }
    
    if(strcmp(type,"setProgressiveOffset")==0 || strcmp(type,"setPO")==0)
    {
      
      if(progressiveProgress!=0)
      {
        retdoc["progress"]=progressiveProgress;
        retdoc["nak_msg"]="need to wait for progress down to 0";
        return MCMD_Status::TASK_FAILED;
      }

      float offset=jgetNum(cmd,"offset");



      if(offset!=offset)//disable
      {
        progressiveOffset=NAN;
        return MCMD_Status::TASK_OK;
      }

      progressiveOffset=offset;

      return MCMD_Status::TASK_OK;

      

    }
    
     if(strcmp(type,"softLimit")==0)
    {
      float max=jgetNum(cmd,"max",NAN);
      float min=jgetNum(cmd,"min",NAN);
      return MCMD_Status::TASK_OK;
    }
    
    retdoc["result"]="TASK_UNSUPPORTED";
    return MCMD_Status::TASK_UNSUPPORTED;


  }
  int dbgCounter=0;
  bool motorEnabled=false;
  #define ABSNUM(num) ((num)<0?(-num):(num))



  void ERROR_EV()
  {

      if(motorEnabled==true)
      {
        motorMonitor_enableRecord=false;
        // djrl.async_printf(0,"Mot disable:%d,%d,%d id:id:%d r:%d",motorR.state,motorZ.state,motorV.state,latestSegID,(int)(latestSegRatio*100));
        // // djrl.async_printf(0,"MV: %d,%d,   %d",motorV.pre_posDiff1,motorV.pre_posDiff2,motorV.enc_diff);

        motorEnabled=false;
        motor_enable(false);
        EM_STOP();
        // const std::vector<int32_t> &motSteps=getMotMoveVec();
      }
  }


  const vector<int32_t>* updateMotor()
  {
    if( motorPPU.state==MotorCtrlCiA402::RunningState::fault )
    {
      ERROR_EV();
    }
    else
    {
      if(motorEnabled==false)motorEnabled=true;
      motorMonitor_enableRecord=true;
      //update motor location
      const std::vector<int32_t> &motSteps=getMotMoveVec();
      dbgCounter++;
      motorPPU.setDO(DO_pdo_reg);

      // motorPPU.update(motSteps[0]);
      return &motSteps;

    }

    return NULL;
  }


};
MotionGroup_PPU MG_PPU;


class MotionGroup_PLATE:public XY_Coord_Conv
{ 
   
  public:

  void motor_attach(EthercatMaster &master)
  {
    motorPLATE.attach(master);
  }
  void motor_detach()
  {
    motorPLATE.detach();
  }
  void motor_enable(bool en=true)
  {
    motorPLATE.enable(en);
  }


  typedef StpGroup<1,5> StpGroup1_5;
  StpGroup1_5 MotorGroup;

  

  // std::array<struct MSTP_segment,1> segsBuffer;


  // MSTP_axisSetup _axisSetup[1];

  StpGroup1_5::CMDVec latestRunLocation={{0}};

  StpGroup1_5::CMDiVec preMotloc_pls={0};

  std::vector<int32_t>mot_step_vec;
public:
  float cx,cy;
  float angleToPlatEnc=NAN;
  MotionGroup_PLATE():XY_Coord_Conv(),mot_step_vec(1)
  {
    // segs.setBuffer(segsBuffer.data(),segsBuffer.size());

  }

  float* getLatestLocation()
  {
    return latestRunLocation.vec;
  }

  //the reducer is 60:1
  float warp_dist=6400*60;

  float tar_speed=0/360;//5 deg/s
  float acc=100/4*6400/360;//5 deg/s
  float cur_speed=0;//5 deg/s

  bool updated=false;


  float cur_swing_mag=0;
  float tar_swing_mag=0;
  float acc_swing_mag=100/4*6400/360;

  float cur_swing_frq=0;
  float tar_swing_frq=0;
  float acc_swing_frq=100/4*6400/360;


  


  struct knockState{
    bool enable;//from cmd thread/exectuion thread
    bool executing;//from exectuion thread


    int runSetIndex;



    int loopCount;
    int runCount;



    bool onState;
    int pin;
  };
  std::vector<int> knock_param;//[loop count1,on time1, off time1,   loop count2,on time2, off time2....

  knockState knock_state={false,false,0,0,true,-1};



  float speedUpdate(float speed,float tar_speed,float acc)
  {

    if(speed<tar_speed)
    {
      speed+=acc*updatePeriod_s;
      if(speed>tar_speed)speed=tar_speed;
    }
    if(speed>tar_speed){
      speed-=acc*updatePeriod_s;
      if(speed<tar_speed)speed=tar_speed;
    }

    return speed;
  }


  void exeKnock()
  {


    if(knock_state.enable && knock_param.size()>=3)
    {
      int arrIdx=knock_state.runSetIndex*3;
      knock_state.executing=true;
      if(arrIdx+2>=knock_param.size())//range error
      {
        knock_state.runSetIndex=0;
        arrIdx=0;
      }

      int onTime=knock_param[arrIdx+1];
      int offTime=knock_param[arrIdx+2];

      if(knock_state.onState==true)
      {
        if(knock_state.runCount<onTime)
        {
          TMP_HACK_IO_Output0|=1<<knock_state.pin;
        }
        else
        {
          knock_state.runCount=0;
          knock_state.onState=false;
        }
      }

      if(knock_state.onState==false)
      {
        if(knock_state.runCount<offTime)
        {
          TMP_HACK_IO_Output0&=~(1<<knock_state.pin);
        }
        else
        {
          knock_state.runCount=0;
          knock_state.onState=true;

          
          int loopNumber=knock_param[arrIdx];
          if(knock_state.loopCount<loopNumber)
          {
            knock_state.loopCount++;
          }
          else    
          {
            knock_state.loopCount=0;
            knock_state.runSetIndex++;
          }
        }
      }

      if( (knock_state.runSetIndex*3+2) >=knock_param.size())
      {
        knock_state.runSetIndex=0;
      }
      knock_state.runCount++;

    }
    else
    {
      knock_state.executing=false;
    }





  }




  float accTime=0;
  void update()//every system tick, update the location
  {

    exeKnock();




    if(motorPLATE.state!=MotorCtrlCiA402::RunningState::running)
    {
      motorPLATE.update(0);
      return;
    }




    cur_speed=speedUpdate(cur_speed,tar_speed,acc);


    
    float addonLoc=0;
    if(cur_swing_mag!=0 || tar_swing_mag!=0)
    {
      cur_swing_mag=speedUpdate(cur_swing_mag,tar_swing_mag,acc_swing_mag);
      cur_swing_frq=speedUpdate(cur_swing_frq,tar_swing_frq,acc_swing_frq);
      if(cur_swing_frq>0 && cur_swing_frq<0.01)
      {
        cur_swing_frq=0.01;
      }
      else if(cur_swing_frq<0 && cur_swing_frq>-0.01)
      {
        cur_swing_frq=-0.01;
      }//prevent small frq


      accTime+=updatePeriod_s;
      float swingTime=cur_swing_frq*accTime;

      
      {

        if(swingTime>1)
        {
          swingTime-=1;
          accTime=swingTime/cur_swing_frq;
        }
        else if(swingTime<-1)
        {
          swingTime+=1;
          accTime=swingTime/cur_swing_frq;
        }
      }
      addonLoc=cur_swing_mag*sin(swingTime*2*M_PI);
    }
    else
    {
      accTime=0;
    }

    latestRunLocation.vec[0]+=cur_speed*updatePeriod_s+addonLoc;//in rad









    updated=true;


  }
  int32_t latestGoSteps=0;
  

  const vector<int32_t>* updateMotor()
  {
    const std::vector<int32_t> &plateSteps=getMotMoveVec();
    latestGoSteps=plateSteps[0];
    // motorPLATE.update(plateSteps[0]);
    return &plateSteps;
  }




  void motUnit2PulseConversion(int32_t *pulse_dst,float *unit_src)
  {
    pulse_dst[0]=(int32_t)(unit_src[0]);
  }

  void motPulse2UnitConversion(float *unit_dst,int32_t *pulse_src)
  {
    unit_dst[0]=((float)pulse_src[0]);//R  steps to arcDeg angle
  }



  void inverse(float *mot_vec_dst,const float* loc_vec_src){
    mot_vec_dst[0]=loc_vec_src[0];
  }
  void forward(float* loc_vec_dst,const float *mot_vec_src)
  {
    loc_vec_dst[0]=mot_vec_src[0];
  }
  
  int turnCount=0;
  virtual const std::vector<int32_t>& getMotMoveVec()
  { 
    if(updated==false)return mot_step_vec;
    StpGroup1_5::CMDVec curMotLoc_unit={{0}};
    
    inverse(curMotLoc_unit.vec,latestRunLocation.vec);

    StpGroup1_5::CMDiVec _curMotLoc_pls={0};
    _curMotLoc_pls.vec[0]=(int32_t)(curMotLoc_unit.vec[0]*1600*4.5/(73*2*3.14159));//R  arc angle to steps

    motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);



    mot_step_vec[0]=-(_curMotLoc_pls.vec[0]-preMotloc_pls.vec[0]);//invert polarity

    dbgInfo=mot_step_vec[0];

    bool warp_recalc=false;
    if(latestRunLocation.vec[0]>warp_dist)//warp around
    {
      latestRunLocation.vec[0]-=warp_dist;
      turnCount++;
      warp_recalc=true;
    }
    else if(latestRunLocation.vec[0]<0)//warp around
    {
      latestRunLocation.vec[0]+=warp_dist;
      turnCount--;
      warp_recalc=true;
    }
    if(warp_recalc)
    {
      inverse(curMotLoc_unit.vec,latestRunLocation.vec);
      motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);
    }

    preMotloc_pls=_curMotLoc_pls;

    updated=false;
    return mot_step_vec;
  }

  void convertDomainXYZToObjParam(float* ret_params_in_dev_coord,float x,float y,float z,float angleOnLoc)
  {
    float dom_loc[3]={x,y,z};
    float dev_loc[3];
    coord_dom2dev(dev_loc,dom_loc);
    convertDeviceXYZToObjParam(ret_params_in_dev_coord,dev_loc[0],dev_loc[1],dev_loc[2],angleOnLoc);
  }

  void convertDeviceXYZToObjParam(float* ret_params_in_dev_coord,float x,float y,float z,float angleOnLoc)
  {
    float dev_loc[3]={x,y,z};
    ret_params_in_dev_coord[0]=dev_loc[0];
    ret_params_in_dev_coord[1]=dev_loc[1];
    ret_params_in_dev_coord[2]=dev_loc[2];
    ret_params_in_dev_coord[3]=angleOnLoc;//x y z angle
  }

  void predictCurObj_DomainLocation(float* ret_ObjDomainLoc,float*params_in_dev_coord)
  {
    float curPlate_angle=getLatestLocation()[0];

    predictObj_DomainLocation(ret_ObjDomainLoc,params_in_dev_coord, curPlate_angle);
  }
  
  void predictObj_DomainLocation(float* ret_ObjDomainLoc,float*params_in_dev_coord,float curPlate_angle)
  {
    float x=params_in_dev_coord[0];
    float y=params_in_dev_coord[1];
    float z=params_in_dev_coord[2];
    float OnLoc_angle=params_in_dev_coord[3];

    float angDiff_rad=(curPlate_angle-OnLoc_angle)/warp_dist*(2*(float)M_PI);
    float sinA=sinf(angDiff_rad);
    float cosA=cosf(angDiff_rad);

    float dx=x-cx;
    float dy=y-cy;
    float rotated_x=cx+dx*cosA-dy*sinA;
    float rotated_y=cy+dx*sinA+dy*cosA;


    float devLocation[3]={0};
    devLocation[0]=rotated_x;
    devLocation[1]=rotated_y;
    devLocation[2]=z;

    coord_dev2dom(ret_ObjDomainLoc,devLocation);
  }

  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    {
      auto ret = XY_Coord_Conv::CMDProcess(cmd,cmd_ret);
      if(ret!=MCMD_Status::TASK_UNSUPPORTED)
      {
        return ret;
      }
    }

    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }
    const char* cmd_type = cmd["cmd"];


    if(strcmp(cmd_type,"get_position")==0)
    {
      cmd_ret["position"]=latestRunLocation.vec[0];
      return MCMD_Status::TASK_OK;
    }

    
    if(strcmp(cmd_type,"get_running_param")==0)
    {
      cmd_ret["position"]=latestRunLocation.vec[0];
      cmd_ret["speed"]=tar_speed;
      cmd_ret["warp_dist"]=warp_dist;
      cmd_ret["latestGoSteps"]=latestGoSteps;
      
      return MCMD_Status::TASK_OK;
    }
    
// {"type":"EON"}{"type":"m0","cmd":"set_speed","speed":0.1}
    if(strcmp(cmd_type,"set_speed")==0)
    {
      if(!cmd["speed"].is<float>())
      {
        return MCMD_Status::PARAM_PARSE_ERROR;
      }
      tar_speed=cmd["speed"];

      
      if(cmd["acc"].is<float>())
      {
        acc=cmd["acc"];
      }
      return MCMD_Status::TASK_OK;
    }

    if(strcmp(cmd_type,"set_swing_param")==0)
    {


      tar_swing_mag =jgetNum(cmd,"mag",tar_swing_mag);
      acc_swing_mag =jgetNum(cmd,"mag_acc",acc_swing_mag);


      tar_swing_frq =jgetNum(cmd,"frq",tar_swing_frq);
      acc_swing_frq =jgetNum(cmd,"frq_acc",acc_swing_frq);


      return MCMD_Status::TASK_OK;
    }



    
    if(strcmp(cmd_type,"MACHPARAM")==0)
    {
      /*
      {
        "x": 20.65681712011812,
        "y": 165.864228077559,
        "angleToPlatEnc": 61306.70701034479
      }*/
      if(!cmd["x"].is<float>()||!cmd["y"].is<float>()||!cmd["angleToPlatEnc"].is<float>())
      {
        return MCMD_Status::PARAM_PARSE_ERROR;
      }


      cx=cmd["x"];
      cy=cmd["y"];
      angleToPlatEnc=cmd["angleToPlatEnc"];

      return MCMD_Status::TASK_OK;
    }


    if(strcmp(cmd_type,"set_knock_param")==0)
    {
      
      knock_state.enable=false;

      delay(updatePeriod_s*1000*2);
      if(knock_state.executing==true)
      {
        return MCMD_Status::TASK_FAILED;
      }

      djrl.dbg_printf("set_knock_param");



      knock_state.runSetIndex=0;
      knock_state.loopCount=0;
      knock_state.runCount=0;



      knock_state.onState=true;




      // {"knock_param":[
      //   1,2,3,4,5,6
      // ],
      // "pin":4
      //}
      std::vector<int> _knock_param;
      if(cmd["knock_param"].is<JsonArray>()&& cmd["knock_param"].size()>0)
      {
        for(auto v:cmd["knock_param"].as<JsonArray>())
        {
          if(v.is<int>())
          {
            _knock_param.push_back(v.as<int>());
          }
        }
      }

      knock_param=_knock_param;

      if(cmd["pin"].is<int>())
      {
        knock_state.pin=cmd["pin"].as<int>();
      }


      knock_state.enable=true;
      return MCMD_Status::TASK_OK;
    }

    // if(strcmp(cmd_type,"set_centre")==0)
    // {
    //   if(!cmd["x"].is<float>()||!cmd["y"].is<float>())
    //   {
    //     return MCMD_Status::PARAM_PARSE_ERROR;
    //   }
    //   cx=cmd["x"];
    //   cy=cmd["y"];
    //   return MCMD_Status::TASK_OK;
    // }



    // if(strcmp(cmd_type,"get_centre")==0)
    // {
    //   cmd_ret["x"]=cx;
    //   cmd_ret["y"]=cy;
    //   return MCMD_Status::TASK_OK;
    // }

    cmd_ret["cmd"]=cmd_type;

    
    return MCMD_Status::TASK_UNSUPPORTED;


  }
};

MotionGroup_PLATE MG_PLATE;



int GetPlateObjLocOnArmRZV(float* retLoc, float*params);

uint32_t checkpointInterval=0;
uint32_t checkpointInterval_latest=0;
uint32_t checkpointCounter=0;



EthercatMaster master;  // Create an EtherCAT Master Object
class MotionGroup_SIMP:public XY_Coord_Conv
{ 
  public:
  
  void motor_attach(EthercatMaster &master)
  {
    motorR.attach(master);
    motorZ.attach(master);
    motorV.attach(master);
  }
  void motor_detach()
  {
    motorR.detach();
    motorZ.detach();
    motorV.detach();
  }
  void motor_enable(bool en=true)
  {
    motorMonitor_enableRecord=en;
    motorR.enable(en);
    motorZ.enable(en);
    motorV.enable(en);
  }




  uint32_t checkpointAdeadCounter=0;



  typedef StpGroup<3,50> StpGroup3_50;

  
  StpGroup3_50 MotorGroup;

  typedef StpGroup3_50::MSTP_segment MSTP_segment;

  typedef  StpGroup3_50::CMDVec CMDVec;//X Y Z
  typedef  StpGroup3_50::CMDVec MotVec;//R Z X
  typedef  StpGroup3_50::CMDiVec MotVec_i;



  class Motor_run_data_RZV: public Motor_run_data<3>
  {
    typedef StpGroup<3,1> StpGroupN_1;
    public:
    typedef typename Motor_run_data<3>::Vec Vec; // X Y Z
    typedef typename Motor_run_data<3>::Vec_i Vec_i; // Use typename for dependent types

    Motor_run_data_RZV(): Motor_run_data<3>()
    {
      
    }
    

    int motorPulsePreRev=6400;//  pulse/rev


    void unit2pulse(Vec &pulse_vec_dst,const Vec &unit_vec_src) override {
      pulse_vec_dst.vec[0]=unit_vec_src.vec[0]*motorPulsePreRev/72;//R 360 deg   1/5 timing belt reduction, 1 motor rotation is 1/5 arm rotation = 360/5=72, BTW the radius of R ~150mm
      pulse_vec_dst.vec[1]=unit_vec_src.vec[1]*motorPulsePreRev/60;//Z  mm to steps timing belt 60mm/rev
      pulse_vec_dst.vec[2]=unit_vec_src.vec[2]*motorPulsePreRev/16;//V  mm to steps linear screw rod, 16mm/rev
    }
    void pulse2unit(Vec &unit_vec_dst,const Vec &pulse_vec_src) override {
      unit_vec_dst.vec[0]=pulse_vec_src.vec[0]*72/motorPulsePreRev;//R  steps to arcDeg angle
      unit_vec_dst.vec[1]=pulse_vec_src.vec[1]*60/motorPulsePreRev;//Z  steps to mm
      unit_vec_dst.vec[2]=pulse_vec_src.vec[2]*16/motorPulsePreRev;//V  steps to mm
    }
  };

  
  Motor_run_data_RZV planer_RZV_rdata;





  typedef StpGroup<1,5> StpGroup1_5;
  
  typedef StpGroup1_5::MSTP_segment MSTP_segment1_5;
  StpGroup1_5  planer_S,planer_ReelAdv;

  Motor_run_data_1Axis planer_S_rdata;
  Motor_run_data_1Axis planer_ReelAdv_rdata;



  int motor_FilterBufferSampling(float idx,CMDVec &ret)
  {
    int idx0=(int)idx;
    int idx1=idx0+1;
    
    if(motor_FilterBuffer.size()==0)
    {
      return -1;
    }
    if(idx1>=motor_FilterBuffer.size())
    {
      ret=*motor_FilterBuffer.getTail();

      return 0;
    }
    MotionGroup_SIMP::CMDVec d0=*motor_FilterBuffer.getHead((idx0+1));
    MotionGroup_SIMP::CMDVec d1=*motor_FilterBuffer.getHead((idx1+1));


    ret = MotorGroup.lerp(d0, d1, idx - idx0);
    return 0;
  }

  private:
  // static const int OUTPUT_HIST_DIV=1;
  // RingBuf_Static<RXVec,1024/OUTPUT_HIST_DIV> outputHist;


  //XYZ
  
  // @....o....o....o....o....o....o....@....o....o....o....o....o....o....@
  // A                                  B                                  C
  // latestCMDLocation=A                 =B                                  =C  //A,B C are the location from user command G01 for example
  //AdvLocation is every "o" 


  enum HomingStatus{
    HNOP=0,
    Start,
    Back,
    Forward,
    FineBack,
    End
  };

  struct HomingSeq{
    HomingStatus status;
    int axisIdx;
    float speed;
    float speed_fine;
    int sensorPin;

    int sensorSense;
  };


  //RZX
  //the function getMotMoveVec needs move vector to feed to pulse generator
  //Threrefore, we need a preMotloc to substrace to get motor move diff


  //curMotLoc_unit is a unit based number, could be mm or rad 
  //inverse() and forward() are unit based conversion function

  //curMotLoc_unit is the current location of the motor, should be produced by inverse() from latestRunLocation()
  //curMotLoc_unit is also used by homing process. Every motor do homing independently,
  //so homing precess will touch the curMotLoc_unit directly and bypass the inverse() function
  //(since the motor location is uncertain, the conversion is not valid)


  //preMotloc_pls is a pulse based discrete number, used to prevent float error accumulation
 
  //BTW, when curMotLoc_unit[n]==0, preMotloc_pls[n] should be 0 too, cannot have offset be involved
  


  struct LiveTaskInfo{
    enum{
      RELEASE,
      checkpoint_reply,
      IO_pulse
    }type;
    int cmdID;

      
    union {
      struct {
        int target_checkpoint;
        int respond_delayTime;
      }checkpoint_reply_info;

      struct {
        int IO_bank;
        int IO_pin;
        int delayTime;
      }IO_pulse_info;
    };

  };


  RingBuf_Static<LiveTaskInfo,5> LiveTask;


  /*

  User cmd0 -> latestCMDLocation=cmd0, push to segsBuffer
  User cmd1 -> latestCMDLocation=cmd1, push to segsBuffer


  system tick -> update() -> latestRunLocation=segAdvance() -> curMotLoc_unit=inverse(latestRunLocation) 
              -> getMotMoveVec-> _curMotLoc_pls=convert(curMotLoc_unit )->  pulseGenerator.feed(_curMotLoc_pls-preMotloc_pls) -> preMotloc_pls=_curMotLoc_pls



  */



  //add homing state 

  bool homedFlag=false;
  int homingCMDID=-1;

  
  std::vector<HomingSeq> homingSeq;
  // HomingSeq homingSeq[sizeof(RXVec)/sizeof(float)]={{HNOP,0,0,0,0,0}};

  bool DBG_allowSkipHoming=true;

  struct CTX_TrackingRouteOption{

    bool doBlockFeedOnLoc;
    MotVec BlockFeedOnLoc;


    enum{
      VAD,
      LOC,

    }progress_type;
    union {
      struct {
        MSTP_segment movParam;
      }progress_VAD;
      struct {
        CMDVec start;
        CMDVec end;

        CMDVec vec;
        float distance;

      }progress_LOC;
    };


  };

  struct CTX_TrackingInfo{
    CMDVec Pr;
    int (*PtF)(float* retLoc, float*params);
    float param[5];



    CTX_TrackingRouteOption tro;
    MotionGroup_SIMP *self;
  };

  std::array<ResourcePool<CTX_TrackingInfo>::ResourceData,30> trackingInfoCTXBuffer;


  struct TrackingInfo{

    //there are two sets of tracking targets

    //P=Pc+(Pt0-Pr0)*(1-progress)+(Pt1-Pr1)*progress
    //Pt0=PtF0(param0), Pt1=PtF1(param1)
    //if PtF0==NULL Pt0=Pr0 vice versa


    MSTP_segment_adv_info advInfo;
    // MSTP_segment movParam;
    
    CTX_TrackingRouteOption tro;

    

    //-1 means not in tracking transition mode, a new transition schedule can be set, and the real progress is 0
    // float progress
    float progress;


    CMDVec Pr0;
    int (*PtF0)(float* retLoc, float*params);
    float param0[5];



    CMDVec Pr1;
    int (*PtF1)(float* retLoc, float*params);
    float param1[5];



  };
public:

  ResourcePool <CTX_TrackingInfo>trackingInfoCTX_pool;

  TrackingInfo trackingInfo;

  RingBuf_Static<MotionGroup_SIMP::CMDVec, 100> motor_FilterBuffer;

  MotionGroup_SIMP():XY_Coord_Conv(),trackingInfoCTX_pool(trackingInfoCTXBuffer.data(),trackingInfoCTXBuffer.size()),
  planer_S_rdata(1.0f),planer_ReelAdv_rdata(1.0f) 
  {

    // memset(outputHist.buff,0,outputHist.capacity()*sizeof(RXVec));

    // motorV.encoder_check=false;


    /*
    
    axis unit
    axis1: R unit deg
    axis2: Z unit mm
    axis3: V unit mm
    */
   
    MotorGroup.axisSetup[0].P_PreMult=1;
    MotorGroup.axisSetup[0].A_Factor=1;
    MotorGroup.axisSetup[0].V_Factor=1;
    MotorGroup.axisSetup[0].MaxVJump=1;
    MotorGroup.axisSetup[0].V_Max=10000;
    MotorGroup.axisSetup[0].A_Max=200000;


    planer_S.axisSetup[0]=
    planer_ReelAdv.axisSetup[0]=
    MotorGroup.axisSetup[0];


    MotorGroup.axisSetup[1]=MotorGroup.axisSetup[2]=
    MotorGroup.axisSetup[0];


    MotorGroup.axisSetup[0].V_Max=1800;
    MotorGroup.axisSetup[0].A_Max=30000;

    MotorGroup.axisSetup[1].V_Max=450;
    MotorGroup.axisSetup[1].A_Max=30000;

    // MotorGroup.axisSetup[2].V_Max=220;
    MotorGroup.axisSetup[2].V_Max=180;
    MotorGroup.axisSetup[2].A_Max=4000;


    MotorGroup.axisSetup[1].V_Max=300;
    MotorGroup.axisSetup[1].A_Max=17000;




    //to make each speed and acc even, we need to adjust the R speed V_factor x2*PI*R/360deg
    MotorGroup.axisSetup[0].P_PreMult=300*M_PI/360;//R is 150mm



    MotorGroup.adv_info.minSpeed=5;


    if(DBG_allowSkipHoming)
    {
      // {//set homing actual location
      //   curMotLoc_unit.vec[0]=ArmHomingAngle;//R
      //   curMotLoc_unit.vec[1]=20;//Z
      //   curMotLoc_unit.vec[2]=-20;//X
      //   motUnit2PulseConversion(preMotloc_pls.vec,curMotLoc_unit.vec);
      // }

      // forward(latestRunLocation.vec,curMotLoc_unit.vec);
      // latestCMDLocation=latestRunLocation;

      homedFlag=true;

    }


    trackingInfo.progress=-1;
    trackingInfo.PtF0=NULL;
    trackingInfo.PtF1=NULL;

    trackingInfo.advInfo.minSpeed=5;
    trackingInfo.advInfo.magicSpace=0.1;

    // design_lowpass_filter(10,1000);
  }


  uint32_t preCount=0;
  uint32_t updateCount=0;




  float XVecAngle=46*3.14159/180;
  float ArmHomingAngle=(120)*3.14159/180;
  float XVecX=cosf(XVecAngle), XVecY=sinf(XVecAngle),Arm_r=178;



  uint32_t getlimitSensor()
  {
    return 
    ((motorV.getDigitalInput())<<2)|
    ((motorZ.getDigitalInput())<<1)|
    (motorR.getDigitalInput());
  }



  void homingACT()
  {



    static int ccc=0;
    HomingSeq &hP=homingSeq[0];
    uint32_t sensorReading=getlimitSensor();
    float T=updatePeriod_s;
    switch (hP.status) 
    {
    case Start:
      hP.status=Back;
      break;
    case Back:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=Forward;
          ccc=0;
        }
        else
        {
          
          planer_RZV_rdata.latestMotLoc.vec[hP.axisIdx]+=hP.speed*T;
        }
      break;

    case Forward:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)!=hP.sensorSense)
        {
          if(ccc++<200)//overshoot slowly
          {
            planer_RZV_rdata.latestMotLoc.vec[hP.axisIdx]-=hP.speed_fine*T;
          }
          else
          {
            hP.status=FineBack;
          }
        }
        else
        {
          planer_RZV_rdata.latestMotLoc.vec[hP.axisIdx]-=hP.speed*T;
        }
      break;

    case FineBack:
        // if(ccc--<0)
        if(((sensorReading>>hP.sensorPin)&1)==hP.sensorSense)
        {
          hP.status=End;
        }
        else
        {
          planer_RZV_rdata.latestMotLoc.vec[hP.axisIdx]+=hP.speed_fine*T;
        }
      break;


    case HNOP:
    case End:


      planer_RZV_rdata.latestMotLoc.vec[hP.axisIdx]=0;
      planer_RZV_rdata.preMotLoc.vec[hP.axisIdx]=0;
      


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

  bool HACK_segComRequest=false;//Hack try to avoid timer thread and cmd thread conflict on segsBuffer
  
  int32_t R_Enc,Z_Enc,V_Enc;

  int latestSegID=-1;
  void update()//every system tick, update the location
  {
    checkpointInterval++;
    // R_Enc=(int32_t)((uint32_t)(motorR.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    // Z_Enc=(int32_t)((uint32_t)(motorZ.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    // V_Enc=(int32_t)((uint32_t)(motorV.motdrv.driveGetAdditionalActualPosition(0)) << 1) / 2;
    
    if(
      motorR.state!=MotorCtrlCiA402::RunningState::running || 
      motorZ.state!=MotorCtrlCiA402::RunningState::running ||
      motorV.state!=MotorCtrlCiA402::RunningState::running   )
    {
      motorR.update(0);
      motorZ.update(0);
      motorV.update(0);
      
      //deplete seg Queue
      RESET_Queue();
      HACK_segComRequest=false;
      return;
    }

    updateCount=(updateCount+1)%100000;



    
    size_t LiveTask_size=LiveTask.size();
    if(LiveTask_size)
    {

      for(int i=0;i<LiveTask_size;i++)
      {
        auto* task=LiveTask.getTail(i);
        if(task==NULL)break;
        switch(task->type)
        {
          case LiveTaskInfo::checkpoint_reply:
          {
            if(checkpointCounter>=task->checkpoint_reply_info.target_checkpoint)
            {
              if(task->checkpoint_reply_info.respond_delayTime<=0 || checkpointCounter>task->checkpoint_reply_info.target_checkpoint)
              {

                struct Mstp2CommInfo tinfo={};
                tinfo.type=Mstp2CommInfo_Type::respCheckpointFrame;

                tinfo.isAck=checkpointCounter==task->checkpoint_reply_info.target_checkpoint;
                tinfo.checkPointInterval=checkpointInterval_latest;//just to attach the latest checkpointInterval
                // checkpointInterval_latest;
                tinfo.resp_id=task->cmdID;
                tinfo.segQSpace=MotorGroup.segs.space();
                Mstp2CommInfo* Qhead=NULL;
                while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
                *Qhead=tinfo;
                Mstp2CommInfoQ.pushHead();
                task->type=LiveTaskInfo::RELEASE;//mark deletion
              }


              task->checkpoint_reply_info.respond_delayTime--;


            }
          }
        }

      }


      for(;;)//release completed
      {
        auto* task=LiveTask.getTail();
        if(task==NULL || task->type!=LiveTaskInfo::RELEASE)break;
        LiveTask.consumeTail();
      }
    }
    // CTX_TrackingInfo *ti=TrackingInfoQ.getTail();
    do{
      if(homingCMDID!=-1)
      {
        if(homingSeq.size()!=0)
        {

          homingACT();
          if(homingSeq.size()==0)
          {


            planer_RZV_rdata.RESET();
            planer_RZV_rdata.forward(planer_RZV_rdata.latestRunLocation,planer_RZV_rdata.latestMotLoc);
            planer_RZV_rdata.latestCMDLocation=
            planer_RZV_rdata.latestRunLocation_post=
            planer_RZV_rdata.latestRunLocation;

            motor_FilterBuffer.clear();
            
            struct Mstp2CommInfo tinfo={
            .type=Mstp2CommInfo_Type::respFrame,
            .isAck=true,
            .resp_id=homingCMDID
            };
            sprintf(tinfo.strinfo,"%.3f %.3f %.3f",planer_RZV_rdata.latestRunLocation.vec[0],planer_RZV_rdata.latestRunLocation.vec[1],planer_RZV_rdata.latestRunLocation.vec[2]);
            Mstp2CommInfo* Qhead=NULL;
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
            *Qhead=tinfo;
            Mstp2CommInfoQ.pushHead();
            homingCMDID=-1;
            homedFlag=true;


          }
        }


        break;
      }
      
      if(homedFlag==false)break;//
      

      MG_PPU.progressiveProgress=0;





      {//planer_S
        
        float T=0;
        float timeLeft=updatePeriod_s;
        while(T!=timeLeft)
        {
          timeLeft-=T;
          T=timeLeft;
          auto *curSeg= planer_S.segAdvance(T);

          if(curSeg==NULL)
          {
            break;
          }

          
          
          if(curSeg->type==MSTP_segment_type::seg_wait)
          {
            if(curSeg->ctx!=NULL)
            {
              MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
              if(p_res->type==MSTP_SegCtx_TYPE::SEG_HALT_FOR_CHECKPOINT )
              {

                if(checkpointCounter>=p_res->SEG_HALT_FOR_CHECKPOINT.target_checkpoint)
                {
                  planer_S.adv_info.dstanceWent=curSeg->distanceEnd=-1;//Set dstanceWent and distanceEnd equal and this will end this seg_wait
                }
                else
                {
                  planer_S.adv_info.dstanceWent=0;//reset progress. ie, run forever
                }
              }
            }

            break;
          }
          else if(curSeg->type==MSTP_segment_type::seg_line)//linear interpolation
          {
            bool ret=planer_S.getSegLocation(curSeg,planer_S_rdata.latestRunLocation);
            planer_S_rdata.latestRunLocation_post=planer_S_rdata.latestRunLocation;
      
          }

        }
        
      }



      {//planer_ReelAdv
        
        float T=0;
        float timeLeft=updatePeriod_s;
        while(T!=timeLeft)
        {
          timeLeft-=T;
          T=timeLeft;
          auto *curSeg= planer_ReelAdv.segAdvance(T);

          if(curSeg==NULL)
          {
            break;
          }

          
          
          if(curSeg->type==MSTP_segment_type::seg_wait)
          {
            if(curSeg->ctx!=NULL)
            {
              MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
              if(p_res->type==MSTP_SegCtx_TYPE::SEG_HALT_FOR_CHECKPOINT )
              {

                if(checkpointCounter>=p_res->SEG_HALT_FOR_CHECKPOINT.target_checkpoint)
                {
                  planer_ReelAdv.adv_info.dstanceWent=curSeg->distanceEnd=-1;//Set dstanceWent and distanceEnd equal and this will end this seg_wait
                }
              }
            }
          }
          else if(curSeg->type==MSTP_segment_type::seg_line)//linear interpolation
          {
            bool ret=planer_ReelAdv.getSegLocation(curSeg,planer_ReelAdv_rdata.latestRunLocation);
            planer_ReelAdv_rdata.latestRunLocation_post=planer_ReelAdv_rdata.latestRunLocation;
      
          }

        }
        
      }



      float T=0;
      float timeLeft=updatePeriod_s;

      while(T!=timeLeft)//keep update until the time is used up
      {
        timeLeft-=T;
        T=timeLeft;
        MSTP_segment*curSeg= MotorGroup.segAdvance(T);//The T will be updated to the time that used

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
        
        latestSegRatio=-1;
        if(curSeg==NULL)
        {
          
          latestSegID=-1;
          // adv_info.dstanceWent=0;
          break;
        }

        latestSegID=curSeg->id;

        if(curSeg->type==MSTP_segment_type::seg_wait)
        {
          if(curSeg->ctx!=NULL)
          {
            MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
            if(p_res->type==MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED )
            {
              if(trackingInfo.progress==-1)//sattled
              {
                MotorGroup.adv_info.dstanceWent=curSeg->distanceEnd=-1;//this will end the seg_wait, next update will get a new seg
              }
              else
              {
                MotorGroup.adv_info.dstanceWent=0;
              }
            }
            else if(p_res->type==MSTP_SegCtx_TYPE::SEG_HALT )
            {
              if(p_res->SEG_HALT.haltFlag ==false)//isInSegHoldState is released by the user command
              {
                // djrl.async_printf(0,"haltFlag==false");
                MotorGroup.adv_info.dstanceWent=curSeg->distanceEnd=-1;//this will end the seg_wait, next update will get a new seg
              }
              else
              {
                MotorGroup.adv_info.dstanceWent=0;
              }
            }
          }

          break;

        }
        else if(curSeg->type==MSTP_segment_type::seg_line || curSeg->type==MSTP_segment_type::seg_arc)//linear interpolation
        {
          bool ret=MotorGroup.getSegLocation(curSeg,planer_RZV_rdata.latestRunLocation);
    
        }


      }

    }while(0);



    HACK_segComRequest=false;
    if(homedFlag==false)return;//

    planer_RZV_rdata.latestRunLocation_post=planer_RZV_rdata.latestRunLocation;



    static int pptt=0;
    {//tracking process
      //P=Pc+(Pt0-Pr0)*(1-progress)+(Pt1-Pr1)*progress
      //progress goes from 0 to 1
      //when progress=0, P=Pc+(Pt0-Pr0)
      //when progress goes up
      //when progress=1, P=Pc+(Pt1-Pr1) 
      // AND, set progress=0, Pt0=Pt1 Pr0=Pr1
      // in practice progress=-1, meaning no tracking in progress, a new tracking schedule can be set by CMD

      bool justFinish=false;
      static int preProgress=0;
      if(preProgress==-1 && trackingInfo.progress!=preProgress)
      {
        preProgress=trackingInfo.progress;
        pptt=0;
      }
      if(trackingInfo.progress>=0)//in tracking progress
      {//advancing the progress


        float T = updatePeriod_s;


        float progress=0;


        if(trackingInfo.tro.progress_type==CTX_TrackingRouteOption::VAD){

          StpGroup3_50::MSTP_segment_adv_state  st=StpGroup3_50::segAdvance(T,&trackingInfo.tro.progress_VAD.movParam,&trackingInfo.advInfo);

          dbgInfo.progress_count++;

          dbgInfo.advSt=st;
          progress=trackingInfo.advInfo.dstanceWent/trackingInfo.tro.progress_VAD.movParam.distanceEnd;
          dbgInfo.distEnd=trackingInfo.tro.progress_VAD.movParam.distanceEnd;
          dbgInfo.distWent=trackingInfo.advInfo.dstanceWent;
          dbgInfo.distStart=trackingInfo.tro.progress_VAD.movParam.distanceStart;

        }
        else if (trackingInfo.tro.progress_type==CTX_TrackingRouteOption::LOC)
        {
          MotVec curVec={
            planer_RZV_rdata.latestRunLocation_post.vec[0]-trackingInfo.tro.progress_LOC.start.vec[0],
            0,
            planer_RZV_rdata.latestRunLocation_post.vec[2]-trackingInfo.tro.progress_LOC.start.vec[2]};



          float dotProduct=
            curVec.vec[0]*trackingInfo.tro.progress_LOC.vec.vec[0]+
            curVec.vec[2]*trackingInfo.tro.progress_LOC.vec.vec[2];
          progress=dotProduct/trackingInfo.tro.progress_LOC.distance;

          

          if(pptt==0)
          {
            djrl.async_printf(0,"progress:%0.3f dp:%0.3f  pt:%0.3f,%0.3f,",
              progress,
              dotProduct,
              planer_RZV_rdata.latestRunLocation_post.vec[0],
              planer_RZV_rdata.latestRunLocation_post.vec[2]);

            
            djrl.async_printf(0,"pt:%0.3f,%0.3f,",
              trackingInfo.tro.progress_LOC.start.vec[0],
              trackingInfo.tro.progress_LOC.start.vec[2]);
          }

          if(progress<0)progress=0;
          if(progress>0.99999)progress=0.99999;//will not be 1
          MG_PPU.progressiveProgress=progress;
        }

        if(progress>=1)
          djrl.async_printf(0,"%0.3f,%0.3f <= %0.3f,%0.3f",
            planer_RZV_rdata.latestRunLocation_post.vec[0],
            planer_RZV_rdata.latestRunLocation_post.vec[2],
            
            trackingInfo.tro.BlockFeedOnLoc.vec[0],
            trackingInfo.tro.BlockFeedOnLoc.vec[2]);

        if(trackingInfo.tro.doBlockFeedOnLoc)//override the location
        {
          planer_RZV_rdata.latestRunLocation_post.vec[0]=trackingInfo.tro.BlockFeedOnLoc.vec[0];
          planer_RZV_rdata.latestRunLocation_post.vec[2]=trackingInfo.tro.BlockFeedOnLoc.vec[2];
        }





        if(progress>=1)//progress reaches the end
        {

          dbgInfo.progress_reach_count++;
          progress=1;

          //move target 1 to target 0, and set target 1 to NULL
          trackingInfo.Pr0=trackingInfo.Pr1;
          trackingInfo.PtF0=trackingInfo.PtF1;
          memcpy(trackingInfo.param0,trackingInfo.param1,sizeof(trackingInfo.param0));
          trackingInfo.PtF1=NULL;
          justFinish=true;
          progress=-1;


        }
        trackingInfo.progress=progress;
      }


      float trackingProgress=trackingInfo.progress;
      if(trackingProgress<0)
      {
        trackingProgress=0;
      }

      float efDown=1;//?
      if(trackingInfo.PtF0!=NULL)
      {
        dbgInfo.PtF0_count++;
        CMDVec tarLoc0=planer_RZV_rdata.latestRunLocation_post;
        trackingInfo.PtF0(tarLoc0.vec,trackingInfo.param0);

        for(int i=0;i<MotorGroup.vec_dim;i++)
        {
          float diff=(tarLoc0.vec[i]-trackingInfo.Pr0.vec[i]);

          // dbgInfo.loc.vec[i]=diff;
          // dbgInfo.loc.vec[i]=diff;
          planer_RZV_rdata.latestRunLocation_post.vec[i]+=(1-trackingProgress)*diff*efDown;

        }

        if(justFinish)
          djrl.async_printf(0,"jr diff:%.3f,%.3f,%.3f",
            dbgInfo.loc.vec[0],dbgInfo.loc.vec[1],dbgInfo.loc.vec[2]);
      }
      else
      {
        dbgInfo.loc.vec[0]=
          dbgInfo.loc.vec[1]=
          dbgInfo.loc.vec[2]=0;
      }

      if(trackingInfo.PtF1!=NULL)
      {
        dbgInfo.PtF1_count++;
        CMDVec tarLoc1;
        trackingInfo.PtF1(tarLoc1.vec,trackingInfo.param1);

        for(int i=0;i<MotorGroup.vec_dim;i++)
        {

          float diff=(tarLoc1.vec[i]-trackingInfo.Pr1.vec[i]);
          // dbgInfo.loc2.vec[i]=tarLoc1.vec[i];
          // dbgInfo.loc.vec[i]=trackingInfo.Pr1.vec[i];
          // dbgInfo.loc2.vec[i]=latestRunLocation_postTracking.vec[i];
          planer_RZV_rdata.latestRunLocation_post.vec[i]+=(trackingProgress)*diff*efDown;
          

        }


        if(pptt==0)
        {
          djrl.async_printf(0,"prog:%0.3f %0.3f,%0.3f",
            trackingProgress,
            planer_RZV_rdata.latestRunLocation_post.vec[0],
            planer_RZV_rdata.latestRunLocation_post.vec[2]);

          djrl.async_printf(0,"tL:%0.3f,%0.3f pr1:%0.3f,%0.3f",
            tarLoc1.vec[0],
            tarLoc1.vec[2],
            trackingInfo.Pr1.vec[0],
            trackingInfo.Pr1.vec[2]
            
            
            );
        }
      }
      else
      {
        dbgInfo.loc2.vec[0]=
          dbgInfo.loc2.vec[1]=
          dbgInfo.loc2.vec[2]=0;
      }

    }
    //result: latestRunLocation_postTracking

    if(pptt++>500)pptt=0;
    if(0)
    {
      static int printCounter=0;
      if(printCounter==0)
      {
        // djrl.async_printf(0,"l_pt:%.2f,%.2f,%.2f",
        // latestRunLocation_postTracking.vec[0],
        // latestRunLocation_postTracking.vec[1],
        // latestRunLocation_postTracking.vec[2]);
        


        if(trackingInfo.progress>=0)
        {

          djrl.async_printf(0,"tprog:%.3f================",
          trackingInfo.progress);
          // djrl.async_printf(0,"diff1:%.2f,%.2f,%.2f",
          // dbgInfo.loc.vec[0],
          // dbgInfo.loc.vec[1],
          // dbgInfo.loc.vec[2]);
          djrl.async_printf(0,"diff2:%.2f,%.2f,%.2f",
          dbgInfo.loc2.vec[0],
          dbgInfo.loc2.vec[1],
          dbgInfo.loc2.vec[2]);

          
          djrl.async_printf(0,"dbg:%.2f,%.2f,%.2f",
          GetPlateObjLocOnArmRZV_DBG[0],
          GetPlateObjLocOnArmRZV_DBG[1],
          GetPlateObjLocOnArmRZV_DBG[2]);
        }

      }
      printCounter=(printCounter+1)&((1<<9)-1);
    }


    float preR=planer_RZV_rdata.latestRunLocation_post.vec[0];
  
    if(motor_FilterBuffer.space()==0)
    {
      motor_FilterBuffer.consumeTail();
    }
    motor_FilterBuffer.pushHead(planer_RZV_rdata.latestRunLocation_post);
    
    if(ZVIShaper_Idx1>0 && ZVIShaper_Idx2==0)
    {//ZV filter
      CMDVec curLoc=planer_RZV_rdata.latestRunLocation_post;
      CMDVec pt1;
      motor_FilterBufferSampling(ZVIShaper_Idx1,pt1);

      //only filter R:[0]
      planer_RZV_rdata.latestRunLocation_post.vec[0]=(curLoc.vec[0]+pt1.vec[0])/2;
    }

    if(ZVIShaper_Idx1>0 && ZVIShaper_Idx2>0)
    {//ZV filter
      CMDVec curLoc=planer_RZV_rdata.latestRunLocation_post;
      CMDVec pt1;
      motor_FilterBufferSampling(ZVIShaper_Idx1,pt1);
      CMDVec pt2;
      motor_FilterBufferSampling(ZVIShaper_Idx2,pt2);

      //only filter R:[0]
      planer_RZV_rdata.latestRunLocation_post.vec[0]=(curLoc.vec[0]+2*pt1.vec[0]+pt2.vec[0])/4;
    }
    dDiff=planer_RZV_rdata.latestRunLocation_post.vec[0]-preR;
  }

  float dDiff=0;
  float ZVIShaper_Idx1=0;
  float ZVIShaper_Idx2=0;


  


  void ERROR_EV()
  {
    if(motorEnabled==true)
    {
      motorMonitor_enableRecord=false;
      djrl.async_printf(0,"EM_STOP:mot disabled %d,%d,%d id:id:%d r:%d",motorR.state,motorZ.state,motorV.state,latestSegID,(int)(latestSegRatio*100));
      // djrl.async_printf(0,"MV: %d,%d,   %d",motorV.pre_posDiff1,motorV.pre_posDiff2,motorV.enc_diff);

      motorEnabled=false;
      motor_enable(false);
      EM_STOP();
      // const std::vector<int32_t> &motSteps=getMotMoveVec();
    }
  }


  int32_t dbg_maxVStep=0,dbg_maxStepDiff=0;
  int dbg_index=2;
  bool motorEnabled=false;
  #define ABSNUM(num) ((num)<0?(-num):(num))

  const vector<int32_t>* updateMotor()
  {
    if(
      motorR.state==MotorCtrlCiA402::RunningState::disabled || 
    motorZ.state==MotorCtrlCiA402::RunningState::disabled||
    motorV.state==MotorCtrlCiA402::RunningState::disabled)
    {
      
      ERROR_EV();
    }
    else
    {
      if(motorEnabled==false)motorEnabled=true;
      motorMonitor_enableRecord=true;
      //update motor location
      const std::vector<int32_t> &motSteps=getMotMoveVec();



      if(motorMonitor_enableRecord)
      {
        int encDiff=0;
        if(dbg_index==0)
        {
          encDiff=motorR.latest_enc_error;
        }
        else if(dbg_index==1)
        {
          encDiff=motorZ.latest_enc_error;
        }
        else if(dbg_index==2)
        {
          encDiff=motorV.latest_enc_error;
        }
        
        static int skipCounter=0;

        if(skipCounter>=motorMonitor_recordSkipMod)
          skipCounter=0;

        if(motorMonitor_recordCD==0 && skipCounter==0 && motorMonitor.space()>0)
          motorMonitor.pushHead(encDiff);
        skipCounter++;

        if(motorMonitor_recordCD)motorMonitor_recordCD--;
      }


      {
        static int32_t preVStep=0;
        int32_t diff=motSteps[dbg_index]-preVStep;
        preVStep=motSteps[dbg_index];
        if(ABSNUM(dbg_maxStepDiff)<ABSNUM(diff))
        {
          dbg_maxStepDiff=diff;
        }

        if(ABSNUM(dbg_maxVStep)<ABSNUM(motSteps[dbg_index]))
        {
          dbg_maxVStep=motSteps[2];
        }
      }


      return &motSteps;
    }
    return NULL;
  }

  void copyCMDVec(float*dst,float*src)
  {
    memcpy(dst,src,sizeof(CMDVec));
  }


  // std::string vec_to_string(float*dst)
  // {
  //   std::string ret="[";
  //   for(int i=0;i<MotorGroup.vec_dim;i++)
  //   {
  //     ret+=std::to_string(dst[i]);

  //     if(i<MotorGroup.vec_dim-1)
  //       ret+=",";
  //   }

  //   return ret+"]";
  // }


  void RESET_Queue()
  {
    homedFlag=false;
    MotorGroup.RESET();
    planer_S.RESET();
    planer_ReelAdv.RESET();

    homingSeq.resize(0);
    trackingInfo.progress=-1;
    trackingInfo.PtF0=NULL;
    trackingInfo.PtF1=NULL;

    LiveTask.clear();

    checkpointCounter=checkpointAdeadCounter=0;
    checkpointInterval=checkpointInterval_latest=0;
    sctx_pool.RESET_occupation();
    trackingInfoCTX_pool.RESET_occupation();
  }
  void EM_STOP()
  {
    RESET_Queue();

  }


  
  int turnCount=0;
  std::vector<int32_t> mot_vec_dst={0,0,0,0,0};
  virtual const std::vector<int32_t>& getMotMoveVec()
  { 

    if(homingCMDID==-1 && homedFlag==true)//motor position was drived by latestRunLocation_postTracking
    {
      planer_RZV_rdata.inverse(planer_RZV_rdata.latestMotLoc,planer_RZV_rdata.latestRunLocation_post);

    }
    else//in homing mode, we control the motor directly
    {

    }

    auto rzv_loc=planer_RZV_rdata.Motor_Unit_Adv_Update(planer_RZV_rdata.latestMotLoc);
    mot_vec_dst[0]=-rzv_loc.vec[0];//invert polarity
    mot_vec_dst[1]=rzv_loc.vec[1];//Z
    mot_vec_dst[2]=rzv_loc.vec[2];//invert polarity

    {
      auto motAdv=planer_S_rdata.Loc_Adv_Update(planer_S_rdata.latestRunLocation_post);
      mot_vec_dst[3]=motAdv.vec[0];
    }


    {
      auto motAdv=planer_ReelAdv_rdata.Loc_Adv_Update(planer_ReelAdv_rdata.latestRunLocation_post);
      mot_vec_dst[4]=motAdv.vec[0];
    }

    // mot_vec_dst->vec[3]=_curMotLoc_stp.vec[3]-preMotloc.vec[3];
    // mot_vec_dst->vec[4]=_curMotLoc_stp.vec[4]-preMotloc.vec[4];
    return mot_vec_dst;
  }

  // float 
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

  int findIntersection(float vx, float vy, float cx, float cy, float r, float *t1,float *t2) {
      *t1=*t2=NAN;

      float a = vx*vx + vy*vy;
      float b = -2*(cx*vx + cy*vy);
      float c = cx*cx + cy*cy - r*r;
      return solveQuadratic_real(a, b, c, t1, t2);
  }


  // void inverse(float *mot_vec_dst,const float* loc_vec_src){

  //   // for(int i=0;i<MotorGroup.vec_dim;i++)
  //   // {
  //   //   mot_vec_dst->vec[i]=loc_vec_src[i];
  //   // }




  //   float cx=loc_vec_src[0],cy=loc_vec_src[1];

  //   float x1,x2;
  //   volatile int res=findIntersection(XVecX,XVecY,cx,cy,Arm_r,&x1,&x2);



  //   float mx=x1<x2?x1:x2;

  //   float vx=cx-mx*XVecX;
  //   float vy=cy-mx*XVecY;
    
  //   mot_vec_dst[0]=atan2f(vy,vx);//R

  //   mot_vec_dst[1]=loc_vec_src[2];//Z

  //   mot_vec_dst[2]=mx;//X

  //   // volatile int res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
  // }
  // void forward(float* loc_vec_dst,const float *mot_vec_src)
  // {
  //   // for(int i=0;i<MotorGroup.vec_dim;i++)
  //   // {
  //   //   loc_vec_dst[i]=mot_vec_src->vec[i];
  //   // }


  //   float R= mot_vec_src[0];//R
  //   float Z= mot_vec_src[1];//R
  //   float X= mot_vec_src[2];//R
    
  //   float ArmX=cosf(R)*Arm_r+X*XVecX;
  //   float ArmY=sinf(R)*Arm_r+X*XVecY;

    
  //   loc_vec_dst[0]=ArmX;
  //   loc_vec_dst[1]=ArmY;
  //   loc_vec_dst[2]=Z;
  // }


  typedef struct{
    bool inited;
    bool y_axis_inverted;//true for image coordination downward for bigger number
    float linear_vec_x;
    float linear_vec_y;


    float arc_c0_x;
    float arc_c0_y;
    float arc_r;
    float angleOffset;

    
    int calibMap_size;
    float calibMap[4*20];//pre shifted using calibMap_shift_factor
  }ArmParam;

  ArmParam armParam={0};

public:
  bool XYZ2RZV(float *dst,const float* src){
    if(armParam.inited==false)return false;

    
    CMDVec adj_src;
    if(armParam.calibMap_size>0)
    {


      
      float adjX=src[0],adjY=src[1];
      convert_point_f(
         adjX, adjY, 
        &adjX,&adjY,
        0,armParam.calibMap,armParam.calibMap_size);
    


      adj_src.vec[0]=((float)adjX)-armParam.arc_c0_x;
      adj_src.vec[1]=((float)adjY)-armParam.arc_c0_y;
      adj_src.vec[2]=src[2];


    }
    else
    {
      
      adj_src.vec[0]=src[0]-armParam.arc_c0_x;
      adj_src.vec[1]=src[1]-armParam.arc_c0_y;
      adj_src.vec[2]=src[2];
    }

  

    float x1,x2;
    volatile int res=findIntersection(
      armParam.linear_vec_x,
      armParam.linear_vec_y,
      adj_src.vec[0],
      adj_src.vec[1],
      armParam.arc_r,&x1,&x2);


    float mv=x2;
    

//    cmd_ret["_x"]=adj_src.vec[0];
//    cmd_ret["_y"]=adj_src.vec[1];
//    cmd_ret["x2"]=x2;
    float cmdAngle=atan2f(
      adj_src.vec[1]-x2*armParam.linear_vec_y,
      adj_src.vec[0]-x2*armParam.linear_vec_x)
      *180/M_PI;
    
    if(armParam.y_axis_inverted)cmdAngle=-cmdAngle;
    

    // cmd_ret["a.angleOffset"]=armParam.angleOffset;
    // cmd_ret["cmdAngle"]=cmdAngle;

    dst[0]=cmdAngle+armParam.angleOffset;
    dst[1]=adj_src.vec[2];
    dst[2]=mv;

    return true;
  }


  bool RZV2XYZ(float *dst,const float* src){
    if(armParam.inited==false)return false;
    float R_deg=src[0]-armParam.angleOffset;
    float R_rad=R_deg*M_PI/180;
    float Z=src[1];
    float V=src[2];

    float X=cosf(R_rad)*armParam.arc_r+V*armParam.linear_vec_x;
    float Y=sinf(R_rad)*armParam.arc_r+V*armParam.linear_vec_y;

    dst[0]=X+armParam.arc_c0_x;
    dst[1]=Y+armParam.arc_c0_y;
    dst[2]=Z;

    if(armParam.calibMap_size>0)
    {

      
      float oriX_est=dst[0],oriY_est=dst[1];
      convert_point_f(
         oriX_est, oriY_est, 
        &oriX_est,&oriY_est,
        1,armParam.calibMap,armParam.calibMap_size);
    
      dst[0]=(float)oriX_est;
      dst[1]=(float)oriY_est;
    }
    
  
    return true;
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


  static int IOCtrlEndCB( MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=100;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    
    uint32_t *tarOutIOReg=p_res->IO_CTRL.bank==0?&TMP_HACK_IO_Output0:&TMP_HACK_IO_Output1;

    uint32_t latest_pinout=*tarOutIOReg;
    uint32_t mask=p_res->IO_CTRL.mask;
    latest_pinout&=~mask;//clear masked pins 
    latest_pinout|=mask&p_res->IO_CTRL.pout; //put pinout in port

    *tarOutIOReg=latest_pinout;


    if(p_res->IO_CTRL.cid>=0)
    {
      struct Mstp2CommInfo tinfo={
      .type=Mstp2CommInfo_Type::trigInfo,
      
      .trig_id=p_res->IO_CTRL.tid,
      .cam_id=p_res->IO_CTRL.cid,

      };
      char *str=tinfo.strinfo;
      str[0]='\0';
      if(p_res->IO_CTRL.ttagbf)
      {
        auto ttagbf=p_res->IO_CTRL.ttagbf;
        int index=0;
        bool isFirstTag=true;
        while (ttagbf)
        {
          if(ttagbf&1)//index hit
          {
            if(isFirstTag==false)
            {
              str+=sprintf(str,",");
            }
            std::string tag_str=camTriggerTagArr[index];
            char ctag[50];
            strcpy(ctag,tag_str.c_str());
            //check contain with "s_PLAT_LOC="
            if(strstr(ctag,"s_PLAT_LOC=")!=NULL)
            {
              float platLoc=MG_PLATE.getLatestLocation()[0];
              str+=sprintf(str,ctag,platLoc);
            }
            else if(strstr(ctag,"s_SYSTIME=")!=NULL)
            {
              str+=sprintf(str,ctag,SYS_TIME_ms());//unsigned long
            }
            else
            {
              str+=sprintf(str,ctag);
            }

            isFirstTag=false;
          }
          /* code */
          index++;
          ttagbf>>=1;
        }
      }
      Mstp2CommInfo* Qhead=NULL;
      while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);//maybe shouldn't do it here
      *Qhead=tinfo;
      Mstp2CommInfoQ.pushHead();
    }
    
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }

  
  static int BumpCheckPointCounterEndCB( MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    {

      MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
      if(p_res)
      {

        p_res->isProcessed=true;
        sctx_pool.returnResource(p_res);
        seg->ctx=NULL;
      }
    
    }
    checkpointInterval_latest=checkpointInterval;
    checkpointInterval=0;
    checkpointCounter++;


    return 0;
  }


  static int OnTimeRepEndCB( MSTP_segment* seg,MSTP_segment_adv_info *info)
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

  // static int instTrackingEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  // {
  //   CB_Count+=1;
  //   MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
  //   if(p_res==NULL)return -1;
  //   *(p_res->TRACKING_INFO.p_ctrl_progress)=0;

  //   seg->ctx=NULL;
  //   sctx_pool.returnResource(p_res);
  //   return 0;
  // }

  static int instTrackingAbortCMDEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info);

  static int instTrackingAbortCMD_froce_EndCB(MSTP_segment* seg,MSTP_segment_adv_info *info);


  static int instTrackingCMDEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    CTX_TrackingInfo *ti=(CTX_TrackingInfo *)p_res->Generic.obj;
    
    MotionGroup_SIMP::TrackingInfo &trackingInfo=ti->self->trackingInfo;


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
        
        // dbg_msg_sendQ("EM_STOP");
        
        djrl.async_printf(0,"EM_STOP prog:%.1f pType:%d",trackingInfo.progress,trackingInfo.tro.progress_type,trackingInfo.tro);
        djrl.async_printf(0,"dists:%.2f,%.2f,%.2f",
          trackingInfo.tro.progress_VAD.movParam.Edistance,
          trackingInfo.tro.progress_VAD.movParam.distanceStart,
          trackingInfo.tro.progress_VAD.movParam.distanceEnd);

        djrl.async_printf(0,"dists:%d,%d,%d,%d",
          dbgInfo.progress_count,
          dbgInfo.advSt,
          dbgInfo.progress_count,
          dbgInfo.progress_reach_count);


          

        
        ti->self->EM_STOP();
        break;
      }


      {//Init trackingInfo movParam & advInfo
      
        trackingInfo.tro=ti->tro;

        if(trackingInfo.tro.progress_type==CTX_TrackingRouteOption::VAD)
        {
          trackingInfo.tro.progress_VAD.movParam.type=MSTP_segment_type::seg_line;


          trackingInfo.tro.progress_VAD.movParam.vcur=
          trackingInfo.tro.progress_VAD.movParam.vto=0;

          trackingInfo.tro.progress_VAD.movParam.distanceStart=0;

          trackingInfo.tro.progress_VAD.movParam.ctx=NULL;
          trackingInfo.tro.progress_VAD.movParam.startCB=
          trackingInfo.tro.progress_VAD.movParam.endCB=NULL;
        }
        if(trackingInfo.tro.progress_type==CTX_TrackingRouteOption::LOC)
        {

        }



        trackingInfo.advInfo.deaWeagle=1.2;
        trackingInfo.advInfo.magicSpace=0,
        trackingInfo.advInfo.minSpeed=5,
        trackingInfo.advInfo.inInDAcc=false,
        trackingInfo.advInfo.dstanceWent=0;
      }

      

      trackingInfo.PtF1=ti->PtF;
      trackingInfo.Pr1=ti->Pr;
      memcpy(trackingInfo.param1,ti->param,sizeof(trackingInfo.param1));



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

  static int SegHoldUntilStartCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    return 0;
  }


  
  static int SegHoldUntilEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    // p_res->SEG_HALT.haltFlag=false;
    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }

  
  
  static int ReturnCtxEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    // p_res->SEG_HALT.haltFlag=false;
    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int ReturnCtxEndCB(MSTP_segment1_5* seg,MSTP_segment_adv_info *info)
  {
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    // p_res->SEG_HALT.haltFlag=false;
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
    MotionGroup_SIMP *self=(MotionGroup_SIMP *)p_res->Generic.obj;

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
    .speed=7.0,
    .acc=10.0,
    .deacc=-10.0,
    .cornorR=0,

  };


  bool CheckHead(const char *str1,const char *str2)
  {
    return strncmp(str1, str2, strlen(str2))==0;
  }

  char cachedCMD[300];
  float PPU_offset=0;
  float R_offset=0,Z_offset=0,V_offset=0;
  float X_offset=0,Y_offset=0;
  float vars[20]={0};


  
  int G1_DomXYZ(float X,float Y,float Z,float F,float ACC,float DEA,float Corm)
  {
    
      CMDVec offseted_latestCMDLocation=planer_RZV_rdata.latestCMDLocation;
      offseted_latestCMDLocation.vec[0]-=R_offset;
      offseted_latestCMDLocation.vec[1]-=Z_offset;
      offseted_latestCMDLocation.vec[2]-=V_offset;
      CMDVec vec_dev_xyz;
      
      if(false == RZV2XYZ(vec_dev_xyz.vec,offseted_latestCMDLocation.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }

      CMDVec vec_dom_xyz;
      coord_dev2dom(vec_dom_xyz.vec,vec_dev_xyz.vec);



      if(X==X)
        vec_dom_xyz.vec[0]=X;
      if(Y==Y)
        vec_dom_xyz.vec[1]=Y;
      if(Z==Z)
        vec_dom_xyz.vec[2]=Z;

      coord_dom2dev(vec_dev_xyz.vec,vec_dom_xyz.vec);

      CMDVec vec_rzv;
      if(false == XYZ2RZV(vec_rzv.vec,vec_dev_xyz.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }
      return G1_RZV(vec_rzv.vec[0],vec_rzv.vec[1],vec_rzv.vec[2],F,ACC,DEA,Corm);
  }


  int HACK_LATEST_CMD_ID=-1;
  float latestSegRatio=-1;
  public:
  int XYZ2RZV_w_offset(float *dst,const float* src)
  {
    
    CMDVec vec_xyz;


    if(src[0]!=src[0] || src[1]!=src[1] || src[2]!=src[2])
    {//fill in the current location
      CMDVec offseted_latestCMDLocation=planer_RZV_rdata.latestCMDLocation;
      offseted_latestCMDLocation.vec[0]-=R_offset;
      offseted_latestCMDLocation.vec[1]-=Z_offset;
      offseted_latestCMDLocation.vec[2]-=V_offset;
      
      if(false == RZV2XYZ(vec_xyz.vec,offseted_latestCMDLocation.vec))
      {
        return -1;
      }
    }

    if(src[0]==src[0])
      vec_xyz.vec[0]=src[0];
    if(src[1]==src[1])
      vec_xyz.vec[1]=src[1];
    if(src[2]==src[2])
      vec_xyz.vec[2]=src[2];

    if(false == XYZ2RZV(dst,vec_xyz.vec))
    {
      return -2;
    }

    dst[0]+=R_offset;
    dst[1]+=Z_offset;
    dst[2]+=V_offset;

    return 0;
  }


  int G1_XYZ(float X,float Y,float Z,float F,float ACC,float DEA,float Corm)
  {
    
      CMDVec offseted_latestCMDLocation=planer_RZV_rdata.latestCMDLocation;
      offseted_latestCMDLocation.vec[0]-=R_offset;
      offseted_latestCMDLocation.vec[1]-=Z_offset;
      offseted_latestCMDLocation.vec[2]-=V_offset;
      CMDVec vec_xyz;
      
      if(false == RZV2XYZ(vec_xyz.vec,offseted_latestCMDLocation.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }

      if(X==X)
        vec_xyz.vec[0]=X;
      if(Y==Y)
        vec_xyz.vec[1]=Y;
      if(Z==Z)
        vec_xyz.vec[2]=Z;

      CMDVec vec_rzv;
      if(false == XYZ2RZV(vec_rzv.vec,vec_xyz.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }
      return G1_RZV(vec_rzv.vec[0],vec_rzv.vec[1],vec_rzv.vec[2],F,ACC,DEA,Corm);
  }

  int G1_RZV(float R,float Z,float V,float F,float ACC,float DEA,float Corm)
  {


      CMDVec vec_coord=planer_RZV_rdata.latestCMDLocation;
      if(R==R)
        vec_coord.vec[0]=R+R_offset;
      if(Z==Z)
        vec_coord.vec[1]=Z+Z_offset;
      if(V==V)
        vec_coord.vec[2]=V+V_offset;




      MSTP_segment_extra_info exinfo = latestExtInfo;

      if(F==F)
      {
        exinfo.speed=F; // Convert each element to float and store it.
      }

      if(ACC==ACC)
      {
        exinfo.acc=ACC; // Convert each element to float and store it.
      }

      if(DEA==DEA)
      {
        exinfo.deacc=DEA; // Convert each element to float and store it.
      }

      if(Corm==Corm)
      {
        exinfo.cornorR=Corm; // Convert each element to float and store it.
      }
      if(latestSegHaltCtx==NULL)
      {//NOT in Queue blocking status
        exinfo.cornorR=0;
      }

      latestExtInfo=exinfo;
      // __UPRT_I_("vec:%s",vec_to_string(moveVec.vec).c_str());
      HACK_segComRequest=true;

      
      CMDVec moveVec;

      moveVec=MotorGroup.sub(vec_coord,planer_RZV_rdata.latestCMDLocation);
      


      while(HACK_segComRequest)yield();//wait for the moment that the segComRequest turned off by timer thread, but, it is not a good way to do it
      MotorGroup.pushInMoveVec(HACK_LATEST_CMD_ID,moveVec,planer_RZV_rdata.latestCMDLocation,exinfo,NULL,NULL,NULL);
      planer_RZV_rdata.latestCMDLocation=vec_coord;

              // ReadGVecData(blks,blkCount,vec_f,&exinfo);

      // ConvUnitVecToPulseVec(&vec_f,&exinfo);
      
      // xVec newLoc=stpG.pulse_latestLoc;
      // SetxVec_fToxVec(newLoc,vec_f);
      // // xVec goVec=vecSub(newLoc,stpG.pulse_latestLoc);
      
      // stpG.MoveTo(newLoc,NULL,&exinfo);

      return  MCMD_Status::TASK_OK;

  }


  MSTP_SegCtx *latestSegHaltCtx=NULL;

  int segsSafeSpace=6;
  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {


    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::TASK_UNSUPPORTED;
    }



    if( MotorGroup.segs.space()<segsSafeSpace)
    {
      if(cachedCMD[0]!='\0')return MCMD_Status::TASK_FAILED;//the cached CMD is not empty
      int slen=serializeJson(cmd, cachedCMD,sizeof(cachedCMD));

      
      djrl.dbg_printf("chache cmd slen:%d...",slen);
      
      return MCMD_Status::TASK_WAIT_HOLD_RSP;
    }


    const char* type = cmd["cmd"];


    if(homedFlag==false)
    {
      RESET_Queue();
      goto HOMINGSEC;
    }

    HACK_LATEST_CMD_ID= cmd["id"];
    // int test_runCount=0;
    // if(cmd["runCount"].is<float>())
    //   test_runCount=cmd["runCount"];

    // int ret_vars_count=9999;
    // if(cmd["ret_vars_count"].is<float>())
    //   ret_vars_count=(int)cmd["ret_vars_count"];


    if(strcmp(type,"G1.XYZ")==0)//XYZ
    {


      float X=jgetNum(cmd,"X");
      float Y=jgetNum(cmd,"Y");
      float Z=jgetNum(cmd,"Z");
      
      float F=jgetNum(cmd,"F");
      float ACC=jgetNum(cmd,"ACC");
      float DEA=jgetNum(cmd,"DEA");
      float Corm=jgetNum(cmd,"Corm");



      return G1_XYZ(X,Y,Z,F,ACC,DEA,Corm);

    }

    
    if(strcmp(type,"G1.domXYZ")==0)//XYZ
    {

      float X=jgetNum(cmd,"X");
      float Y=jgetNum(cmd,"Y");
      float Z=jgetNum(cmd,"Z");

      float F=jgetNum(cmd,"F");
      float ACC=jgetNum(cmd,"ACC");
      float DEA=jgetNum(cmd,"DEA");
      float Corm=jgetNum(cmd,"Corm");



      return G1_DomXYZ(X,Y,Z,F,ACC,DEA,Corm);
    }



    if(strcmp(type,"PredictPlateObjLoc.DomXYZ")==0)//XYZ
    {

      float domX=jgetNum(cmd,"X");
      float domY=jgetNum(cmd,"Y");
      float domZ=jgetNum(cmd,"Z");


      float OBJ_PLATE_ENC=jgetNum(cmd,"OBJ_PLATE_ENC");


      float MOVE_TO_PLATE_ENC=jgetNum(cmd,"MOVE_TO_PLATE_ENC");


      if(OBJ_PLATE_ENC==OBJ_PLATE_ENC && MOVE_TO_PLATE_ENC==MOVE_TO_PLATE_ENC)
      {



        float param[3];
        MG_PLATE.convertDomainXYZToObjParam(param,domX,domY,domZ,OBJ_PLATE_ENC);
        
        float domXYZ[3];
        MG_PLATE.predictObj_DomainLocation(domXYZ,param,MOVE_TO_PLATE_ENC);

        domX=domXYZ[0];
        domY=domXYZ[1];
        domZ=domXYZ[2];
      }

      cmd_ret["X"]=domX;
      cmd_ret["Y"]=domY;
      cmd_ret["Z"]=domZ;

      return MCMD_Status::TASK_OK;
    }

    {

      if(strcmp(type,"STATION.G1")==0)
      {
        float R=jgetNum(cmd,"R");

        if(R!=R)
        {
          return MCMD_Status::TASK_FAILED;
        }
        StpGroup1_5::CMDVec newLoc;
        newLoc.vec[0]=R;

        
        MSTP_segment_extra_info exinfo;
        exinfo.speed=jgetNum(cmd,"F",planer_S_rdata.speed_exinfo.speed);
        exinfo.acc=jgetNum(cmd,"ACC",planer_S_rdata.speed_exinfo.acc);
        exinfo.deacc=jgetNum(cmd,"DEA",planer_S_rdata.speed_exinfo.deacc);

        planer_S_rdata.speed_exinfo=exinfo;
        
        
        auto moveVec=planer_S.sub(newLoc,planer_S_rdata.latestCMDLocation);
        planer_S.pushInMoveVec(HACK_LATEST_CMD_ID,moveVec,planer_S_rdata.latestCMDLocation,exinfo,NULL,NULL,NULL);
        planer_S_rdata.latestCMDLocation=newLoc;
        return MCMD_Status::TASK_OK;

      }
          
      if(strcmp(type,"STATION.SegHalt")==0)
      {
        


        float target_checkpoint=jgetNum(cmd,"checkpoint");
        if(target_checkpoint!=target_checkpoint)
        {
          return MCMD_Status::TASK_FAILED;
        }

        MSTP_SegCtx *p_res;

        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return MCMD_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::SEG_HALT_FOR_CHECKPOINT;
        p_res->SEG_HALT_FOR_CHECKPOINT.target_checkpoint=target_checkpoint;

        planer_S.pushInPause(HACK_LATEST_CMD_ID,99999999,NULL,ReturnCtxEndCB,(void*)p_res);
        return  MCMD_Status::TASK_OK;
      }



    }

    
    {
      if(strcmp(type,"REELADV.G1")==0)
      {
        float R=jgetNum(cmd,"R");
        if(R!=R)
        {
          return MCMD_Status::TASK_FAILED;
        }
        
        StpGroup1_5::CMDVec newLoc;
        newLoc.vec[0]=R;

        MSTP_segment_extra_info exinfo;
        exinfo.speed=jgetNum(cmd,"F",planer_ReelAdv_rdata.speed_exinfo.speed);
        exinfo.acc=jgetNum(cmd,"ACC",planer_ReelAdv_rdata.speed_exinfo.acc);
        exinfo.deacc=jgetNum(cmd,"DEA",planer_ReelAdv_rdata.speed_exinfo.deacc);

        planer_ReelAdv_rdata.speed_exinfo=exinfo;
        
        
        auto moveVec=planer_ReelAdv.sub(newLoc,planer_ReelAdv_rdata.latestCMDLocation);
        planer_ReelAdv.pushInMoveVec(HACK_LATEST_CMD_ID,moveVec,planer_ReelAdv_rdata.latestCMDLocation,exinfo,NULL,NULL,NULL);
        planer_ReelAdv_rdata.latestCMDLocation=newLoc;
        return MCMD_Status::TASK_OK;

      }

      if(strcmp(type,"REELADV.SegHalt")==0)
      {
        


        float target_checkpoint=jgetNum(cmd,"checkpoint");
        if(target_checkpoint!=target_checkpoint)
        {
          return MCMD_Status::TASK_FAILED;
        }

        MSTP_SegCtx *p_res;

        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return MCMD_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::SEG_HALT_FOR_CHECKPOINT;
        p_res->SEG_HALT_FOR_CHECKPOINT.target_checkpoint=target_checkpoint;

        planer_ReelAdv.pushInPause(HACK_LATEST_CMD_ID,99999999,NULL,ReturnCtxEndCB,(void*)p_res);
        return  MCMD_Status::TASK_OK;
      }


    }

    if(strcmp(type,"G1.PlatXYZ")==0)//XYZ
    {

      float X=jgetNum(cmd,"X");
      float Y=jgetNum(cmd,"Y");
      float Z=jgetNum(cmd,"Z");

      float platXYZ[3]={X,Y,Z};
      float domXYZ[3];
      MG_PLATE.coord_dev2dom(domXYZ,platXYZ);//might have NAN in the XYZ
      
      float F=jgetNum(cmd,"F");
      float ACC=jgetNum(cmd,"ACC");
      float DEA=jgetNum(cmd,"DEA");
      float Corm=jgetNum(cmd,"Corm");



      return G1_DomXYZ(domXYZ[0],domXYZ[1],domXYZ[2],F,ACC,DEA,Corm);
    }

    if(strcmp(type,"G1")==0 ||strcmp(type,"G1.RZV")==0)//RZV
    {

      float R=jgetNum(cmd,"R");
      float Z=jgetNum(cmd,"Z");
      float V=jgetNum(cmd,"V");
      
      float F=jgetNum(cmd,"F");
      float ACC=jgetNum(cmd,"ACC");
      float DEA=jgetNum(cmd,"DEA");
      float Corm=jgetNum(cmd,"Corm");

      
      return G1_RZV(R,Z,V,F,ACC,DEA,Corm);

    }

    if(strcmp(type,"M114.XYZ")==0)
    {

      CMDVec vec_xyz;
      CMDVec offseted_latestCMDLocation=planer_RZV_rdata.latestCMDLocation;
      offseted_latestCMDLocation.vec[0]-=R_offset;
      offseted_latestCMDLocation.vec[1]-=Z_offset;
      offseted_latestCMDLocation.vec[2]-=V_offset;
      
      if(false == RZV2XYZ(vec_xyz.vec,offseted_latestCMDLocation.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }

      cmd_ret["XYZ"]["X"]=vec_xyz.vec[0];
      cmd_ret["XYZ"]["Y"]=vec_xyz.vec[1];
      cmd_ret["XYZ"]["Z"]=vec_xyz.vec[2];




      return MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"M114.domXYZ")==0)
    {

      CMDVec offseted_latestCMDLocation=planer_RZV_rdata.latestCMDLocation;
      offseted_latestCMDLocation.vec[0]-=R_offset;
      offseted_latestCMDLocation.vec[1]-=Z_offset;
      offseted_latestCMDLocation.vec[2]-=V_offset;
      
      CMDVec vec_devxyz;
      if(false == RZV2XYZ(vec_devxyz.vec,offseted_latestCMDLocation.vec))
      {
        return MCMD_Status::TASK_FAILED;
      }
      
      CMDVec vec_domxyz;
      coord_dev2dom(vec_domxyz.vec,vec_devxyz.vec);

      cmd_ret["XYZ"]["X"]=vec_domxyz.vec[0];
      cmd_ret["XYZ"]["Y"]=vec_domxyz.vec[1];
      cmd_ret["XYZ"]["Z"]=vec_domxyz.vec[2];


      return MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"M114")==0||strcmp(type,"M114.RZV")==0)
    {
      cmd_ret["RZV"]["R"]=planer_RZV_rdata.latestCMDLocation.vec[0]-R_offset;
      cmd_ret["RZV"]["Z"]=planer_RZV_rdata.latestCMDLocation.vec[1]-Z_offset;
      cmd_ret["RZV"]["V"]=planer_RZV_rdata.latestCMDLocation.vec[2]-V_offset;


      return MCMD_Status::TASK_OK;
    }






    if(strcmp(type,"MACHPARAM")==0)//XYZ
    {
      armParam.inited=false;
      armParam.arc_r=cmd["arc"]["r"].as<float>();
      armParam.arc_c0_x=cmd["arc"]["x"].as<float>();
      armParam.arc_c0_y=cmd["arc"]["y"].as<float>();
      armParam.angleOffset=cmd["arc"]["angleOffset"].as<float>();

      armParam.linear_vec_x=cmd["linear"]["vec"]["x"].as<float>();
      armParam.linear_vec_y=cmd["linear"]["vec"]["y"].as<float>();
      armParam.y_axis_inverted=cmd["y_axis_inverted"];
      
      armParam.inited=true;
      return MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"COORD_ADJ_MAP")==0)//XYZ
    {
      if( MotorGroup.segs.size()!=0)//make sure there is no move in progress
      {
        cmd_ret["msg"]="segs.size()!=0";
        return MCMD_Status::TASK_FAILED;
      }


      // try{



      
      
      if(cmd["map"].size()>10)
      {
        cmd_ret["msg"]="map.size()>10";
        return MCMD_Status::TASK_FAILED;
      }
      
      /*
      cmd json:
      {
        "mult_factor":1000,
        "map":[{tar:[1,2],res:[3,4]},{tar:[5,6],res:[7,8]}...]
      }
      */
      //loop thru the map
      armParam.calibMap_size=cmd["map"].size();
      djrl.dbg_printf("armParam.calibMap_size:%d",armParam.calibMap_size);
      float* map_ptr=armParam.calibMap;//fill the map
      for(int i=0;i<armParam.calibMap_size;i+=1,map_ptr+=4)
      {
        map_ptr[0]=(cmd["map"][i]["tar"][0].as<float>());
        map_ptr[1]=(cmd["map"][i]["tar"][1].as<float>());
        map_ptr[2]=(cmd["map"][i]["res"][0].as<float>());
        map_ptr[3]=(cmd["map"][i]["res"][1].as<float>());
      }
      // }
      // catch(...)
      // {
      //   armParam.calibMap_size=0;
      //   cmd_ret["msg"]="exception";
      //   return MCMD_Status::TASK_FAILED;
      // }

      return MCMD_Status::TASK_OK;

    }

    if(strcmp(type,"SetAxisPos")==0)//RZV
    {
      
      if(cmd["R"].is<float>())
      {
        float tmp=cmd["R"].as<float>();
        R_offset=planer_RZV_rdata.latestCMDLocation.vec[0]-tmp; 
      }

      if(cmd["Z"].is<float>())
      {
        float tmp=cmd["Z"].as<float>();
        Z_offset=planer_RZV_rdata.latestCMDLocation.vec[1]-tmp; 
      }

      if(cmd["V"].is<float>())
      {
        float tmp=cmd["V"].as<float>();
        V_offset=planer_RZV_rdata.latestCMDLocation.vec[2]-tmp; 
      }

      return  MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"G4")==0)
    {
      
      uint32_t pauseTime=0;
      if(cmd["P"].is<uint32_t>())
      {
        pauseTime=cmd["P"].as<uint32_t>();
      }

      
      MotorGroup.pushInPause(HACK_LATEST_CMD_ID,pauseTime,NULL,NULL,NULL);

      return  MCMD_Status::TASK_OK;

    }

    if(strcmp(type,"BumpCheckPoint")==0)
    {
      checkpointAdeadCounter++;
      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,BumpCheckPointCounterEndCB,NULL);

      cmd_ret["checkpoint"]=checkpointAdeadCounter;
      return MCMD_Status::TASK_OK;
    }

    
    if(strcmp(type,"WaitForCheckPoint")==0)
    {
      
      auto* taskInfo=LiveTask.getHead();


      if(taskInfo==NULL)
      {
        
        cmd_ret["msg"]="LiveTask.size:"+std::to_string(LiveTask.size());
        return MCMD_Status::TASK_FAILED;
      }
      int target_checkpoint=jgetNum(cmd,"checkpoint",checkpointAdeadCounter);
      if(target_checkpoint>checkpointAdeadCounter)
      {
        cmd_ret["msg"]="checkpoint:"+std::to_string(target_checkpoint)+" top checkpoint:"+std::to_string(checkpointAdeadCounter);
        return MCMD_Status::PARAM_PARSE_ERROR;
      }

      int respond_delayTime=jgetNum(cmd,"rsp_delay",0);
      



      
      taskInfo->type=LiveTaskInfo::checkpoint_reply;
      taskInfo->checkpoint_reply_info.target_checkpoint=target_checkpoint;
      taskInfo->checkpoint_reply_info.respond_delayTime=respond_delayTime;
      taskInfo->cmdID=jgetNum(cmd,"id",-1);


      
      LiveTask.pushHead();

      return MCMD_Status::TASK_WAIT_HOLD_RSP;
    }


    if(strcmp(type,"M42")==0)
    {
      int32_t tid=-1,cid=-1;
      int32_t bank=0;
      uint32_t mask=0;
      uint32_t pout=0;
      uint32_t ttagbf=0;


      
      if(cmd["cid"].is<int32_t>())
      {
        cid=jgetInt32(cmd,"cid",cid);
        tid=jgetInt32(cmd,"tid",tid);
        ttagbf=jgetInt32(cmd,"ttagbf",ttagbf);

      }


      bank=jgetInt32(cmd,"b",bank);
      mask=jgetInt32(cmd,"m",mask);
      pout=jgetInt32(cmd,"p",pout);


      pout&=mask;


    
      MSTP_SegCtx *p_res;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;

      p_res->IO_CTRL.bank=bank;
      p_res->IO_CTRL.mask=mask;
      p_res->IO_CTRL.pout=pout;
      p_res->IO_CTRL.cid=cid;
      p_res->IO_CTRL.tid=tid;
      p_res->IO_CTRL.ttagbf=ttagbf;
      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,IOCtrlEndCB,(void*)p_res);


      return MCMD_Status::TASK_OK;
    }
    
    if(strcmp(type,"M400")==0)//Wait for action finish
    { 

      MSTP_SegCtx *p_res;
      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return  MCMD_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::ON_TIME_REPLY;

      p_res->ON_TIME_REP.id=cmd["id"].as<int>();;
      p_res->ON_TIME_REP.isAck=true;
      MotorGroup.pushInPause(HACK_LATEST_CMD_ID,0,NULL,OnTimeRepEndCB,(void*)p_res);
      return  MCMD_Status::TASK_OK_HOLD_RSP;
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
      
      return MCMD_Status::TASK_OK;
    }


    if(strcmp(type,"TargetTrack_Abort")==0)
    {

      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,instTrackingAbortCMDEndCB,NULL);



      return MCMD_Status::TASK_OK;
    }


    if(strcmp(type,"TargetTrack_Abort_force")==0)
    {

      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,instTrackingAbortCMD_froce_EndCB,NULL);



      return MCMD_Status::TASK_OK;
    }
    if(strcmp(type,"TargetTrack")==0)
    {


      int trackIdx=-1;
      if(cmd["trackIdx"].is<float>())
        trackIdx=cmd["trackIdx"];
      else
      {
        cmd_ret["msg"]="trackIdx is needed";
        return MCMD_Status::PARAM_PARSE_ERROR;
      }
      

      //get the latst seq pointer in the queue


      CTX_TrackingInfo tinfo;

      

      if(trackIdx==0)
      {
        tinfo.PtF=NULL;
      }
      else if(trackIdx==1)
      {
        float plat_angle=jgetNum(cmd,"plat_angle");
        float obj_dom_x =jgetNum(cmd,"obj_dom_x");
        float obj_dom_y =jgetNum(cmd,"obj_dom_y");

        if(plat_angle!=plat_angle || obj_dom_x!=obj_dom_x || obj_dom_y!=obj_dom_y)
        {
          cmd_ret["msg"]="plat_angle obj_dom_x obj_dom_y are needed";
          return MCMD_Status::PARAM_PARSE_ERROR;
        }


        MG_PLATE.convertDomainXYZToObjParam(tinfo.param,obj_dom_x,obj_dom_y,planer_RZV_rdata.latestCMDLocation.vec[1]-Z_offset,plat_angle);

        
        djrl.dbg_printf("tinfo.param: %.3f,%.3f,%.3f,%.3f",
          tinfo.param[0],
          tinfo.param[1],
          tinfo.param[2],
          tinfo.param[3]);
        tinfo.PtF=GetPlateObjLocOnArmRZV;



        float gox=jgetNum(cmd,"X");
        float goy=jgetNum(cmd,"Y");
        float goz=jgetNum(cmd,"Z");

        float goArmXYZ[3]={gox,goy,goz};
        // if(goy!=goy || gox!=gox || goz!=goz)
        // {
        //   cmd_ret["msg"]="target X,Y are needed";
        //   return MCMD_Status::PARAM_PARSE_ERROR;
        // }


        if(XYZ2RZV_w_offset(tinfo.Pr.vec,goArmXYZ)!=0)
        {
          cmd_ret["msg"]="XYZ2RZV_w_offset return failed....";
          return MCMD_Status::PARAM_PARSE_ERROR;
        }

        
      
        djrl.dbg_printf("tinfo.Pr: %.3f,%.3f,%.3f",
          tinfo.Pr.vec[0],
          tinfo.Pr.vec[1],
          tinfo.Pr.vec[2]);
        //convert To RZV

      }
      else if(trackIdx==2)
      {
        tinfo.PtF=mockObjTrajectOnArmRZV;


        tinfo.Pr.vec[0]=jgetNum(cmd,"R");
        tinfo.Pr.vec[1]=jgetNum(cmd,"Z");
        tinfo.Pr.vec[2]=jgetNum(cmd,"V");

      }
      else
      {
        tinfo.PtF=NULL;
      }




      MSTP_segment_adv_info _advInfo;
      MSTP_segment _movParam;

      int progress_type=  jgetInt32(cmd,  "progress_type",0);

      tinfo.self=this;

      if(progress_type==0)
      {

        _movParam.type=MSTP_segment_type::seg_line;

        // float transitionTime=1000;
          
        float F=  jgetNum(cmd,  "F",1000);
        float ACC=jgetNum(cmd,"ACC",1000);
        float DEA=jgetNum(cmd,"DEA");
        float distance=jgetNum(cmd,"DISTANCE",1000);

        if(F<0)F=-F;
        if(ACC<0)ACC=-ACC;

        if(DEA!=DEA)
        {
          DEA=-ACC;
        }
        if(DEA>0)DEA=-DEA;
        

        tinfo.tro.progress_type= CTX_TrackingRouteOption::VAD;
        tinfo.tro.progress_VAD.movParam.acc=F;
        tinfo.tro.progress_VAD.movParam.deacc=DEA;
        tinfo.tro.progress_VAD.movParam.vmax=ACC;
        tinfo.tro.progress_VAD.movParam.Edistance=
        tinfo.tro.progress_VAD.movParam.distanceEnd=distance;
        tinfo.tro.progress_VAD.movParam.distanceStart=0;

      }

      else if(progress_type==1)
      {

        tinfo.tro.progress_type= CTX_TrackingRouteOption::LOC;

        float st_X =cmd["start"]["X"].as<float>();
        float st_Y =cmd["start"]["Y"].as<float>();
        
        float en_X =cmd["end"]["X"].as<float>();
        float en_Y =cmd["end"]["Y"].as<float>();
        if(st_X!=st_X || st_Y!=st_Y || en_X!=en_X || en_Y!=en_Y)
        {
          cmd_ret["msg"]="start X,Y end X,Y are needed";
          return MCMD_Status::PARAM_PARSE_ERROR;
        }
        
        
        float startArmXYZ[3]={st_X,st_Y,0};

        

        if(XYZ2RZV_w_offset(tinfo.tro.progress_LOC.start.vec,startArmXYZ)!=0)
        {
          cmd_ret["msg"]="XYZ2RZV_w_offset return failed....";
          return MCMD_Status::PARAM_PARSE_ERROR;
        }



        float endArmXYZ[3]={en_X,en_Y,0};

        

        if(XYZ2RZV_w_offset(tinfo.tro.progress_LOC.end.vec,endArmXYZ)!=0)
        {
          cmd_ret["msg"]="XYZ2RZV_w_offset return failed....";
          return MCMD_Status::PARAM_PARSE_ERROR;
        }


        tinfo.tro.progress_LOC.vec.vec[0]=tinfo.tro.progress_LOC.end.vec[0]-tinfo.tro.progress_LOC.start.vec[0];
        tinfo.tro.progress_LOC.vec.vec[2]=tinfo.tro.progress_LOC.end.vec[2]-tinfo.tro.progress_LOC.start.vec[2];

        tinfo.tro.progress_LOC.distance=
          hypot(tinfo.tro.progress_LOC.vec.vec[0],tinfo.tro.progress_LOC.vec.vec[2]);

        tinfo.tro.progress_LOC.vec.vec[0]/=tinfo.tro.progress_LOC.distance;
        tinfo.tro.progress_LOC.vec.vec[2]/=tinfo.tro.progress_LOC.distance;



        
        retdoc["sr"]=tinfo.tro.progress_LOC.start.vec[0];
        retdoc["sv"]=tinfo.tro.progress_LOC.start.vec[2];
        retdoc["er"]=tinfo.tro.progress_LOC.end.vec[0];
        retdoc["ev"]=tinfo.tro.progress_LOC.end.vec[2];
        retdoc["dist"]=tinfo.tro.progress_LOC.distance;
        retdoc["vr"]=tinfo.tro.progress_LOC.vec.vec[0];
        retdoc["vv"]=tinfo.tro.progress_LOC.vec.vec[2];
      }
      else if(progress_type==2)
      {

        tinfo.tro.progress_type= CTX_TrackingRouteOption::LOC;

        tinfo.tro.progress_LOC.start.vec[0]=cmd["start"]["R"].as<float>();
        tinfo.tro.progress_LOC.start.vec[2]=cmd["start"]["V"].as<float>();


        tinfo.tro.progress_LOC.end.vec[0]=cmd["end"]["R"].as<float>();
        tinfo.tro.progress_LOC.end.vec[2]=cmd["end"]["V"].as<float>();


        tinfo.tro.progress_LOC.distance=
          hypot(
            tinfo.tro.progress_LOC.start.vec[0]-tinfo.tro.progress_LOC.end.vec[0],
            tinfo.tro.progress_LOC.start.vec[2]-tinfo.tro.progress_LOC.end.vec[2]);
        tinfo.tro.progress_LOC.vec.vec[0]=tinfo.tro.progress_LOC.end.vec[0]-tinfo.tro.progress_LOC.start.vec[0];
        tinfo.tro.progress_LOC.vec.vec[2]=tinfo.tro.progress_LOC.end.vec[2]-tinfo.tro.progress_LOC.start.vec[2];

        
        //normalize
        tinfo.tro.progress_LOC.vec.vec[0]/=tinfo.tro.progress_LOC.distance;
        tinfo.tro.progress_LOC.vec.vec[2]/=tinfo.tro.progress_LOC.distance;

      }

      
      tinfo.tro.doBlockFeedOnLoc=false;
      if(cmd.containsKey("BlockFeedOnLoc"))
      {
        
        float fr_X =cmd["BlockFeedOnLoc"]["X"].as<float>();
        float fr_Y =cmd["BlockFeedOnLoc"]["Y"].as<float>();
        
        float blockXYZ[3]={fr_X,fr_Y,0};

        

        XYZ2RZV_w_offset(tinfo.tro.BlockFeedOnLoc.vec,blockXYZ);


        tinfo.tro.doBlockFeedOnLoc=true;
      }else
      //{"type":"m1","cmd":"TargetTrack","trackIdx":2, "progress_type":2,  "R":-20,"Z":-5,"V":5, "start":{"R":-20,"Z":-5,"V":5}, "end":{"R":-50,"Z":-5,"V":10}, "BlockFeedOnLocRZV":{"R":-20,"Z":-5,"V":5} }   
      if(cmd.containsKey("BlockFeedOnLocRZV"))
      {
        
        tinfo.tro.BlockFeedOnLoc.vec[0] =cmd["BlockFeedOnLocRZV"]["R"].as<float>();
        tinfo.tro.BlockFeedOnLoc.vec[2] =cmd["BlockFeedOnLocRZV"]["V"].as<float>();


        tinfo.tro.doBlockFeedOnLoc=true;
      }

      // return MCMD_Status::TASK_FAILED;
      CTX_TrackingInfo *p_tinfo;
      while((p_tinfo=trackingInfoCTX_pool.applyResource())==NULL)yield();
      *p_tinfo=tinfo;


      MSTP_SegCtx *p_res;
      while((p_res=sctx_pool.applyResource())==NULL)yield();



      
      p_res->type=MSTP_SegCtx_TYPE::GENERIC;
      p_res->Generic.obj=(void*)p_tinfo;

      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,instTrackingCMDEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK;

    }


    if(strcmp(type,"ASSERT_TargetTrack_sattled")==0)
    {



      MSTP_SegCtx *p_res;

      while((p_res=sctx_pool.applyResource())==NULL) yield();

      p_res->type=MSTP_SegCtx_TYPE::GENERIC;
      p_res->Generic.obj=(void*)this;

      
      MotorGroup.pushInInstant(HACK_LATEST_CMD_ID,NULL,AssertTrackingSattledEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"TargetTrack_sattled")==0)
    {
      MSTP_SegCtx *p_res;

      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return MCMD_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED;

      p_res->TRACKING_HOLD_UNTIL.id=-1;
      //-1 means halt forever
      MotorGroup.pushInPause(HACK_LATEST_CMD_ID,-1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK;
    }



    if(strcmp(type,"TargetTrack_sattled.BLOCKING")==0)
    {
      MSTP_SegCtx *p_res;

      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return MCMD_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED;

      p_res->TRACKING_HOLD_UNTIL.id=cmd["id"];

      //-1 means halt forever
      MotorGroup.pushInPause(-HACK_LATEST_CMD_ID,1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK_HOLD_RSP;
    }


    
    if(strcmp(type,"SegHalt")==0)
    {
      if(latestSegHaltCtx!=NULL)
      {
        bool keepGoing=false;
        if(keepGoing)
        {
          latestSegHaltCtx->SEG_HALT.haltFlag=false;
          latestSegHaltCtx=NULL;
        }
        else
        {
          return  MCMD_Status::TASK_FAILED;
        }

      }





      MSTP_SegCtx *p_res;

      int retryCount=0;
      while((p_res=sctx_pool.applyResource())==NULL)
      {
        if(retryCount++>100)
          return MCMD_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::SEG_HALT;
      p_res->SEG_HALT.haltFlag=true;

      latestSegHaltCtx=p_res;
      //-1 means halt forever
      MotorGroup.pushInPause(HACK_LATEST_CMD_ID,99999999,SegHoldUntilStartCB,SegHoldUntilEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK;
    }

    
    
    if(strcmp(type,"SegHaltRelease")==0)
    {
      if(latestSegHaltCtx!=NULL)//release the last latch/halt
      {
        latestSegHaltCtx->SEG_HALT.haltFlag=false;
        latestSegHaltCtx=NULL;
        return  MCMD_Status::TASK_OK;
      }
      return  MCMD_Status::TASK_FAILED;
    }


      
    {
      auto ret = XY_Coord_Conv::CMDProcess(cmd,cmd_ret);
      if(ret!=MCMD_Status::TASK_UNSUPPORTED)
      {
        return ret;
      }
    }

HOMINGSEC:



    if(strcmp(type,"ZVIS_PARAM")==0)
    {
      ZVIShaper_Idx1=jgetNum(cmd,"idx1",0);
      ZVIShaper_Idx2=jgetNum(cmd,"idx2",0);
      
      cmd_ret["idx1"]=ZVIShaper_Idx1;
      cmd_ret["idx2"]=ZVIShaper_Idx2;
      return MCMD_Status::TASK_OK;
    }
    
    if(strcmp(type,"SET_DBG_COUNTER_IDX")==0)
    {
      int idx=jgetInt32(cmd,"idx",0);
      if(idx<0||idx>2)idx=0;
      dbg_index=idx;
      dbg_maxStepDiff=0;
      dbg_maxVStep=0;
      turnCount=0;
      return  MCMD_Status::TASK_OK;
    }



    if(strcmp(type,"ENABLE_MOTOR_MONITOR")==0)
    {
      if(motorMonitor_enableRecord==true)
      {
        motorMonitor_enableRecord=false;
        delay(2);

      }
      motorMonitor.clear();

      motorMonitor_recordCD=jgetInt32(cmd,"recordCD",0);
      motorMonitor_recordSkipMod=jgetInt32(cmd,"recordSkip",0)+1;
      motorMonitor_enableRecord=jgetBool(cmd,"enable",true);


      return  MCMD_Status::TASK_OK;
    }

    /*
{"type":"m1","cmd":"ENABLE_MOTOR_MONITOR","enable":true}
{"type":"m1","cmd":"DUMP_MOTOR_MONITOR","size":20}
 */
    if(strcmp(type,"DUMP_MOTOR_MONITOR")==0)
    {
      motorMonitor_enableRecord=false;
      delay(2);
      
      int offset=jgetInt32(cmd,"offset",0);
      int size=jgetInt32(cmd,"size",motorMonitor.capacity());
      int monSize=motorMonitor.size();
      int bufferSize=monSize;
      monSize-=offset;
      if(size>monSize)
      {
        size=monSize;
      }
      if(size<0)size=0;

      djrl.dbg_DIRECT_PRINT("{\"size\":%d,\"d\":[",motorMonitor.size());

      for(int i=0;i<size;i++)
      {

        djrl.dbg_DIRECT_PRINT("%d%s%s",
        *motorMonitor.getTail(i+offset),
        i==(size-1)?"":",",
        i%30==0?"\n":""
        );
      }
      djrl.dbg_DIRECT_PRINT("\n]}\n");


      return  MCMD_Status::TASK_OK;
    }
    
    if(strcmp(type,"SET_MOTOR_VAMAX")==0)
    {
      int axis=jgetInt32(cmd,"axis",-1);
      if(axis<0||axis>2)return MCMD_Status::TASK_FAILED;

      retdoc["V_Max"]=MotorGroup.axisSetup[axis].V_Max=jgetInt32(cmd,"V_Max",MotorGroup.axisSetup[axis].V_Max);
      retdoc["A_Max"]=MotorGroup.axisSetup[axis].A_Max=jgetInt32(cmd,"A_Max",MotorGroup.axisSetup[axis].A_Max);
      
      return  MCMD_Status::TASK_OK;
    }

    

    if(strcmp(type,"MOTOR_PARAMS")==0)
    {
      
      int idx=jgetInt32(cmd,"idx",-1);
      MotorCtrlCiA402_FeedBackStepper_position *tarMot=NULL;
      switch (idx)
      {
      case 0:
        tarMot=&motorR;
        break;
      case 1:
        tarMot=&motorZ;
        break;
      case 2:
        tarMot=&motorV;
        break;
      
      default:
        break;
      }
      if(tarMot==NULL)return MCMD_Status::PARAM_PARSE_ERROR;
      
      int COMP_DELAY=jgetInt32(cmd,"COMP_DELAY",-1);
      if(COMP_DELAY!=-1)
      {
        tarMot->CMDPosCompOffset_d100=COMP_DELAY;
      }
      retdoc["COMP_DELAY"]=tarMot->CMDPosCompOffset_d100;

      int allowedError=jgetInt32(cmd,"ALLOWED_ERROR",-1);
      if(allowedError!=-1)
      {
        tarMot->allowedError=allowedError;
      }
      retdoc["ALLOWED_ERROR"]=tarMot->allowedError;



      bool ENABLE_SKIP_DISABLE=jgetBool(cmd,"ENABLE_SKIP_DISABLE",tarMot->encoder_check);
      tarMot->encoder_check=ENABLE_SKIP_DISABLE;
      retdoc["ENABLE_SKIP_DISABLE"]=ENABLE_SKIP_DISABLE;


      
      tarMot->motor_skip_adjust_thres=
        jgetInt32(cmd,"ADJ_THRES",tarMot->motor_skip_adjust_thres);



      return MCMD_Status::TASK_OK;
    }






    if(strcmp(type,"BLOCKING_DELAY")==0)
    {
      int delaytime=jgetInt32(cmd,"time",1);
      delay(delaytime);
      return  MCMD_Status::TASK_OK;
    }

    // if(strcmp(type,"G28.PPU")==0)//homing
    // {
    //   // homingStatus=HomingStatus::Start;
    //   // homingAxisIdx=0;
    //   if(
    //     motorPPU.state==MotorCtrlCiA402::RunningState::fault ||
    //     motorR.state==MotorCtrlCiA402::RunningState::disabled|| 
    //     motorZ.state==MotorCtrlCiA402::RunningState::disabled||
    //     motorV.state==MotorCtrlCiA402::RunningState::disabled)
    //   {
    //     return MCMD_Status::TASK_FAILED;
    //   }
    //   homingSeq.clear();
    //   homingSeq.clear();
    //   float fineSpeed=1;


    //   float speedMult=10;
    //   if(cmd["S"].is<float>())
    //   {
    //     speedMult=cmd["S"].as<float>(); // Convert each element to float and store it.
    //     if(speedMult<0)speedMult=-speedMult;
    //   }
    //   if(cmd["s"].is<float>())
    //   {
    //     fineSpeed=cmd["s"].as<float>(); // Convert each element to float and store it.
    //     if(fineSpeed<0)fineSpeed=-fineSpeed;
    //   }
    //   PPU_offset=0;

    //   homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=3,.speed= speedMult,.speed_fine= fineSpeed,.sensorPin=1,.sensorSense=1});

    //   homingCMDID=cmd["id"].as<int>();

    //   return  MCMD_Status::TASK_OK_HOLD_RSP;
    // }
    // else 
    
    if(strcmp(type,"G28")==0)//homing
    {
      // homingStatus=HomingStatus::Start;
      // homingAxisIdx=0;
      if(
        motorR.state==MotorCtrlCiA402::RunningState::disabled|| 
        motorZ.state==MotorCtrlCiA402::RunningState::disabled||
        motorV.state==MotorCtrlCiA402::RunningState::disabled)
      {
        return MCMD_Status::TASK_FAILED;
      }
      homingSeq.clear();
      float fineSpeed=1;


      float speedMult=10;
      if(cmd["S"].is<float>())
      {
        speedMult=cmd["S"].as<float>(); // Convert each element to float and store it.
        if(speedMult<0)speedMult=-speedMult;
      }
      if(cmd["s"].is<float>())
      {
        fineSpeed=cmd["s"].as<float>(); // Convert each element to float and store it.
        if(fineSpeed<0)fineSpeed=-fineSpeed;
      }
      R_offset=Z_offset=V_offset=0;//reset offset
      X_offset=Y_offset=0;

      homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=1,.speed= speedMult,.speed_fine= fineSpeed,.sensorPin=1,.sensorSense=1});
      homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=0,.speed= speedMult,.speed_fine= fineSpeed,.sensorPin=0,.sensorSense=1});
      homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=-speedMult,.speed_fine=-fineSpeed,.sensorPin=2,.sensorSense=1});
      // homingSeq.push_back({.status=  HomingStatus::Start,.axisIdx=1,.speed=5,.speed_fine=1,.sensorPin=1,.sensorSense=0});
      // homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=5,.speed_fine=1,.sensorPin=2,.sensorSense=0});

  

      homingCMDID=cmd["id"].as<int>();

      return  MCMD_Status::TASK_OK_HOLD_RSP;
    }

    return MCMD_Status::TASK_UNSUPPORTED;
  }

  int try_cachedCMDProcess(JsonDocument& blank_jobj,JsonDocument& cmd_ret)
  {
    if(cachedCMD[0]=='\0')return MCMD_Status::NOP;
    if( MotorGroup.segs.space()<segsSafeSpace)return MCMD_Status::NOP;
    djrl.dbg_printf("try_cachedCMDProcess");
    blank_jobj.clear();


    
    DeserializationError error = deserializeJson(blank_jobj, (const char*)cachedCMD);
    int ret=CMDProcess(blank_jobj,cmd_ret);
    for(int i=0;cachedCMD[i];i++)
    {
      if(cachedCMD[i]=='"')cachedCMD[i]='\'';
    }
    djrl.dbg_printf(">>>ret:%d",ret);
    cachedCMD[0]='\0';
    return ret;
  }




};

MotionGroup_SIMP MG_SIMP;


int MotionGroup_SIMP::instTrackingAbortCMDEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
{
  {

    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res)
    {

      p_res->isProcessed=true;
      sctx_pool.returnResource(p_res);
      seg->ctx=NULL;
    }
  
  }

  djrl.async_printf(0,"abort curProg:%0.5f",MG_SIMP.trackingInfo.progress*100);
  if(MG_SIMP.trackingInfo.PtF1!=NULL && MG_SIMP.trackingInfo.progress<0.0001)
  {
    MG_SIMP.trackingInfo.PtF1=NULL;
    MG_SIMP.trackingInfo.progress=-1;
  }
  else
  {}

  return 0;
}

int MotionGroup_SIMP::instTrackingAbortCMD_froce_EndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
{
  {

    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res)
    {

      p_res->isProcessed=true;
      sctx_pool.returnResource(p_res);
      seg->ctx=NULL;
    }
  
  }

  djrl.async_printf(0,"froce abort curProg:%0.5f",MG_SIMP.trackingInfo.progress*100);
  MG_SIMP.trackingInfo.PtF1=NULL;
  MG_SIMP.trackingInfo.progress=-1;
  return 0;
}





class AUXTaskExecutor{

  struct AUX_TASK_INFO {

    enum AUX_TASK_INFO_TYPE{
      AUX_DELAY=1,
      AUX_WAIT_CHECKPOINT=2,
      AUX_IO_CTRL=3,
      
      AUX_WAIT_PROGRESS=101,
    } type;

    
      
    union {
      struct {
        int time_ms;
      }delay;
      struct {
        float target_progress;
        int approaching_dir;
        
        //assume target_progress=0.4
        //approaching_dir >0 means approach target_progress from smaller number (0.1->0.2->0.5(hit))
        //approaching_dir ==0 means exact hit  (0.1->0.2->0.5 -> 0.3->0.4(hit))
        //approaching_dir <0 ...(0.6->0.5->0.3(hit))
        

        
        int timeout;
      }wait_progress;


      struct {
        int target;
        int checkpoint;
        int ret_id;//-1 it will push in the queue and respond immidiately, otherwise, it will wait for the checkpoint
        int timeout_ms;//if after the timeout the checkpoint is not reached, it will return failed
      }wait_checkpoint;


      struct {

        uint32_t bank,mask,pout;
        int32_t tid,cid,ttagbf;
      }io_ctrl;
    };


    //Just for ioCtrl
    // string CID;
    // string TTAG;
  };

  char cachedCMD[300];
  public:
  RingBuf_Static<struct AUX_TASK_INFO,20> TaskQ;


  
  void update()
  {
    auto *taskInfo=TaskQ.getTail();
    if(taskInfo==NULL)return;
    switch(taskInfo->type)
    {
      case AUX_TASK_INFO::AUX_DELAY:
      {
        if(taskInfo->delay.time_ms)
        {
          taskInfo->delay.time_ms--;
        }
        else
        {
          TaskQ.consumeTail();
        }
        return;
      }
      case AUX_TASK_INFO::AUX_WAIT_PROGRESS:
      {
        float progress=MG_SIMP.trackingInfo.progress;

        if(taskInfo->wait_progress.approaching_dir>0)//approaching from smaller
        {
          if(progress>=taskInfo->wait_progress.target_progress)
          {
            TaskQ.consumeTail();
            return;
          }
        }
        else if(taskInfo->wait_progress.approaching_dir<0)//approaching from bigger
        {
          if(progress<=taskInfo->wait_progress.target_progress)
          {
            TaskQ.consumeTail();
            return;
          }
        }
        else
        {
          float diff=progress-taskInfo->wait_progress.target_progress;
          if(diff<0)diff=-diff;
          float range=0;
          if(diff<=range)//exact hit
          {
            TaskQ.consumeTail();
            return;
          }
        }

        if(taskInfo->wait_progress.timeout)
        {
          taskInfo->wait_progress.timeout--;
        }

        if(taskInfo->wait_progress.timeout==0)
        {
          TaskQ.consumeTail();//error, cannot reach target
          return;
        }


        return;
      }

      case AUX_TASK_INFO::AUX_WAIT_CHECKPOINT:
      {
        if(taskInfo->wait_checkpoint.target==1)
        {
          
          if(checkpointCounter>=taskInfo->wait_checkpoint.checkpoint)
          {
            if(taskInfo->wait_checkpoint.ret_id!=-1)
            {
              bool isAck=checkpointCounter==taskInfo->wait_checkpoint.checkpoint;
              struct Mstp2CommInfo tinfo={
              .type=Mstp2CommInfo_Type::respCheckpointFrame,
              .checkPointInterval=checkpointInterval_latest,
              .isAck=isAck,
              .resp_id=taskInfo->wait_checkpoint.ret_id
              };

              {
                Mstp2CommInfo* Qhead=NULL;
                while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
                *Qhead=tinfo;
                Mstp2CommInfoQ.pushHead();
              }

            }
            TaskQ.consumeTail();
          }



          if(taskInfo->wait_checkpoint.timeout_ms)//try timeout
          {
            taskInfo->wait_checkpoint.timeout_ms--;
          }
          else
          {
            if(taskInfo->wait_checkpoint.ret_id!=-1)
            {

              struct Mstp2CommInfo tinfo={
              .type=Mstp2CommInfo_Type::respCheckpointFrame,
              .isAck=false,
              .resp_id=taskInfo->wait_checkpoint.ret_id
              };

              {
                Mstp2CommInfo* Qhead=NULL;
                while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
                *Qhead=tinfo;
                Mstp2CommInfoQ.pushHead();
              }
            }

            TaskQ.consumeTail();//failed
          }



          return;
        }
        
        TaskQ.consumeTail();//failed
        return;
      }

      case AUX_TASK_INFO::AUX_IO_CTRL:
      {
        auto &io_ctrl_info=taskInfo->io_ctrl;


        uint32_t *tarOutIOReg=io_ctrl_info.bank==0?&TMP_HACK_IO_Output0:&TMP_HACK_IO_Output1;

        uint32_t latest_pinout=*tarOutIOReg;
        uint32_t mask=io_ctrl_info.mask;
        latest_pinout&=~mask;//clear masked pins 
        latest_pinout|=mask&io_ctrl_info.pout; //put pinout in port

        *tarOutIOReg=latest_pinout;


        if(io_ctrl_info.cid==0)
        {
          TaskQ.consumeTail();//failed
          return;
        }

        
        auto ttagbf=io_ctrl_info.ttagbf;
        int index=0;
        bool isFirstTag=true;



        struct Mstp2CommInfo tinfo={
        .type=Mstp2CommInfo_Type::trigInfo,
        .trig_id=io_ctrl_info.tid,
        .cam_id=io_ctrl_info.cid,
        };
        char *str=tinfo.strinfo;
        str[0]='\0';

        while (ttagbf)
        {
          if(ttagbf&1)
          {
            if(isFirstTag==false)
            {
              str+=sprintf(str,",");
            }
            isFirstTag=false;

            std::string tag_str=camTriggerTagArr[index];
            const char *ctag=tag_str.c_str();


            if(strstr(ctag,"s_PLAT_LOC=")!=NULL)
            {
              float platLoc=MG_PLATE.getLatestLocation()[0];
              str+=sprintf(str,ctag,platLoc);
            }
            else if(strstr(ctag,"s_SYSTIME=")!=NULL)
            {
              str+=sprintf(str,ctag,SYS_TIME_ms());//unsigned long
            }
            else
            {
              str+=sprintf(str,ctag);
            }


          }
          index++;
          ttagbf>>=1;
        }

        Mstp2CommInfo* Qhead=NULL;
        while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);//maybe shouldn't do it here
        *Qhead=tinfo;
        Mstp2CommInfoQ.pushHead();


        TaskQ.consumeTail();//failed
        return;
      }





    }
  }


  AUX_TASK_INFO* spinWaitQ(int retryTimes=-1)
  {
    for(;retryTimes!=0;)
    {
      if(retryTimes>0)retryTimes--;
      
      AUX_TASK_INFO *taskInfo=TaskQ.getHead();
      if(taskInfo)return taskInfo;
    }
    return NULL;
  }

  int QSpace()
  {
    return TaskQ.space();
  }

  const int safeQSpace=5;
  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::TASK_UNSUPPORTED;
    }


    
    if(TaskQ.space()<safeQSpace)
    {
      if(cachedCMD[0]!='\0')return MCMD_Status::TASK_FAILED;//the cached CMD is not empty
      int slen=serializeJson(cmd, cachedCMD,sizeof(cachedCMD));
      djrl.dbg_printf("chache cmd slen:%d...",slen);
      return MCMD_Status::TASK_WAIT_HOLD_RSP;
    }


    const char* type = cmd["cmd"];

    if(strcmp(type,"DELAY")==0)
    {
      AUX_TASK_INFO *taskInfo=spinWaitQ();
      taskInfo->type=AUX_TASK_INFO::AUX_DELAY;
      taskInfo->delay.time_ms=jgetInt32(cmd,"time_ms",0);
      TaskQ.pushHead();
      return MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"WAIT_CHECKPOINT")==0)
    {
      AUX_TASK_INFO *taskInfo=spinWaitQ();
      taskInfo->type=AUX_TASK_INFO::AUX_WAIT_CHECKPOINT;
      taskInfo->wait_checkpoint.target=1;
      taskInfo->wait_checkpoint.checkpoint=jgetInt32(cmd,"checkpoint",MG_SIMP.checkpointAdeadCounter);
      taskInfo->wait_checkpoint.ret_id=jgetInt32(cmd,"id",-1);
      taskInfo->wait_checkpoint.timeout_ms=jgetInt32(cmd,"timeout",1000);
      TaskQ.pushHead();

      return taskInfo->wait_checkpoint.ret_id==-1?
        MCMD_Status::TASK_OK:
        MCMD_Status::TASK_WAIT_HOLD_RSP;
    }

    if(strcmp(type,"IO_CTRL")==0)
    {
      AUX_TASK_INFO *taskInfo=spinWaitQ();
      taskInfo->type=AUX_TASK_INFO::AUX_IO_CTRL;
      taskInfo->io_ctrl.bank=jgetInt32(cmd,"bank",0);
      taskInfo->io_ctrl.mask=jgetInt32(cmd,"mask",0);
      taskInfo->io_ctrl.pout=jgetInt32(cmd,"pout",0);
      taskInfo->io_ctrl.cid=jgetInt32(cmd,"cid",0);
      taskInfo->io_ctrl.tid=jgetInt32(cmd,"tid",0);
      taskInfo->io_ctrl.ttagbf=jgetInt32(cmd,"ttagbf",0);


      TaskQ.pushHead();
      return MCMD_Status::TASK_OK;
    }


    if(strcmp(type,"TRIG_CTRL_WAIT_PROGRESS")==0)
    {
    
      float target_progress = jgetNum(cmd, "target_progress");
      int approaching_dir = jgetInt32(cmd, "approaching_dir", -43243);
      int timeout = jgetInt32(cmd, "timeout", -1000);  // Default timeout of 1000 ms
      
      int bank=jgetInt32(cmd,"bank",0);
      int mask=jgetInt32(cmd,"mask",0);
      int pout=jgetInt32(cmd,"pout",0);



      if(target_progress!=target_progress || approaching_dir==-43243 || timeout<=0 || mask==0)
      {
        return MCMD_Status::TASK_FAILED;
      }



      {//wait 


        AUX_TASK_INFO *taskInfo = spinWaitQ();
        taskInfo->type = AUX_TASK_INFO::AUX_WAIT_PROGRESS;
        taskInfo->wait_progress.target_progress = target_progress;
        taskInfo->wait_progress.approaching_dir = approaching_dir;
        taskInfo->wait_progress.timeout = timeout;
        TaskQ.pushHead();

      }




      {//on
        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_IO_CTRL;
        taskInfo->io_ctrl.bank=bank;
        taskInfo->io_ctrl.mask=mask;
        taskInfo->io_ctrl.pout=pout;
        taskInfo->io_ctrl.cid=jgetInt32(cmd,"cid",0);
        taskInfo->io_ctrl.tid=jgetInt32(cmd,"tid",0);
        taskInfo->io_ctrl.ttagbf=jgetInt32(cmd,"ttagbf",0);
        TaskQ.pushHead();

      }


      {//wait

        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_DELAY;
        taskInfo->delay.time_ms=jgetInt32(cmd,"ontime",10);
        TaskQ.pushHead();
      }




      {//off
        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_IO_CTRL;
        taskInfo->io_ctrl.bank=bank;
        taskInfo->io_ctrl.mask=mask;
        taskInfo->io_ctrl.pout=~pout;
        taskInfo->io_ctrl.cid=0;
        taskInfo->io_ctrl.tid=0;
        taskInfo->io_ctrl.ttagbf=0;
        TaskQ.pushHead();

      }



      return MCMD_Status::TASK_OK;
    }


    if(strcmp(type,"TRIG_CTRL")==0)
    {
      {//wait
        int pretime=jgetInt32(cmd,"pretime",0);
        if(pretime>0)
        {
          AUX_TASK_INFO *taskInfo=spinWaitQ();
          taskInfo->type=AUX_TASK_INFO::AUX_DELAY;
          taskInfo->delay.time_ms=pretime;
          TaskQ.pushHead();

        }
      }


      int bank=jgetInt32(cmd,"bank",0);
      int mask=jgetInt32(cmd,"mask",0);
      int pout=jgetInt32(cmd,"pout",0);



      {//on
        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_IO_CTRL;
        taskInfo->io_ctrl.bank=bank;
        taskInfo->io_ctrl.mask=mask;
        taskInfo->io_ctrl.pout=pout;
        taskInfo->io_ctrl.cid=jgetInt32(cmd,"cid",0);
        taskInfo->io_ctrl.tid=jgetInt32(cmd,"tid",0);
        taskInfo->io_ctrl.ttagbf=jgetInt32(cmd,"ttagbf",0);
        TaskQ.pushHead();

      }


      {//wait

        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_DELAY;
        taskInfo->delay.time_ms=jgetInt32(cmd,"ontime",10);
        TaskQ.pushHead();
      }




      {//off
        AUX_TASK_INFO *taskInfo=spinWaitQ();
        taskInfo->type=AUX_TASK_INFO::AUX_IO_CTRL;
        taskInfo->io_ctrl.bank=bank;
        taskInfo->io_ctrl.mask=mask;
        taskInfo->io_ctrl.pout=~pout;
        taskInfo->io_ctrl.cid=0;
        taskInfo->io_ctrl.tid=0;
        taskInfo->io_ctrl.ttagbf=0;
        TaskQ.pushHead();

      }



      return MCMD_Status::TASK_OK;
    }

    return MCMD_Status::TASK_UNSUPPORTED;
  }




  int try_cachedCMDProcess(JsonDocument& blank_jobj,JsonDocument& cmd_ret)
  {
    if(cachedCMD[0]=='\0')return MCMD_Status::NOP;
    if(TaskQ.space()<=safeQSpace)return MCMD_Status::NOP;
    // djrl.dbg_printf("try_cachedCMDProcess");
    blank_jobj.clear();


    
    DeserializationError error = deserializeJson(blank_jobj, (const char*)cachedCMD);
    int ret=CMDProcess(blank_jobj,cmd_ret);
    // for(int i=0;cachedCMD[i];i++)
    // {
    //   if(cachedCMD[i]=='"')cachedCMD[i]='\'';
    // }
    // djrl.dbg_printf(">>>ret:%d",ret);
    cachedCMD[0]='\0';
    return ret;
  }
};





int GetPlateObjLocOnArmRZV(float* ret_ArmRZV, float*params_in_arm_coord)
{


  float platObjDomXYZ[3];
  float platObjArmXYZ[3];

  MG_PLATE.predictCurObj_DomainLocation(platObjDomXYZ, params_in_arm_coord);
  MG_SIMP.coord_dom2dev(platObjArmXYZ,platObjDomXYZ);//convert domain corrdinate to arm device corrdinate
  GetPlateObjLocOnArmRZV_DBG[0]=platObjArmXYZ[2];
  MG_SIMP.XYZ2RZV_w_offset(ret_ArmRZV,platObjArmXYZ);
  GetPlateObjLocOnArmRZV_DBG[1]=ret_ArmRZV[1];

  return 0;
}



int loopTimeSS=0;
int mockObjTrajectOnArmRZV(float* ret_ArmRZV, float*params_in_arm_coord)
{
  ret_ArmRZV[1]=0;
  int llK=(1<<12);
  loopTimeSS&=(llK-1);
  ret_ArmRZV[0]=-50+5*cos(loopTimeSS*2*M_PI/llK);
  ret_ArmRZV[2]=10+2*sin(loopTimeSS*2*M_PI/llK);

  return 0;
}

AUXTaskExecutor aux0;

int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode!=1 )return 0;


  doc.clear();
  retdoc.clear();
  DeserializationError error = deserializeJson(doc, (const char*)raw);
  bool rspAck=false;
  int NAKReason=-9999;
  bool doRsp=false;

  int segAvaSpace=-1;

  const char* type = doc["type"];
  // const char* id = doc["id"];
  if(strcmp(type,"RESET")==0)
  {
    
    return msg_printf("RESET_OK","");
  } 
  if(strcmp(type,"TG1")==0)//{"type":"TG1","exp":"1 + atan ( 5 , 6 ) ^ 2"}
  {
    const char* _exp = doc["exp"];


    std::string infix=_exp;
    std::string postfix= infixToPostfix(infix);

    djrl.dbg_printf("%s",postfix.c_str());

    doRsp=true;
  } 
  if(strcmp(type,"ECAT_ON")==0 || strcmp(type,"EON")==0)
  {
    rspAck=startECat();
    doRsp=true;
  } 
  if(strcmp(type,"ECAT_OFF")==0 || strcmp(type,"EOFF")==0)
  {
    rspAck=stopECat();
    doRsp=true;
  } 

  
  if(strcmp(type,"SYS_RESTART")==0)
  {
    doRsp=false;


    retdoc["id"]=(int)doc["id"];
    retdoc["ack"]=true;
    uint8_t buff[700];
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);
    
    io_outpb(0x64,0xfe);

  } 

  

  if(strcmp(type,"enable_periodic_print")==0)
  {
    enable_periodic_print=jgetBool(doc,"enable");
    rspAck=true;
    doRsp=true;
  } 


  
  if(strcmp(type,"MOFF")==0)
  {
    MG_SIMP.motor_enable(false);
    MG_PLATE.motor_enable(false);
    MG_PPU.motor_enable(false);
    motorS1.enable(false);
    motorReelAdv.enable(false);
    rspAck=true;
    doRsp=true;
  } 
  if(strcmp(type,"MON")==0)
  {
    MG_SIMP.motor_enable(true);
    MG_PLATE.motor_enable(true);
    MG_PPU.motor_enable(true);
    motorS1.enable();
    motorReelAdv.enable();
    rspAck=true;
    doRsp=true;
  } 
  else if(strcmp(type,"BYE")==0)
  {
    doRsp=rspAck=true;

  }      
  else if(strcmp(type,"SetTriggerIndexedTag")==0)
  {
    //camTriggerTagMap
    doRsp=true;
    rspAck=false;
    if(doc["idx"].is<int>()&&doc["tag"].is<std::string>())
    {
      int idx=doc["idx"];
      if(idx>=0 && idx<camTriggerTagArr.size())
      {
        camTriggerTagArr[idx]=doc["tag"].as<std::string>();
        rspAck=true;
      }
    }
  }
  else if(strcmp(type,"GetTriggerIndexedTag")==0)
  {
    //camTriggerTagMap
    doRsp=rspAck=true;
    int idx=-9898;
    if(doc["idx"].is<int>())
      idx=doc["idx"];
    if(idx>=0 && idx<camTriggerTagArr.size())
      retdoc["tag"]=camTriggerTagArr[idx];
    else if(idx==-9898)
    {//return all
      //make retdoc["tags"] as object

      JsonObject tags_obj = retdoc.createNestedObject("tags");
      for(int i=0;i<camTriggerTagArr.size();i++)
      {
        if(camTriggerTagArr[i].length()>0)
        {
          //convert i as string as key
          tags_obj[std::to_string(i)]=camTriggerTagArr[i];
        }
      }
    }
    else
    {
      rspAck=false;

    }


  }
  

  else if(strcmp(type,"m0")==0 )//{"type":"m0","cmd":"get_centre"}
  {
    rspAck=MG_PLATE.CMDProcess(doc,retdoc)==MCMD_Status::TASK_OK;
    doRsp=true;
  }
  
  else if(strcmp(type,"PPU")==0 )//{"type":"m0","cmd":"get_centre"}
  {
    NAKReason=MG_PPU.CMDProcess(doc,retdoc);
    rspAck=(NAKReason==MCMD_Status::TASK_OK);
    doRsp=true;
    segAvaSpace=MG_SIMP.MotorGroup.segs.space();
    if(NAKReason==MCMD_Status::TASK_WAIT_HOLD_RSP || NAKReason==MCMD_Status::TASK_OK_HOLD_RSP)
    {
      doRsp=false;
    }
  }
  
  
  else if(strcmp(type,"m1")==0 )//{"type":"m0","cmd":"get_centre"}
  {
    int ret=MG_SIMP.CMDProcess(doc,retdoc);
    rspAck=(ret==MCMD_Status::TASK_OK);
    doRsp=true;
    segAvaSpace=MG_SIMP.MotorGroup.segs.space();
    if(ret==MCMD_Status::TASK_WAIT_HOLD_RSP || ret==MCMD_Status::TASK_OK_HOLD_RSP)
    {
      doRsp=false;
    }
  }



  else if(strcmp(type,"AUX0")==0 )//{"type":"m0","cmd":"get_centre"}
  {
    int ret=aux0.CMDProcess(doc,retdoc);
    retdoc["sp"]=aux0.QSpace();
    rspAck=(ret==MCMD_Status::TASK_OK);
    doRsp=true;
    if(ret==MCMD_Status::TASK_WAIT_HOLD_RSP || ret==MCMD_Status::TASK_OK_HOLD_RSP)
    {
      doRsp=false;
    }

  }
  


  
  // else if(AUX_Task_Try_Read(doc,type,retdoc,doRsp,rspAck))
  // {
  // }


  if(doRsp)
  {
    retdoc["id"]=(int)doc["id"];
    retdoc["ack"]=rspAck;
    if(segAvaSpace>=0)
      retdoc["sp"]=segAvaSpace;
    
    uint8_t buff[700];
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);

  }


  return 0;


}




void MData_JR::loop()
{

}




void loopTaskQ_tryStagedCMD()
{
  do
  {
    doc.clear();
    retdoc.clear();
    bool rspAck=false;
    bool doRsp=false;
    int segAvaSpace=0;



    {
      int ret = MG_SIMP.try_cachedCMDProcess(doc,retdoc);
      if(ret==MCMD_Status::NOP)break;
      if(ret==MCMD_Status::TASK_OK_HOLD_RSP)break;
      if(ret==MCMD_Status::TASK_WAIT_HOLD_RSP)break;
      doRsp=true;
      segAvaSpace=MG_SIMP.MotorGroup.segs.space();
      if(ret==MCMD_Status::TASK_OK)
      {
        rspAck=true;
      }
    }
    if(doRsp)
    {
      retdoc["id"]=(int)doc["id"];
      retdoc["ack"]=rspAck;
      retdoc["segAvaSpace"]=segAvaSpace;

      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      djrl.send_json_string(0,buff,slen,0);

    }

  }while(0);


  do{
    doc.clear();
    retdoc.clear();
    bool rspAck=false;
    bool doRsp=false;
    int QAvaSpace=0;



    {
      int ret = aux0.try_cachedCMDProcess(doc,retdoc);
      if(ret==MCMD_Status::NOP)break;
      if(ret==MCMD_Status::TASK_OK_HOLD_RSP)break;
      if(ret==MCMD_Status::TASK_WAIT_HOLD_RSP)break;
      doRsp=true;
      QAvaSpace=aux0.TaskQ.space();
      if(ret==MCMD_Status::TASK_OK)
      {
        rspAck=true;
      }
    }
    if(doRsp)
    {
      retdoc["id"]=(int)doc["id"];
      retdoc["ack"]=rspAck;
      retdoc["QAvaSpace"]=QAvaSpace;

      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      djrl.send_json_string(0,buff,slen,0);

    }

  }while(0);





}


// EthercatDevice_Generic dmp0;
EthercatDevice_DmpDIQ_Generic dmp0;
EthercatDevice_Generic dmp1;
float inputL0=0;
float inputL1=0;

void myCallback()
{

  TMP_HACK_IO_Input0=dmp0.digitalReadAll();
  TMP_HACK_IO_Input1=dmp1.pdoRead16(0);
  loopTimeSS++;
  aux0.update();


  {

    bool isEveryMotorOK=true;
    do{
      if(motorR.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
      if(motorZ.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
      if(motorV.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
      if(motorS1.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
      if(motorReelAdv.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
      
      if(motorPPU.state!=MotorCtrlCiA402::RunningState::running)
      {
        isEveryMotorOK=false;
        break;
      }
    }while(0);
    isEveryMotorOK=true;
    if(isEveryMotorOK==false)
    {
      MG_PLATE.motor_enable(false);
      MG_SIMP.motor_enable(false);
      MG_PPU.motor_enable(false);
      motorS1.enable(false);
      motorReelAdv.enable(false);
    }
    else
    {
      MG_PLATE.update();
      MG_SIMP.update();
      MG_PPU.update();
      


      const vector<int32_t>*plat_mot_adv =MG_PLATE.updateMotor();
      const vector<int32_t>*simp_mot_adv =MG_SIMP .updateMotor();
      const vector<int32_t>*ppu_mot_adv  =MG_PPU  .updateMotor();

      if(plat_mot_adv)
      {
        motorPLATE.update(plat_mot_adv->at(0));
      }

      if(simp_mot_adv)
      {
        motorR.update(simp_mot_adv->at(0));
        motorZ.update(simp_mot_adv->at(1));
        motorV.update(simp_mot_adv->at(2));
        motorS1.update(simp_mot_adv->at(3));
        motorReelAdv.update(simp_mot_adv->at(4));
      }
      if(ppu_mot_adv)
      {
        motorPPU.update(ppu_mot_adv->at(0));
      }

    }

  }

  dmp0.digitalWriteAll(TMP_HACK_IO_Output0);
  dmp1.pdoWrite16(0,TMP_HACK_IO_Output1);
}

void setup()
{
//	EVA.begin();
  Serial.begin(115200); // Initialize Serial communication
  Serial.setTimeout(0,0);
  while (!Serial); // Wait for the serial port to connect
  // delay(5000);

  {
    motorR.encStepPerRev=8192;
    motorR.allowedError=70;

    motorZ.encStepPerRev=8192*2;
    motorZ.allowedError=70*5;

    motorV.encStepPerRev=8192*2;
    motorV.allowedError=70*5;

    motorR.motor_skip_adjust_thres=
    motorV.motor_skip_adjust_thres=
    motorZ.motor_skip_adjust_thres=40;



    motorPLATE.encoder_check=false;

    motorS1.encoder_check=false;
    motorReelAdv.encoder_check=false;

    motorPPU.gearRatioB=6400;//64pulse/rev
  }
}




void printODList(EthercatDevice *device,int paramBaseAddr=0x1800,int setupBaseAddr=0x1A00)
{

  {
    int paramA=paramBaseAddr;
    int setupA=setupBaseAddr;
    int maxSubIdx=device->sdoUpload8(paramA,0);
    int COB_ID=device->sdoUpload32(paramA,1);
    int TransType=device->sdoUpload8(paramA,2);
    int InhibitTime=device->sdoUpload16(paramA,3);

    int EventTimer=device->sdoUpload16(paramA,5);



    int len=device->sdoUpload8(setupA,0);

    djrl.dbg_printf("pdoG[p:%04X s:%04X]:len:%d==cob_id:%X TransType:%X InhibitTime:%d EventTimer:%d  ==",
    paramBaseAddr,setupBaseAddr,len,
    COB_ID,TransType,InhibitTime,EventTimer);

    for(int j=0;j<len;j++)
    {
      uint32_t mapping= device->sdoUpload32(setupA,j+1);
      {
          uint16_t mappedIndex = (mapping >> 16) & 0xFFFF;
          uint8_t mappedSubIndex = (mapping >> 8) & 0xFF;
          uint8_t bitLength = mapping & 0xFF;
          djrl.dbg_printf("  [%d]:Mapped Index: 0x%X, SubIdx: %d, BitLen: %d",
                 j, mappedIndex, mappedSubIndex, bitLength);
      }
    }

  }

  // djrl.dbg_printf("getODlist_ReadListSize:%d",
  //     device->getODlist_ReadListSize()
  //   );
}

// void printPdoList(EthercatDevice *device) {
//     uint32_t abortcode;

    
//     const uint16_t RxPdoStartIndex = 0x1600;
//     const uint16_t TxPdoStartIndex = 0x1A00;
//     const uint16_t PdoEndIndex = 0x1FFF;

//     djrl.dbg_printf("PDO List for EtherCAT Device:\n");

    
//     for (uint16_t index = RxPdoStartIndex; index <= PdoEndIndex; index++) {
//         uint8_t maxSubIndex = 0;
//         char pdoName[128] = {0};

        
//         int result = device->getObjectDescription(index, nullptr, &maxSubIndex, nullptr, pdoName, sizeof(pdoName), &abortcode);
//         if (result != 0) {
            
//             continue;
//         }

//         djrl.dbg_printf("PDO Index: 0x%X, Name: %s", index, pdoName);

        
//         for (uint8_t subIndex = 0; subIndex <= maxSubIndex; subIndex++) {
//             uint32_t mapping;
//             result = device->sdoUpload32(index, subIndex, &mapping);
//             if (result == 0) {
//                 uint16_t mappedIndex = (mapping >> 16) & 0xFFFF;
//                 uint8_t mappedSubIndex = (mapping >> 8) & 0xFF;
//                 uint8_t bitLength = mapping & 0xFF;
//                 djrl.dbg_printf("  SubIndex: %d -> Mapped Index: 0x%X, SubIndex: %d, BitLength: %d",
//                        subIndex, mappedIndex, mappedSubIndex, bitLength);
//             } else {
//                 djrl.dbg_printf("  SubIndex: %d -> [Error reading mapping]", subIndex);
//             }
//         }
//     }


//     return;
    
//     // for (uint16_t index = TxPdoStartIndex; index <= PdoEndIndex; index++) {
//     //     uint8_t maxSubIndex = 0;
//     //     char pdoName[128] = {0};

    
//     //     int result = device->getObjectDescription(index, nullptr, &maxSubIndex, nullptr, pdoName, sizeof(pdoName), &abortcode);
//     //     if (result != 0) {
    
//     //         continue;
//     //     }

//     //     djrl.dbg_printf("PDO Index: 0x%X, Name: %s", index, pdoName);

    
//     //     for (uint8_t subIndex = 0; subIndex <= maxSubIndex; subIndex++) {
//     //         uint32_t mapping;
//     //         result = device->sdoUpload32(index, subIndex, &mapping);
//     //         if (result == 0) {
//     //             uint16_t mappedIndex = (mapping >> 16) & 0xFFFF;
//     //             uint8_t mappedSubIndex = (mapping >> 8) & 0xFF;
//     //             uint8_t bitLength = mapping & 0xFF;
//     //             djrl.dbg_printf("  SubIndex: %d -> Mapped Index: 0x%X, SubIndex: %d, BitLength: %d",
//     //                    subIndex, mappedIndex, mappedSubIndex, bitLength);
//     //         } else {
//     //             djrl.dbg_printf("  SubIndex: %d -> [Error reading mapping]" subIndex);
//     //         }
//     //     }
//     // }
// }

// void printEthercatDeviceInfo(EthercatDevice *device) {
//     uint32_t abortcode;
//     uint16_t od_list[256];  // Assuming a max of 256 objects in the OD

//     // Step 1: Get the Object Dictionary List
//     int length = device->getODlist(od_list, sizeof(od_list) / sizeof(od_list[0]), &abortcode);
//     if (length <= 0) {
//         djrl.dbg_printf("Failed to get OD list, len:%d abort code: 0x%X",length, abortcode);
//         return;
//     }

//     // Print general device information
//     djrl.dbg_printf("EtherCAT Device Information:");
//     djrl.dbg_printf("Vendor ID: 0x%X", device->getVendorID());
//     djrl.dbg_printf("Product Code: 0x%X", device->getProductCode());
//     djrl.dbg_printf("---------------------------------------------------");

//     // Step 2: Iterate through the OD list and print object descriptions
//     for (size_t i = 0; i < length; i++) {
//         uint16_t datatype;
//         uint8_t max_od_subindex;
//         uint8_t objcode;
//         char objname[128] = {0};

//         int result = device->getObjectDescription(od_list[i], &datatype, &max_od_subindex, &objcode, objname, sizeof(objname), &abortcode);
//         if (result != 0) {
//             djrl.dbg_printf("Failed to get description for OD index 0x%X, abort code: 0x%X", od_list[i], abortcode);
//             continue;
//         }

//         djrl.dbg_printf("---------------------------------------------------");
//         djrl.dbg_printf("ObjIdx: 0x%X type:0x%X ocode:0x%X ", od_list[i],datatype,objcode);
//         // djrl.dbg_printf("ObjectName: %s MaxSubindex: %d", objname,max_od_subindex);

//         // Step 3: Retrieve and print entry descriptions for each subindex
//         for (uint8_t subindex = 0; subindex <= max_od_subindex; subindex++) {
//             uint8_t valueinfo;
//             uint16_t bitlength;
//             uint16_t objaccess;
//             char entryname[128] = {0};

//             result = device->getEntryDescription(od_list[i], subindex, &valueinfo, &datatype, &bitlength, &objaccess, entryname, sizeof(entryname), &abortcode);
//             if (result != 0) {
//                 djrl.dbg_printf("Failed to get entry description for OD index 0x%X subindex %d, abort code: 0x%X", od_list[i], subindex, abortcode);
//                 continue;
//             }

//             djrl.dbg_printf("__subindex:[%d]",subindex);
//             djrl.dbg_printf("  ValueInfo: 0x%X   DataType: 0x%X", valueinfo,datatype);
//             djrl.dbg_printf("  BitLength: %d ObjectAccess: 0x%X EntryName: %s", bitlength,objaccess,entryname);
//         }
//     }
// }



bool startECat()
{

  if(isECatStarted==true)return false;
  // Initialize the EtherCAT Master. All slaves enter PRE-OPERATIONAL state if successful

  djrl.dbg_printf("master.begin:%d",master.begin());
  uint32_t syncT_ns=updatePeriod_s*1000000000;

  int slaveCount=master.getSlaveCount();
  for(int i=0;i<slaveCount;i++)
  {
    djrl.dbg_printf("=========slave :[%d]=========",i);

    djrl.dbg_printf("AliasAddress:%d",master.getAliasAddress(i));
    djrl.dbg_printf("VendorID:%d ProductCode:%d",master.getVendorID(i),master.getProductCode(i));
    djrl.dbg_printf("RevisionNumber:%d SerialNumber:%d",master.getRevisionNumber(i),master.getSerialNumber(i));

    


  }
  // djrl.dbg_printf("try write aliasAddr ret:%d",master.writeSII_AliasAddress(1,1234));

  djrl.dbg_printf("=========slave  Count:%d =========",master.getSlaveCount());
  
  //Configure each motor in the array




  MG_SIMP.motor_attach(master);
  MG_PLATE.motor_attach(master);
  MG_PPU.motor_attach(master);

  motorS1.attach(master);
  motorReelAdv.attach(master);

  // printPdoList(&(MG_SIMP.motorPPU.motdrv));
  //  printEthercatDeviceInfo(&(MG_SIMP.motorPPU.motdrv));

   {
      djrl.dbg_printf("attach IO0 ret:%d",
        dmp0.attach(IO0_ECAT_STATION_ID, master));

      djrl.dbg_printf(" IO0 alias Address:%d",
        dmp0.getAliasAddress());
      djrl.dbg_printf("setDC  IO0 ret:%d",
        dmp0.setDc(syncT_ns));



      djrl.dbg_printf("attach IO1 ret:%d",
        dmp1.attach(IO1_ECAT_STATION_ID, master));

      djrl.dbg_printf(" IO1 alias Address:%d",
        dmp1.getAliasAddress());
      djrl.dbg_printf("setDC  IO1 ret:%d",
        dmp1.setDc(syncT_ns));

   }
   djrl.dbg_printf("attachCyclicCallback");
   master.attachCyclicCallback(myCallback); // Register a cyclic callback function

   int ret=master.start(syncT_ns, ECAT_SYNC); // Start the EtherCAT Master. All slaves enter OPERATIONAL state if successful
   djrl.dbg_printf("master.start:%d",ret);
   
   djrl.dbg_printf("driveEnable");



   MG_SIMP.motor_enable();
   MG_PLATE.motor_enable();
   MG_PPU.motor_enable();
   motorS1.enable();
   motorReelAdv.enable();
   isECatStarted=true;

   
  //  printODList(&(motorPPU.motdrv),0x1800,0x1A00);
  //  printODList(&(motorPPU.motdrv),0x1801,0x1A01);
  //  printODList(&(motorPPU.motdrv),0x1802,0x1A02);
  //  printODList(&(motorPPU.motdrv),0x1803,0x1A03);


  //  printODList(&(motorPPU.motdrv),0x1800,0x1600);
  //  printODList(&(motorPPU.motdrv),0x1801,0x1601);
  //  printODList(&(motorPPU.motdrv),0x1802,0x1602);
  //  printODList(&(motorPPU.motdrv),0x1803,0x1603);


   return true;
}


bool stopECat()
{
  if(isECatStarted==false)return false;
  djrl.dbg_printf("motor disable");
  MG_SIMP.motor_enable(false);
  MG_PPU.motor_enable(false);
  MG_PLATE.motor_enable(false);
  motorS1.enable(false);
  motorReelAdv.enable(false);
  
  djrl.dbg_printf("master.stop()");
  master.stop();
  delay(100);
  djrl.dbg_printf("detachCyclicCallback");
  master.detachCyclicCallback();
  delay(100);
  
  dmp0.detach();
  dmp1.detach();

  djrl.dbg_printf("CNC detatch");
  MG_SIMP.motor_detach();
  MG_PLATE.motor_detach();
  MG_PPU.motor_detach();
  motorS1.detach();
  motorReelAdv.detach();
  isECatStarted=false;
  master.end();
  return true;
}

static uint8_t recvBuf[20];



void loop()
{
  SYS_TIME_ms();
  static unsigned long taskRunTime=5000;
  static int32_t preInputL=0;
  if (enable_periodic_print && millis()>taskRunTime){

    taskRunTime+=1000; // Reset timer
  


    djrl.dbg_printf("MG_SIMP dDiff:%0.3f ,size:%d sp:%d cap:%d",
    MG_SIMP.dDiff,
    MG_SIMP.motor_FilterBuffer.size(),
    MG_SIMP.motor_FilterBuffer.space(),
    MG_SIMP.motor_FilterBuffer.capacity()
    );


    // 
    // preInputL=inputL0;
    // djrl.dbg_printf("motorV:a:%d,v:%d cpC:%d PPU.prog:%0.3f",
    // MG_SIMP.dbg_maxStepDiff,
    // MG_SIMP.dbg_maxVStep,
    // MG_SIMP.segs.size(),
    // MG_PPU.progressiveProgress);

    // djrl.dbg_printf("motor st:%d,%d,%d",
    // MG_SIMP.motorR.state,MG_SIMP.motorR.stableCD,MG_SIMP.motorR.init_enc_offset,
    // MG_SIMP.motorZ.state,
    // MG_SIMP.motorV.state);
    

    // static int outputTest=0;
    // outputTest++;
    // motorPPU.motdrv.sdoDownload8(0x2406,0,outputTest&0x3);
    if(0)
    {
    int addr=0x40C;
    addr=0x032;
    int value=0x1;



    uint8_t pdoBufferL=motorPPU.motdrv.pdoGetInputBytes();
    uint8_t *pdoBuffer=motorPPU.motdrv.pdoGetInputBuffer();
    
    uint8_t pdoBufferOutputL=motorPPU.motdrv.pdoGetOutputBytes();
    uint8_t *pdoBufferOutput=motorPPU.motdrv.pdoGetOutputBuffer();
    if(pdoBuffer && pdoBufferL){

      
      djrl.dbg_DIRECT_PRINT("{\"isize\":%d,\"d\":[",pdoBufferL);

      for(int i=0;i<pdoBufferL;i++)
      {

        djrl.dbg_DIRECT_PRINT("%s%02X%s",

        i%32==0?"\n":"",
        pdoBuffer[i],
        i==(pdoBufferL-1)?"":","
        );
      }
      djrl.dbg_DIRECT_PRINT("]}\n");


      

      
      djrl.dbg_DIRECT_PRINT("{\"osize\":%d,\"d\":[",pdoBufferOutputL);
      for(int i=0;i<pdoBufferOutputL;i++)
      {

        djrl.dbg_DIRECT_PRINT("%s%02X%s",

        i%32==0?"\n":"",
        pdoBufferOutput[i],
        i==(pdoBufferOutputL-1)?"":","
        );
      }
      djrl.dbg_DIRECT_PRINT("]}\n");

    }



    djrl.dbg_printf("motorPPU:ctrl:%X st:%X mod:%X er:%X",
       motorPPU.motdrv.driveGetControlword(),
       motorPPU.motdrv.driveGetState(),
       motorPPU.motdrv.driveGetMode(),
       motorPPU.motdrv.driveGetErrorCode()

      //  MG_SIMP.motorPPU.motdrv.sdoDownload16(addr,0,value),
       
      //  MG_SIMP.motorPPU.motdrv.pdoWrite16(addr,value)
      // motorPPU.motdrv.driveGet
      
      );

    }


    // djrl.dbg_printf("motorPPU:input:%X  CCCx:%X 0x60FD:%X pdoBufferL:%d inputIO:%08X",
    //    motorPPU.motdrv.driveGetDigitalInputs(),
    //    MG_PPU.CCCx,
    //    motorPPU.motdrv.sdoUpload32(0x60FD,0),
    //    pdoBufferL,TMP_HACK_IO_Input0
    //   );
    // if(MG_PPU.homingSeq.size()>0)
    // {
    //   auto homeNode=MG_PPU.homingSeq[0];
    //   djrl.dbg_printf("home status:%d  sensor:%d == %d  counterddd:%d",
    //     homeNode.status,
    //     (MG_PPU.getlimitSensor()>>homeNode.sensorPin)&1,
    //     homeNode.sensorSense,
    //     MG_PPU.counterddd
    //     );
    // }


    // djrl.dbg_printf("motorPPU:%d,%d,%d  %d,%d,%d,%d",
    //    motorPPU.motdrv.driveGetPositionActualValue(),
    //    motorPPU.motdrv.driveGetPositionAcutalInternalValue(),
    //    motorPPU.motdrv.driveGetAdditionalActualPosition(),

    //    motorPPU.motdrv.driveGetTargetPosition(),
    //   motorPPU.motdrv.driveGetInternalPositionDemandValue(),
    //     motorPPU.MnPosAcc,

    //     motorPPU.waitForStableCounter

    //   //  MG_SIMP.motorPPU.motdrv.sdoDownload16(addr,0,value),
       
    //   //  MG_SIMP.motorPPU.motdrv.pdoWrite16(addr,value)
    //   // motorPPU.motdrv.driveGet
      
    //   );

      
      // motorPPU.motdrv.driveSetFaultReactionOptionCode(0x0000);
        // MG_SIMP.motorPPU.motdrv.sdoDownload32(0x6093,1,100)
        // MG_SIMP.motorPPU.motdrv.sdoDownload32(0x6093,2,1)
    // MG_SIMP.motorPPU.motdrv.__updateInputPDO__
    // MG_SIMP.motorPPU.motdrv.sdoDownload16(0x40D,0,2);
  }

  loopTaskQ_tryStagedCMD();

  {
    bool recvF=false;
    int ava=0;
    while((ava=Serial.available()) > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      // int recvLen = Serial.readBytes(recvBuf,sizeof(recvBuf));
      int recvLen = Serial.readBytes(recvBuf,1);
      // djrl.dbg_printf("ava:%d rL:%d",ava,recvLen);
      djrl.recv_data((uint8_t*)recvBuf,recvLen);

      // if(doDataLog)
      // {
      //   for(int i=0;i<recvLen;i++) 
      //   {
      //     if(recvBuf[i]=='"')
      //       recvBuf[i]='\'';
      //   }     
      //   recvBuf[recvLen]='\0';
      //   djrl.dbg_printf(">%s",recvBuf);
      // }

    }
    if(recvF)
    {
      // djrl.dbg_printf("recv DONE");
    }
  }


  if(1){



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


      // xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);


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
        case Mstp2CommInfo_Type::respCheckpointFrame :
        {


          retdoc["id"]=info.resp_id;
          retdoc["ack"]=info.isAck;

          if(info.isAck && info.type==Mstp2CommInfo_Type::respCheckpointFrame)
          {
            retdoc["interval"]=info.checkPointInterval;
            retdoc["sp"]=info.segQSpace;

          }
          if(info.strinfo[0]!='\0')
          {
            retdoc["msg"]=info.strinfo;
          }
          
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case Mstp2CommInfo_Type::trigInfo :
        {
          retdoc["cid"]=info.cam_id;
          retdoc["tid"]=info.trig_id;
          
          retdoc["tag"]=info.strinfo;
          
          retdoc["ack"]=info.isAck;
          retdoc["type"]="bTrig";
          
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
