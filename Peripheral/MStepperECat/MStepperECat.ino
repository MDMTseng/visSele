
// #include <Test.h>
#include <MSteppersV2.h>
#include <string>
#undef min
#undef max
#include "TestGround.h"
#include "ArduinoJson.h"
#include <Data_Layer_Protocol.h>
#include "SCoop.h"
//#include "tinyexpr.h"
/* QEC CiA402 CSP Mode Example */
#include "Ethercat.h" // Include the EtherCAT Library




enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,
  HALT_UNTIL_TRACKING_SATTLED=14,
  ASSERT_TRACKING_SATTLED=15,

  TRACKING_PROGRESS=50,

  GENERIC=999
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

#define AUX_COUNT 5
// static QueueHandle_t AUXTaskQueue[AUX_COUNT];
RingBuf_Static<struct Mstp2CommInfo,20,uint8_t> AUX2CommInfoQ;


struct MSTP_SegCtx_IOCTRL{
  uint32_t PORT=0,S=0;
  int32_t P=0,T=0;
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



bool isECatStarted=false;
bool startECat();
bool stopECat();

const int SegCtxSize=10;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);


int CB_Count=0;
int HACK_cur_cmd_id=-1;

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


uint32_t TMP_HACK_latest_input_pins=0;
uint32_t TMP_HACK_static_Pin_info0=0;

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

  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


  int insertCMD(StpGroup *stpG,JsonDocument &cmd);

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




int MData_JR::insertCMD(StpGroup *stpG,JsonDocument &cmd)
{


  int res=-2;
  // check if the key "code" exists
  if(cmd["code"].is<const char*>())
  {
    // gcpm.putJSONNote(&retdoc);
    // gcpm.putTargetStepperGroup(stpG);

    // const char* code = cmd["code"];

    //   // __PRT_I_("insertCMD:%s",code);
    // // res= gcpm.runGCode(code);

    // gcpm.putJSONNote(NULL);
    // gcpm.putTargetStepperGroup(NULL);
  }
  else
  {
    // res=stpG->JCMDParse(cmd,retdoc);
  }

  // retdoc["cmd"]=cmd;

  return res;
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
  // for(int i=0;i<mstpV2.stepperGroup.size();i++)
  // {//check for buffered G cmd

  //   StpGroup *stpG=mstpV2.stepperGroup[i];
  //   if(stpG->bufferJCMD_ID<0)continue;
  //   if(stpG->segs.space()<GCMD_Magic_SafeSpace)continue;

  //   bool rspAck=false;
  //   bool doRsp=true;

  //   int cmd_id=stpG->bufferJCMD_ID;
  //   stpG->bufferJCMD_ID=-1;


  //   DeserializationError error = deserializeJson(doc, (char*)stpG->bufferJCMD_raw);
  //   auto retSt=MCMD_Status::TASK_OK;//insertCMD(stpG,doc);
  //   if(retSt==MCMD_Status::TASK_OK)
  //   {
  //     rspAck=true;
  //   }
  //   else if(retSt==MCMD_Status::TASK_OK_HOLD_RSP)
  //   {
  //     rspAck=true;
  //     doRsp=false;
  //   }
  //   else
  //   {
  //     rspAck=false;
  //   }


  //   if(doRsp)
  //   {
  //     retdoc["id"]=cmd_id;
  //     retdoc["ack"]=rspAck;
      
  //     // uint8_t buff[700];
  //     int slen=serializeJson(retdoc, (char*)stpG->bufferJCMD_raw,sizeof(stpG->bufferJCMD_raw));
  //     send_json_string(0,(uint8_t*)stpG->bufferJCMD_raw,slen,0);

  //   }
  // }


}


float updatePeriod_s=0.001;



