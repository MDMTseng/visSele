// #include <Test.h>
#include <MSteppersV2.h>
#include <string>
#define ARDUINOJSON_ENABLE_STD_STRING 0
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#include "ArduinoJson.h"
#include <Data_Layer_Protocol.h>
#include "SCoop.h"

/* QEC CiA402 CSP Mode Example */
#include "Ethercat.h" // Include the EtherCAT Library
// void setup()
// {

// }

  
// void loop()
// {

// }


// 
// #include "ArduinoJson.h"
// // #include "Data_Layer/Data_Layer_IF.hpp"

// #include "Data_Layer_Protocol.hpp"
// #include "GCodeParserUtil.hpp"



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
    if(strcmp(type,"RESET")==0)
    {
      
      return msg_printf("RESET_OK","");
    } 
    else if(strcmp(type,"BYE")==0)
    {
      doRsp=rspAck=true;

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
      cur_speed+=acc*updatePeriod_s;
      if(cur_speed>tar_speed)cur_speed=tar_speed;
    }
    if(cur_speed>tar_speed){
      cur_speed-=acc*updatePeriod_s;
      if(cur_speed<tar_speed)cur_speed=tar_speed;
    }
    latestAdvLocation.vec[0]+=cur_speed*updatePeriod_s;//in rad
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

  int CMDProcess(JsonDocument& cmd,JsonDocument& cmd_ret)
  {

    JsonObject cmdData=cmd["cmd"];
    if(cmdData.isNull())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }

    if(!cmdData["type"].is<const char*>())
    {
      return MCMD_Status::PARAM_PARSE_ERROR;
    }
    const char* type = cmdData["type"];


    if(strcmp(type,"cur_angle")==0)
    {
      cmd_ret["angle"]=latestAdvLocation.vec[0];
      return MCMD_Status::TASK_OK;
    }

    if(strcmp(type,"set_speed")==0)
    {
      if(!cmdData["speed"].is<float>())
      {
        return MCMD_Status::PARAM_PARSE_ERROR;
      }
      tar_speed=cmdData["speed"];
      return MCMD_Status::TASK_OK;
    }


    if(strcmp(type,"set_centre")==0)
    {
      if(!cmdData["x"].is<float>()||!cmdData["y"].is<float>())
      {
        return MCMD_Status::PARAM_PARSE_ERROR;
      }
      cx=cmdData["x"];
      cy=cmdData["y"];
      return MCMD_Status::TASK_OK;
    }



    if(strcmp(type,"get_centre")==0)
    {
      cmd_ret["cx"]=cx;
      cmd_ret["cy"]=cy;
      return MCMD_Status::TASK_OK;
    }

    cmd_ret["type"]=type;

    
    return MCMD_Status::TASK_UNSUPPORTED;


  }
};

StpGroup_PLATE SG_PLATE;





//


#define MAX_DRIVES (1) // Define the maximum number of drives

EthercatMaster master;  // Create an EtherCAT Master Object
EthercatDevice_CiA402 motor[MAX_DRIVES];  // Create an array of EtherCAT CiA402 Objects for multiple drives

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
            motor[i].driveSetTargetPosition(motor[i].driveGetPositionActualValue()); // Maintain current position
        }
        // If drive state is OPERATION_ENABLED, increment the target position by 10
        if (motor[i].driveGetState() == CIA402_OPERATION_ENABLED) {
            motor[i].driveSetTargetPosition(motor[i].driveGetTargetPosition() + 10); // Increment position
        }
    }
}

void setup()
{
    Serial.begin(115200); // Initialize Serial communication
    while (!Serial); // Wait for the serial port to connect

    // Initialize the EtherCAT Master. All slaves enter PRE-OPERATIONAL state if successful
    Serial.println(master.begin());

    // Configure each motor in the array
    for (int i = 0; i < MAX_DRIVES; i++)
    {
        motor[i].attach(i, master); // Attach each CiA402 device (motor) to the EtherCAT Master
        motor[i].setDc(1000000); // Set Distributed Clock (DC) parameters for the motor
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

    master.attachCyclicCallback(myCallback); // Register a cyclic callback function

    master.start(1000000, ECAT_SYNC); // Start the EtherCAT Master. All slaves enter OPERATIONAL state if successful
    
    for (int i = 0; i < MAX_DRIVES; i++) motor[i].driveEnable(); // Enable each motor in the array
}

void loop()
{
    
}
