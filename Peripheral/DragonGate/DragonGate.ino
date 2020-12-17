
#include "src/FastAccelStepper/FastAccelStepper.h"
//#include "src/ESP32Encoder/ESP32Encoder.h"
//ESP32Encoder encoder;
//
//ESP32Encoder encoder2;
//
//void Init_Encoder()
//{
//  return;
//  ESP32Encoder::useInternalWeakPullResistors=UP;
//  encoder.attachFullQuad(32, 33);
//////    // Attache pins for use as encoder pins
//  encoder2.attachFullQuad(25, 26);
////  encoder.setCount(0);
////  encoder2.setCount(0);
//}
//
//
//
//void loopEnc(){
//  return;
//  const int32_t CountDown=50;
//  static int32_t sameCountDown=CountDown;
//  static int32_t preCSum=0;
//
//
//  int16_t c1,c2;
//  pcnt_get_counter_value(encoder.unit, &c1);
//  pcnt_get_counter_value(encoder2.unit, &c2);
//  int32_t curEnc1=c1;
//  int32_t curEnc2=c2;
//  
////  int32_t curEnc1=(int32_t)encoder.getCount();
////  int32_t curEnc2=(int32_t)encoder2.getCount();
//  int32_t curCSum=cmpCheckSum(7,2, curEnc1,curEnc2);
//  if(preCSum==curCSum)
//  {
//    if(sameCountDown==1)
//    {
//      Serial.println("enc1 = "+String(curEnc1/2)+"  enc2 = "+String(curEnc2/2));
//    }
//    
//    if(sameCountDown)
//    {
//      sameCountDown--;
//    }
//  }
//  else
//  {
//    sameCountDown=CountDown;
//    preCSum=curCSum;
//  }
////  delay(50);
//}
class MultiFAStepper
{
  public:
  FastAccelStepperEngine engine=FastAccelStepperEngine();
  
  FastAccelStepper *stepper[3];


  
  MultiFAStepper(){
    engine.init();
    int i=0;
    stepper[i] = engine.stepperConnectToPin(4);

    if (stepper[i]) {
      stepper[i]->setDirectionPin(5);
    }
    i++;
    stepper[i] = engine.stepperConnectToPin(18);

    if (stepper[i]) {
      stepper[i]->setDirectionPin(19);
    }
// 
//    i++;
//    stepper[i] = engine.stepperConnectToPin(22);
//
//    if (stepper[i]) {
//      stepper[i]->setDirectionPin(23);
//    }
 
  }
};



MultiFAStepper MFAStepper;


void setup() {
  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);
//  Init_Encoder();

  Serial.begin(115200);
  Serial.println("Demo RawAccess");
  Serial.println("Init_Encoder OK");

  
}

// This loop drives the stepper up to 80000 microsteps/s.
// For a NEMA-17 with 200 steps/revolution and 16 microsteps, this means 25
// revolutions/s


bool _direction = true;

uint16_t ticks = TICKS_PER_S / 100000*1;
const struct stepper_command_s cmd_fwd = {
    .ticks = ticks, .steps = 50, .count_up = _direction};

//const struct stepper_command_s cmd_bwd = {
//    .ticks = ticks/2, .steps = 10, .count_up = !_direction};

void loop()
{

  
//  struct stepper_command_s cmd = cmd_fwd;//Q flush
//  for(int i=0;i<20;i++)
//  {
//    MFAStepper.stepper[0]->addQueueEntry(&cmd);
////    MFAStepper.stepper[1]->addQueueEntry(&cmd);
////    MFAStepper.stepper[2]->addQueueEntry(&cmd);
//  }
//  
//  delay(300);
  
    for(int i=0;i<300;i++)
    {
      loopXX();
    }

}

void loopXX() {
//  static int togglePING=HIGH;
  uint8_t qsize=MFAStepper.stepper[0]->queueSize();

  
  if(qsize<8)
  {
    while(1)//filling queue
    {
      struct stepper_command_s cmd =cmd_fwd;//_direction? cmd_fwd:cmd_bwd;
      cmd.count_up=_direction;
      digitalWrite(2,HIGH);
      if(MFAStepper.stepper[0]->addQueueEntry(&cmd)==AQE_OK)//if its ok, then force the second Stepper to be OK as well
      {
//        cmd.ticks/=2;
//        cmd.steps*=2;
//        MFAStepper.stepper[0]->addQueueEntry(&cmd);
//        MFAStepper.stepper[2]->addQueueEntry(&cmd);
//        while(MFAStepper.stepper[1]->addQueueEntry(&cmd)!=AQE_OK);
//        while(MFAStepper.stepper[2]->addQueueEntry(&cmd)!=AQE_OK);
        _direction=!_direction;
        digitalWrite(2,LOW);

      }
      else
      {
        //qsize=MFAStepper.stepper[1]->queueSize();
        //Serial.println("sQ size= "+String(qsize));
        break;
      }
      
    }
  }
  else
  {
//    Serial.println("bQ size= "+String(qsize));
  }
//  loopEnc();
}










uint32_t _rotl(uint32_t value, int shift) {
    if ((shift &= 31) == 0)
      return value;
    return (value << shift) | (value >> (32 - shift));
}
uint32_t cmpCheckSum(int shift,int num_args, ...)
{
   uint32_t val = 0;
   va_list ap;
   int i;

   va_start(ap, num_args);
   for(i = 0; i < num_args; i++) 
   {
      val ^= _rotl(va_arg(ap, uint32_t),3);
      val = _rotl(val,shift);
   }
   va_end(ap);
 
   return val;
}
//