typedef enum { 

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
  
  std::vector<int32_t>mot_step_vec;
public:
  float cx,cy;
  StpGroup_PLATE():StpGroup(),mot_step_vec(1)
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
  float acc=100/4*3.14159/180;//5 deg/s
  float cur_speed=0;//5 deg/s

  bool updated=false;
  void update()//every system tick, update the location
  {
    if(cur_speed<tar_speed)
    {
      cur_speed+=acc*updatePeriod_s;
      if(cur_speed>tar_speed)cur_speed=tar_speed;
    }
    if(cur_speed>tar_speed){
      cur_speed-=acc*updatePeriod_s;
      if(cur_speed<tar_speed)cur_speed=tar_speed;
    }
    latestAdvLocation.vec[0]+=cur_speed*updatePeriod_s;//in rad
    updated=true;
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
  virtual const std::vector<int32_t>& getMotMoveVec()
  { 
    if(updated==false)return mot_step_vec;
    CMDVec curMotLoc_unit={0};
    inverse(curMotLoc_unit.vec,latestAdvLocation.vec);

    MotVec_i _curMotLoc_pls={0};
    // _curMotLoc_pls.vec[0]=(int32_t)(curMotLoc_unit.vec[0]*1600*4.5/(73*2*3.14159));//R  arc angle to steps

    motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);



    mot_step_vec[0]=-(_curMotLoc_pls.vec[0]-preMotloc_pls.vec[0]);//invert polarity



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

    updated=false;
    return mot_step_vec;
  }

  void inverse(float *mot_vec_dst,const float* loc_vec_src){
    mot_vec_dst[0]=loc_vec_src[0];
  }
  void forward(float* loc_vec_dst,const float *mot_vec_src)
  {
    loc_vec_dst[0]=mot_vec_src[0];
  }

  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {

    if(!cmd["cmd"].is<const char*>())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }
    const char* cmd_type = cmd["cmd"];


    if(strcmp(cmd_type,"cur_angle")==0)
    {
      cmd_ret["angle"]=latestAdvLocation.vec[0];
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


    if(strcmp(cmd_type,"set_centre")==0)
    {
      if(!cmd["x"].is<float>()||!cmd["y"].is<float>())
      {
        return MCMD_Status::PARAM_PARSE_ERROR;
      }
      cx=cmd["x"];
      cy=cmd["y"];
      return MCMD_Status::TASK_OK;
    }



    if(strcmp(cmd_type,"get_centre")==0)
    {
      cmd_ret["cx"]=cx;
      cmd_ret["cy"]=cy;
      return MCMD_Status::TASK_OK;
    }

    cmd_ret["cmd"]=cmd_type;

    
    return MCMD_Status::TASK_UNSUPPORTED;


  }
};

