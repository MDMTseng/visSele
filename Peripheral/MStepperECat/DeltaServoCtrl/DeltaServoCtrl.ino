#include "Ethercat.h"
#include <stdarg.h>
EthercatMaster master;
EthercatDevice_CiA402 ASDA_B3;

int latest_status=-999;
void myCallback() {
 // Check the state of the first drive (ASDA_A2) and update its position

 // Check the state of the second drive (ASDA_B3) and update its position
 int stateB3 = ASDA_B3.driveGetState();
 latest_status=stateB3;
 if (stateB3 == CIA402_SWITCHED_ON) {
 ASDA_B3.driveSetTargetPosition(ASDA_B3.driveGetPositionActualValue());
 } else if (stateB3 == CIA402_OPERATION_ENABLED) {
 ASDA_B3.driveSetTargetPosition(ASDA_B3.driveGetTargetPosition());
 }
}

char dbgBuff[1000];
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

void setup() {

  
  Serial.begin(115200); // Initialize Serial communication
  Serial.setTimeout(0,0);
  while (!Serial); // Wait for the serial port to connect
  // delay(5000);
 // Initialize the master and drives
 master.begin();
// Setup second drive (ASDA_B3)
 ASDA_B3.attach(0, master);
 ASDA_B3.setDc(1000000); // Set cycle time
 ASDA_B3.driveSetMode(CIA402_CSP_MODE); // Set control mode
 // Attach the callback function to the master and start the communication
 master.attachCyclicCallback(myCallback);
 master.start(1000000, ECAT_SYNC);
 // Enable the drives to make them ready for motion commands

 ASDA_B3.driveEnable();
}
void loop() {

  delay(1000);
  
    dbg_DIRECT_PRINT("ASDA_B3:ctrl:%X st:%X,%X mod:%X er:%X\n",
      ASDA_B3.driveGetControlword(),
      ASDA_B3.driveGetStatusword(),
      latest_status,
      ASDA_B3.driveGetMode(),
      ASDA_B3.driveGetErrorCode()
      // testServo.motdrv.driveGet
      
      );

  
  
    dbg_DIRECT_PRINT("      :opmode:%d  0x6040:%d\n",
      ASDA_B3.driveGetOperationMode(),
      ASDA_B3.sdoUpload8(0x6041,0)

      );
 // Main loop does not need to do anything in this example
}