StpGroup_PLATE SG_PLATE;



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
    float T=updatePeriod_s;
    switch (hP.status) 
    {
    case Start:
      hP.status=Back;
      break;
    case Back:
        // if(ccc--<0)
        if(((TMP_HACK_latest_input_pins>>hP.sensorPin)&1)==hP.sensorSense)
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
        if(((TMP_HACK_latest_input_pins>>hP.sensorPin)&1)!=hP.sensorSense)
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
        if(((TMP_HACK_latest_input_pins>>hP.sensorPin)&1)==hP.sensorSense)
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
      float timeLeft=updatePeriod_s;

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

        float T = updatePeriod_s;

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

  // virtual void getMotMoveVec(vector<float> &mot_vec_dst)
  // {

  //   if(homingCMDID==-1 && homingFlag==true)
  //   {
  //     // xVec_f curMotLoc_unit={0};
  //     inverse(curMotLoc_unit.vec,latestAdvLocation_postTracking.vec);
  //   }
  //   else//in homing mode
  //   {

  //   }
  //   MotVec_i _curMotLoc_pls={0};
  //   // _curMotLoc_pls.vec[0]=(int32_t)(curMotLoc_unit.vec[0]*1600*4.5/(73*2*3.14159));//R  arc angle to steps

  //   motUnit2PulseConversion(_curMotLoc_pls.vec,curMotLoc_unit.vec);



  //   mot_vec_dst[0]=-(_curMotLoc_pls.vec[0]-preMotloc_pls.vec[0]);//invert polarity
  //   mot_vec_dst[1]=-(_curMotLoc_pls.vec[1]-preMotloc_pls.vec[1]);//Z
  //   mot_vec_dst[2]=-(_curMotLoc_pls.vec[2]-preMotloc_pls.vec[2]);//invert polarity
  //   // mot_vec_dst->vec[3]=_curMotLoc_stp.vec[3]-preMotloc.vec[3];
  //   // mot_vec_dst->vec[4]=_curMotLoc_stp.vec[4]-preMotloc.vec[4];
  //   preMotloc_pls=_curMotLoc_pls;

  // }

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
      TMP_HACK_static_Pin_info0|=((uint32_t)1)<<p_res->IO_CTRL.P;
    }
    else
    {
      TMP_HACK_static_Pin_info0&=~(((uint32_t)1)<<p_res->IO_CTRL.P);
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


  bool CheckHead(const char *str1,const char *str2)
  {
    return strncmp(str1, str2, strlen(str2))==0;
  }
  // int GcodeParse(char **blkIdxes,int blkIdxesL,JsonDocument& cmd_ret)
  // {

    
  //   if(blkIdxesL==0)
  //     return MCMD_Status::LINE_EMPTY;
  //   MCMD_Status retStatus=MCMD_Status::LINE_EMPTY;




  //   char *cblk=blkIdxes[0];
  //   // int cblkL=blks[1]-blks[0];

  //   blkIdxes++;//move to next block
  //   blkIdxesL--;


  //   if(cblk[0]=='G')
  //   {
  //     if(CheckHead(cblk, "G28"))//G28 GO HOME!!!:
  //     {
  //       // homingStatus=HomingStatus::Start;
  //       // homingAxisIdx=0;
  //       homingSeq.clear();
  //       float fineSpeed=1;
  //       homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=1,.speed=50,.speed_fine=fineSpeed,.sensorPin=1,.sensorSense=1});
  //       homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=0,.speed=50*5/Arm_r,.speed_fine=fineSpeed*5/Arm_r,.sensorPin=0,.sensorSense=0});
  //       homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=-50,.speed_fine=-fineSpeed,.sensorPin=2,.sensorSense=0});
  //       // homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=1,.speed=5,.speed_fine=1,.sensorPin=1,.sensorSense=0});
  //       // homingSeq.push_back({.status=HomingStatus::Start,.axisIdx=2,.speed=5,.speed_fine=1,.sensorPin=2,.sensorSense=0});

    

  //       homingCMDID=HACK_cur_cmd_id;

  //       return  MCMD_Status::TASK_OK_HOLD_RSP;
  //     }
  //     if(homingFlag==false)
  //     {
  //       cmd_ret["msg"]="homingFlag==false";
  //       return MCMD_Status::TASK_FATAL_FAILED;
  //     }
  //     if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))
  //     {
  //       // __PRT_D_("G1 baby!!!\n");


  //       CMDVec vec_coord=latestCMDLocation;
  //       float Tmp=NAN;



  //       Tmp=NAN;
  //       FindFloat("X",blkIdxes,blkIdxesL,Tmp);
  //       if(Tmp==Tmp)vec_coord.vec[0]=Tmp;

  //       Tmp=NAN;
  //       FindFloat("Y",blkIdxes,blkIdxesL,Tmp);
  //       if(Tmp==Tmp)vec_coord.vec[1]=Tmp;
        
  //       Tmp=NAN;
  //       FindFloat("Z",blkIdxes,blkIdxesL,Tmp);
  //       if(Tmp==Tmp)vec_coord.vec[2]=Tmp;





  //       // if(z==z)vec_coord.vec[2]=z;


  //       // xVec_f vec_mot=foward(vec_coord);

  //       MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);

  //       if(exinfo.acc!=exinfo.acc)exinfo.acc=latestExtInfo.acc;
  //       if(exinfo.deacc!=exinfo.deacc)exinfo.deacc=latestExtInfo.deacc;
  //       if(exinfo.speed!=exinfo.speed)exinfo.speed=latestExtInfo.speed;

  //       if(exinfo.cornorR!=exinfo.cornorR)exinfo.cornorR=latestExtInfo.cornorR;
  //       if(exinfo.speedOnAxisIdx!=exinfo.speedOnAxisIdx)exinfo.speedOnAxisIdx=latestExtInfo.speedOnAxisIdx;

  //       // __UPRT_I_("vec_coord:%f %f %f  exinfo:f:%f a:%f d:%f aidx:%d\n",vec_coord.vec[0],vec_coord.vec[1],vec_coord.vec[2],
  //       // exinfo.speed,exinfo.acc,exinfo.deacc,exinfo.speedOnAxisIdx);
        

  //       CMDVec moveVec;
  //       vecSub(moveVec.vec,vec_coord.vec,latestCMDLocation.vec,CMD_VEC_DIM);
        
  //       // __UPRT_I_("vec:%s",vec_to_string(moveVec.vec).c_str());
  //       pushInMoveVec(moveVec.vec,&exinfo,CMD_VEC_DIM,NULL,NULL,NULL);
  //       latestCMDLocation=vec_coord;

  //       latestExtInfo=exinfo;
  //               // ReadGVecData(blks,blkCount,vec_f,&exinfo);

  //       // ConvUnitVecToPulseVec(&vec_f,&exinfo);
        
  //       // xVec newLoc=stpG.pulse_latestLoc;
  //       // SetxVec_fToxVec(newLoc,vec_f);
  //       // // xVec goVec=vecSub(newLoc,stpG.pulse_latestLoc);
        
  //       // stpG.MoveTo(newLoc,NULL,&exinfo);

  //       return  MCMD_Status::TASK_OK;
  //     }

  //     else if(CheckHead(cblk, "G4 ") || CheckHead(cblk, "G04 "))//pause
  //     {

  //       float P=NAN;
  //       FindFloat("P",blkIdxes,blkIdxesL,P);
  //       if(P==P && P>=0)
  //       {
  //         pushInPause(P,NULL,NULL,NULL);
  //         return  MCMD_Status::TASK_OK;
  //       }
  //       else
  //       {
  //         return MCMD_Status::TASK_FAILED;
  //       }
  //     }
  //   }
  //   else if(cblk[0]=='M')
  //   {
  //     if(homingFlag==false)
  //     {
  //       cmd_ret["msg"]="homingFlag==false";
  //       return MCMD_Status::TASK_FATAL_FAILED;
  //     }
  //     if(CheckHead(cblk, "M42 "))//IO ctrl
  //     { 
  //       float P=NAN;
  //       FindFloat("P",blkIdxes,blkIdxesL,P);
  //       float S=NAN;
  //       FindFloat("S",blkIdxes,blkIdxesL,S);
  //       if(P!=P || S!=S)
  //       {
  //         return MCMD_Status::TASK_FAILED;
  //       }


  //       MSTP_SegCtx *p_res;
  //       while((p_res=sctx_pool.applyResource())==NULL)
  //       {
  //         yield();
  //       }
  //       p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;

  //       p_res->IO_CTRL.P=P;
  //       p_res->IO_CTRL.S=S;
  //       pushInInstant(NULL,IOCtrlEndCB,(void*)p_res);
  //       return  MCMD_Status::TASK_OK;
  //     }


  //     else if(CheckHead(cblk, "M42.MODE "))//IO ctrl
  //     { 
  //       float P=NAN;
  //       FindFloat("P",blkIdxes,blkIdxesL,P);
  //       float M=NAN;
  //       FindFloat("M",blkIdxes,blkIdxesL,M);
  //       if(P!=P || M!=M)
  //       {
  //         return MCMD_Status::TASK_FAILED;
  //       }
  //       pinMode(P,M);
  //       return  MCMD_Status::TASK_OK;
  //     }

  //     else if(CheckHead(cblk, "M400.BLOCKING"))
  //     { 
        
  //       while(segs.size())
  //       {
  //         yield();
  //       }

  //       // while( trackingState!=TrackingState::InSync && trackingState!=TrackingState::NOP) 
  //       // {
  //       //   yield();
  //       // }

  //       // cmd_ret["R"]=curMotLoc_unit.vec[0];
  //       // cmd_ret["Z"]=curMotLoc_unit.vec[1];
  //       // cmd_ret["X"]=curMotLoc_unit.vec[2];
        
  //       // cmd_ret["Rp"]=preMotloc_pls.vec[0];
  //       // cmd_ret["Zp"]=preMotloc_pls.vec[1];
  //       // cmd_ret["Xp"]=preMotloc_pls.vec[2];

  //       return  MCMD_Status::TASK_OK;
  //     }

  //     else if(CheckHead(cblk, "M400"))
  //     { 

  //       MSTP_SegCtx *p_res;
  //       int retryCount=0;
  //       while((p_res=sctx_pool.applyResource())==NULL)
  //       {
  //         if(retryCount++>100)
  //           return MCMD_Status::TASK_FAILED;
  //         yield();
  //       }
  //       p_res->type=MSTP_SegCtx_TYPE::ON_TIME_REPLY;

  //       p_res->ON_TIME_REP.id=HACK_cur_cmd_id;
  //       p_res->ON_TIME_REP.isAck=true;
  //       pushInInstant(NULL,OnTimeRepEndCB,(void*)p_res);
  //       return  MCMD_Status::TASK_OK_HOLD_RSP;
  //     }

  //     else if(CheckHead(cblk, "M119"))
  //     {
  //       cmd_ret["input"]=TMP_HACK_latest_input_pins;
        
  //       return  MCMD_Status::TASK_OK;
  //     }
  //   }
  //   return  MCMD_Status::TASK_UNSUPPORTED;
  // }



  float vars[20]={0};
  int JCMDParse(JsonDocument& cmd,JsonDocument& cmd_ret)
  {
    JsonObject cmdData=cmd["cmd"];
    if(cmdData.isNull())
    {
      return MCMD_Status::TASK_UNSUPPORTED;
    }

    if(!cmdData["type"].is<const char*>())
    {
      return MCMD_Status::TASK_UNSUPPORTED;
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


    // if(strcmp(type,"COMP")==0)
    // {

    //   if(!cmdData["expr"].is<const char*>())
    //   {
    //     return MCMD_Status::PARAM_PARSE_ERROR;
    //   }

    //   const char* expr = cmdData["expr"];


    //   do{
    //     //Get arduinoJson array "vars"
    //     JsonArray arr = cmdData["vars"];
    //     if(arr.isNull())break;

    //     int len = sizeof(vars)/sizeof(vars[0]);
    //     if(arr.size()<len)len=arr.size();

    //     for (size_t i = 0; i <len; i++) {
    //       //check is number
    //       if(arr[i].is<float>())
    //       {
    //         vars[i] = arr[i].as<float>(); // Convert each element to float and store it.
    //       }
    //       else 
    //       {
    //         vars[i]=NAN;
    //       }
    //     }
    //     //fill vars with cmdData["vars"] number array


    //   }while(0);



    //   te_variable te_vars[] = {
       
    //     {"g", (void*)get_var, TE_CLOSURE1, vars},
    //     {"s", (void*)set_var, TE_CLOSURE2, vars}};
    


    //   auto stime=millis();
    //   int err;
    //   te_expr *n = te_compile(expr, te_vars, sizeof(te_vars)/sizeof(te_vars[0]), &err);

    //   auto etime=millis();
    //   cmd_ret["compileTime"]=etime-stime;
    //   stime=etime;


    //   if (n) {




    //     float result = te_eval(n);
    //     for(int i=1;i<test_runCount;i++)
    //     {
    //       result = te_eval(n);
    //     }
    //     etime=millis();
    //     cmd_ret["exeTime"]=etime-stime;
    //     cmd_ret["result"]=result;

    //     //put vars back to cmd_ret
    //     JsonArray arr = cmd_ret.createNestedArray("vars");
    //     if(ret_vars_count>sizeof(vars)/sizeof(vars[0])) 
    //     {
    //       ret_vars_count=sizeof(vars)/sizeof(vars[0]);
    //     }
    //     for (size_t i = 0; i < ret_vars_count; i++) {
    //       arr.add(vars[i]);
    //     }


    //     te_free(n);

    //     return MCMD_Status::TASK_OK;
    //   } else {
    //     cmd_ret["test2_error"]=err;
    //   }
    //   return MCMD_Status::TASK_FAILED;

    // }

    
    // if(strcmp(type,"LC_intersect")==0)
    // {

    //   //call findIntersection

    //   int test_runCount=1;
    //   if(cmdData["runCount"].is<float>())
    //     test_runCount=cmdData["runCount"];
        
    //   float vx=cmdData["vx"].as<float>();
    //   float vy=cmdData["vy"].as<float>();
    //   float cx=cmdData["cx"].as<float>();
    //   float cy=cmdData["cy"].as<float>();
    //   float r=cmdData["r"].as<float>();

    //   auto stime=millis();
    //   float t1,t2;
    //   volatile int res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
    //   for(int i=1;i<test_runCount;i++)
    //   {
    //     res=findIntersection(vx,vy,cx,cy,r,&t1,&t2);
    //   }
    //   auto etime=millis();
    //   cmd_ret["exeTime"]=etime-stime;
    //   //put result to cmd_ret


    //   JsonArray arr = cmd_ret.createNestedArray("pt_params");
    //   if(res>0)
    //   {
    //     arr.add(t1);
    //     if(res>1)
    //       arr.add(t2);
    //   }
    //   return MCMD_Status::TASK_OK;

    // }

    // if(strcmp(type,"FORWARD")==0)//motor location to head location
    // {
    //   float R=cmdData["R"].as<float>();
    //   float Z=cmdData["Z"].as<float>();
    //   float X=cmdData["X"].as<float>();

    //   float mot_vec[3]={R,Z,X};
    //   float loc[3]={0};

    //   forward(loc,mot_vec);

    //   cmd_ret["X"]=loc[0];
    //   cmd_ret["Y"]=loc[1];
    //   cmd_ret["Z"]=loc[2];
    // }

    // if(strcmp(type,"INVERSE")==0)
    // {
    //   float X=cmdData["X"].as<float>();
    //   float Y=cmdData["Y"].as<float>();
    //   float Z=cmdData["Z"].as<float>();

    //   float loc[3]={X,Y,Z};
    //   float mot_vec[3]={0};

    //   inverse(mot_vec,loc);

    //   cmd_ret["R"]=mot_vec[0];
    //   cmd_ret["Z"]=mot_vec[1];
    //   cmd_ret["X"]=mot_vec[2];
    // }


    if(homingFlag==false)
    {
      cmd_ret["msg"]="homingFlag==false";
      return MCMD_Status::TASK_FATAL_FAILED;
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



    if(strcmp(type,"TargetTrack")==0)
    {


      int trackIdx=-1;
      if(cmdData["trackIdx"].is<float>())
        trackIdx=cmdData["trackIdx"];
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
          return MCMD_Status::PARAM_PARSE_ERROR;
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
          return MCMD_Status::PARAM_PARSE_ERROR;
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

      return  MCMD_Status::TASK_OK;





  
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
        return MCMD_Status::TASK_FAILED;
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
          return MCMD_Status::PARAM_PARSE_ERROR;



        

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
          return MCMD_Status::TASK_FAILED;
        yield();
      }
      p_res->type=MSTP_SegCtx_TYPE::TRACKING_PROGRESS;
      p_res->TRACKING_INFO.p_ctrl_progress=&trackingInfo.progress;

      pushInInstant(NULL,instTrackingEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK;

    }



    if(strcmp(type,"ASSERT_TargetTrack_sattled")==0)
    {



      MSTP_SegCtx *p_res;

      while((p_res=sctx_pool.applyResource())==NULL) yield();

      p_res->type=MSTP_SegCtx_TYPE::GENERIC;
      p_res->Generic.obj=(void*)this;

      
      pushInInstant(NULL,AssertTrackingSattledEndCB,(void*)p_res);

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
      pushInPause(-1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

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

      p_res->TRACKING_HOLD_UNTIL.id=cmdData["id"];

      //-1 means halt forever
      pushInPause(-1,NULL,TrackingHoldUntilEndCB,(void*)p_res);

      return  MCMD_Status::TASK_OK_HOLD_RSP;
    }
      
    return MCMD_Status::TASK_UNSUPPORTED;
  }
};

StpGroup_SIMP SG_SIMP;




int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode!=1 )return 0;


  doc.clear();
  retdoc.clear();
  DeserializationError error = deserializeJson(doc, (const char*)raw);
  bool rspAck=false;
  bool doRsp=false;

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
  else if(strcmp(type,"BYE")==0)
  {
    doRsp=rspAck=true;

  }      
  
  if(strcmp(type,"m0")==0 )//{"type":"m0","cmd":"get_centre"}
  {
    rspAck=SG_PLATE.CMDProcess(doc,retdoc)==MCMD_Status::TASK_OK;
    doRsp=true;
  }
  
  // else if(AUX_Task_Try_Read(doc,type,retdoc,doRsp,rspAck))
  // {
  // }


  if(doRsp)
  {
    retdoc["id"]=(int)doc["id"];
    retdoc["ack"]=rspAck;
    
    uint8_t buff[700];
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);

  }


  return 0;


}

//


#define MAX_DRIVES (2) // Define the maximum number of drives

EthercatMaster master;  // Create an EtherCAT Master Object
EthercatDevice_CiA402 motor[MAX_DRIVES];  // Create an array of EtherCAT CiA402 Objects for multiple drives


float speed=0;
int loopTimes=0;
int init_enc_offsets[MAX_DRIVES];
int inputL=0;
int M2PosAcc=0;
int M1PosAcc=0;

void myCallback()
{


    // Callback function executed cyclically for each drive
    /*
        Uses motor.driveGetState() to get CiA402 status.
        EtherCAT CiA402 State as below:
             CIA402_NOT_READY_TO_SWITCH_ON = 0
             CIA402_SWITCH_ON_DISABLED = 1
             CIA402_READY_TO_SWITCH_ON = 2
             CIA402_SWITCHED_ON = 3
             CIA402_OPERATION_ENABLED = 4
             CIA402_FAULT = 5
             CIA402_FAULT_REACTION_ACTIVE = 6
             CIA402_QUICK_STOP_ACTIVE = 7
    */
    
    // Uses motor.driveSetTargetPosition(int32_t value) to set target position
    // Uses motor.driveGetPositionActualValue() to get current position value
    for (int i = 0; i < MAX_DRIVES; i++) {
        // If drive state is SWITCHED_ON, set target position to the current position
        if (motor[i].driveGetState() == CIA402_SWITCHED_ON) {
          motor[i].driveSetPositionOffset(-motor[i].driveGetPositionActualValue());
          init_enc_offsets[i]=motor[i].driveGetAdditionalActualPosition();
          // motor[i].driveSetTargetPosition(motor[i].driveGetPositionActualValue()); // Maintain current position

          M2PosAcc=0;
          // if(i==1)
          {
            motor[i].driveSetMaxMotorSpeed(1000000 );
          }
          // motor[i].driveSetPositionOffset(motor[i].driveGetAdditionalActualPosition()-motor[i].driveGetPositionActualValue());
        }
        // If drive state is OPERATION_ENABLED, increment the target position by 10
        if (motor[i].driveGetState() == CIA402_OPERATION_ENABLED) {
          SG_PLATE.update();
          
          const std::vector<int32_t> &plateSteps=SG_PLATE.getMotMoveVec();
          // int32_t loc=motor[i].driveGetTargetPosition();
          // if(loc>100000)
          // {
          //   motor[i].driveSetPositionOffset(-100000);
          //   // loc-=100000;
          // }
          // motor[i].driveSetTargetPosition((int32_t)(loc + speed*updatePeriod_s)); // Increment position
          if(i==1)
          {
            // inputL=plateSteps[0];
            M2PosAcc=motor[i].driveGetTargetPosition();
            M2PosAcc+=plateSteps[0];
            int newM2PosAcc=M2PosAcc;
            // if(M2PosAcc>1000)
            // {
            //   motor[i].driveSetHomeOffset(-newM2PosAcc);
            //   newM2PosAcc=0;
            //   // loc-=100000;
            // }
            // else if(M2PosAcc<-1000)
            // {
            //   motor[i].driveSetHomeOffset(newM2PosAcc);
            //   newM2PosAcc=0;
            // }

            motor[i].driveSetTargetPosition(M2PosAcc); 
            inputL=M2PosAcc;
            M2PosAcc=newM2PosAcc;

            // motor[i].driveSetTargetPosition((int32_t)(init_offsets[i] + 1000*sin(loopTimes/1000.0*2*M_PI*1))); 
          }
          else 
          {
            // int32_t cpos=motor[i].driveGetTargetPosition();
            // motor[i].driveSetTargetPosition((int32_t)(cpos + speed*updatePeriod_s));
          }
          // inputL=motor[i].touchProbeReadPositiveValue(0)<<i;
        }
    }

    if(loopTimes<10000)
    {
      speed+=10;
    }
    else
    {
      if(speed>=0)speed-=10;
      if(speed<=0)
      {
        // loopTimes=0;
        speed=0;
      }
    }
    loopTimes++;
}

void setup()
{
//	EVA.begin();
  Serial.begin(115200); // Initialize Serial communication
  while (!Serial); // Wait for the serial port to connect
  // delay(5000);

}

bool startECat()
{

  if(isECatStarted==true)return false;
  // Initialize the EtherCAT Master. All slaves enter PRE-OPERATIONAL state if successful
  djrl.dbg_printf("master.begin:%d",master.begin());

  //Configure each motor in the array
   for (int i = 0; i < MAX_DRIVES; i++)
   {
       motor[i].attach(0,i, master); // Attach each CiA402 device (motor) to the EtherCAT Master
       motor[i].setDc(updatePeriod_s*1000000000); // Set Distributed Clock (DC) parameters for the motor
       /*
           Uses motor.driveSetMode(CIA402_CSP_MODE) to set drive operation mode.
           EtherCAT CiA402 Mode as below:
              CIA402_PP_MODE      = 1
              CIA402_PV_MODE      = 3
              CIA402_PT_MODE      = 4
              CIA402_HOMING_MODE  = 6
              CIA402_CSP_MODE     = 8
              CIA402_CSV_MODE     = 9
              CIA402_CST_MODE     = 10
       */
       motor[i].driveSetMode(CIA402_CSP_MODE); // Set each motor to Cyclic Synchronous Position (CSP) mode
   }

   djrl.dbg_printf("attachCyclicCallback");
   master.attachCyclicCallback(myCallback); // Register a cyclic callback function

   int ret=master.start(updatePeriod_s*1000000000, ECAT_SYNC); // Start the EtherCAT Master. All slaves enter OPERATIONAL state if successful
   djrl.dbg_printf("master.start:%d",ret);
   
   djrl.dbg_printf("driveEnable");
   for (int i = 0; i < MAX_DRIVES; i++) motor[i].driveEnable(); // Enable each motor in the array

   isECatStarted=true;
   return true;
}


bool stopECat()
{
  if(isECatStarted==false)return false;
  for (int i = 0; i < MAX_DRIVES; i++) motor[i].driveDisable(); // Enable each motor in the array

  master.stop();
   
  master.detachCyclicCallback();
  for (int i = 0; i < MAX_DRIVES; i++)
  {
    motor[i].detach(); 
  }
  isECatStarted=false;
  return true;
}

static uint8_t recvBuf[20];

void loop()
{
  static unsigned long taskRunTime=0;
  static int32_t preInputL=0;

  if (millis()>taskRunTime){ // Switch resistor off
    djrl.dbg_printf("inputL:%d",inputL-preInputL);
    preInputL=inputL;
    taskRunTime+=1000; // Reset timer
  }


  djrl.loop();

  {
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.readBytes(recvBuf,sizeof(recvBuf-1));
      //
      djrl.dbg_printf("recvLen:%d",recvLen);
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


  if(0){



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
      if(hasNewInfo ==false && 0!=(AUX2CommInfoQ.size()))
      {
        info=*AUX2CommInfoQ.getTail();
        AUX2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }




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
