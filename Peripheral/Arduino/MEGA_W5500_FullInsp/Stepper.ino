

boolean stepM_seq_a[] = {1, 1, 0, 0, 0, 0, 0, 1};
boolean stepM_seq_b[] = {0, 1, 1, 1, 0, 0, 0, 0};
boolean stepM_seq_c[] = {0, 0, 0, 1, 1, 1, 0, 0};
boolean stepM_seq_d[] = {0, 0, 0, 0, 0, 1, 1, 1};

class StepperMotor
{

    /*

      boolean stepM_seq_a[]={1,0,0,1,0,1,0,0};
      boolean stepM_seq_b[]={0,1,0,1,0,0,1,0};
      boolean stepM_seq_c[]={0,1,0,0,1,0,0,1};
      boolean stepM_seq_d[]={0,0,1,0,0,1,0,1};
    */
  public:
    int p1, p2, p3, p4;

    StepperMotor(int p1, int p2, int p3, int p4)
    {

      pinMode(p1, OUTPUT);
      pinMode(p2, OUTPUT);
      pinMode(p3, OUTPUT);
      pinMode(p4, OUTPUT);
      this->p1 = p1;
      this->p2 = p2;
      this->p3 = p3;
      this->p4 = p4;
    }

    int stepX_number = 0;
    void OneStep(bool dir) { //DRV8825 p1=step p2=dir
      digitalWrite(p2, dir);

      digitalWrite(p1, HIGH);
      digitalWrite(p1, LOW);

    }
    void OneStep_Original(bool dir) {


      digitalWrite(p1, stepM_seq_a[stepX_number]);
      digitalWrite(p2, stepM_seq_b[stepX_number]);
      digitalWrite(p3, stepM_seq_c[stepX_number]);
      digitalWrite(p4, stepM_seq_d[stepX_number]);
      if (dir)
      {
        stepX_number++;
        if (stepX_number == sizeof(stepM_seq_a)) {
          stepX_number = 0;
        }
      }
      else
      {

        if (stepX_number == 0) {
          stepX_number = sizeof(stepM_seq_a) - 1;
        }
        else
          stepX_number--;
      }
    }
};


StepperMotor stepperMotor(22, 23, 24, 25);
//StepperMotor stepperMotorDRV8825(22,23,24,25);


#define CAMERA_PIN 16
#define AIR_BLOW_OK_PIN 18
#define AIR_BLOW_NG_PIN 19
#define GATE_PIN 30


uint32_t perRevPulseCount_HW = 2400*32;
//uint8_t subPulseDiv=1;
uint8_t pulseSkip=32;
uint32_t perRevPulseCount = perRevPulseCount_HW/pulseSkip;

int step_number = 0;

void timer1_HZ(int HZ)
{

  TCNT1  = 0;
  OCR1A = 16000000 / 256 / HZ;        // compare match register 16MHz/256/2Hz
}

void timer1Setup(int HZ)
{


  // initialize timer1

  noInterrupts();           // disable all interrupts

  TCCR1A = 0;

  TCCR1B = 0;

  timer1_HZ(HZ);
  TCCR1B |= (1 << WGM12);   // CTC mode

  TCCR1B |= (1 << CS12);    // 256 prescaler

  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt

  interrupts();             // enable all interrupts
}


int offsetAir=80;

uint32_t PRPC= perRevPulseCount;
uint32_t state_pulseOffset[] = {
  PRPC*30/360, PRPC*30/360+5, 
  PRPC*30/360+10, 
  
  PRPC*240/360+offsetAir, PRPC*240/360+6+offsetAir, 
  PRPC*240/360+20+offsetAir, PRPC*240/360+26+offsetAir};


int stage_action(pipeLineInfo* pli);
int stage_action(pipeLineInfo* pli)
{
  pli->notifMark = 0;
  switch (pli->stage)
  {
    case 0:

      break;
    case 1://Trigger shutter ON
      digitalWrite(CAMERA_PIN, 1);
      pli->notifMark = 1;
      break;
    case 2://Trigger shutter OFF
      digitalWrite(CAMERA_PIN, 0);
      break;

    case 3://Termination stage
      //pli->stage = 5;
      //return -1;
      break;


    case 4://Air Blow OK ON
      digitalWrite(AIR_BLOW_OK_PIN, 1);
      break;
    case 5://Air Blow OK OFF
      digitalWrite(AIR_BLOW_OK_PIN, 0);
      //pli->stage = -1;
      //return -1;
      break;
    case 6://Air Blow NG ON
      digitalWrite(AIR_BLOW_NG_PIN, 1);
      break;
    case 7://Air Blow NG OFF
      digitalWrite(AIR_BLOW_NG_PIN, 0);
      pli->stage = -1;
      return -1;
      break;
  }
  return 0;
}

int next_state(pipeLineInfo* pli);
int next_state(pipeLineInfo* pli)
{
  if (pli->stage < 0 || pli->stage > sizeof(state_pulseOffset) / sizeof(*state_pulseOffset))return -1;
  if (stage_action(pli) < 0)
  {
    return -1;
  }
  pli->trigger_pulse = pli->gate_pulse + state_pulseOffset[pli->stage];
  if (pli->trigger_pulse >= perRevPulseCount)
  {
    pli->trigger_pulse -= perRevPulseCount;
  }
  pli->stage++;
  return 0;
}

uint32_t pulseHZ = 0;
uint32_t tar_pulseHZ = 32 * 800;
uint32_t pulseHZ_step = 1;

uint32_t countX = 0;
uint32_t getTimerCount() {
  return countX;
}

typedef struct GateInfo {
  uint8_t state;
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint8_t debunce;
  uint8_t pre_Sense;
  uint8_t cur_Sense;
  uint8_t Sense;
} GateInfo;
GateInfo gateInfo = {.state = 1};


uint8_t countSkip = 0;
ISR(TIMER1_COMPA_vect)          // timer compare interrupt service routine
{
  stepperMotor.OneStep(true);

  countSkip++;
  if (countSkip < pulseSkip)return;
  countSkip = 0;

  countX++;
  uint32_t countSize = perRevPulseCount;
  countX %= perRevPulseCount;
  if(countX==0)
  {
    
      Serial.print("RBuf.size(): ");
      Serial.print(RBuf.size());
  }
  
  {
    for (uint32_t i = 0; i < RBuf.size(); i++) //Check if trigger pulse is hit, then do action/ mark deletion
    {
      pipeLineInfo* tail = RBuf.getTail(i);
      if (!tail)break;
      if (tail->trigger_pulse == countX)
      {


        Serial.print(RBuf.getTail_Idx(i));
        Serial.print(": ");
        Serial.println(tail->stage);
        int ret = next_state(tail);

        if (ret)
        {
          tail->stage = -3;
        }
      }
    }

    while (1) //Clean completed tail tasks
    {
      pipeLineInfo* tail = RBuf.getTail();
      if (!tail)break;

      if (tail->stage < 0)
      {
        RBuf.consumeTail();
        continue;
      }
      break;
    }
  }


  if (countX == 0)
  {
    //tar_pulseHZ=30;
  }
  uint8_t cur_Sense = digitalRead(GATE_PIN);
  if (cur_Sense != gateInfo.pre_Sense)
  {
    gateInfo.debunce = 0;
  }
  else if (gateInfo.debunce <= 3)gateInfo.debunce++;

  if (gateInfo.debunce == 3)
  {
    if (cur_Sense == 0)
    {
      if (gateInfo.state != cur_Sense)
      {
        gateInfo.state = cur_Sense;
        gateInfo.start_pulse = countX;
      }
    }
    else
    {
      if (gateInfo.state != cur_Sense)
      {

        gateInfo.end_pulse = countX;
        //tar_pulseHZ=1000;
        if (gateInfo.start_pulse > gateInfo.end_pulse)
        {
          gateInfo.end_pulse += perRevPulseCount;
        }
        uint32_t middle_pulse = (gateInfo.end_pulse + gateInfo.start_pulse) >> 1;
        middle_pulse %= perRevPulseCount;
        pipeLineInfo* head = RBuf.getHead();
        if (head != NULL)
        {
          head->gate_pulse = middle_pulse;
          head->stage = 0;
          next_state(head);
          RBuf.pushHead();
//                    Serial.print("====g_pulse:");
//                    Serial.print(head->gate_pulse);
//                    Serial.print(" t_pulse:");
//                    Serial.print(head->trigger_pulse);
//                    Serial.print(" CX:");
//                    Serial.println(countX);

        }
      }

    }
    gateInfo.state = cur_Sense;
  }

  gateInfo.pre_Sense = gateInfo.cur_Sense;
  gateInfo.cur_Sense = cur_Sense;

  if (countX == 130)
  {
  }


}

void setup_Stepper() {
  pinMode(CAMERA_PIN, OUTPUT);
  pinMode(AIR_BLOW_OK_PIN, OUTPUT);
  pinMode(AIR_BLOW_NG_PIN, OUTPUT);
  pinMode(GATE_PIN, INPUT);
  Serial.println(".....");
  timer1Setup(1);
}

int count = 50;

void loop_Stepper() {
  if (pulseHZ != tar_pulseHZ)
  {
    if (pulseHZ < tar_pulseHZ)
    {
      if (tar_pulseHZ - pulseHZ > pulseHZ_step)
        pulseHZ += pulseHZ_step;
      else
        pulseHZ = tar_pulseHZ;

    }
    if (pulseHZ > tar_pulseHZ)
    {
      if (pulseHZ - tar_pulseHZ > pulseHZ_step)
        pulseHZ -= pulseHZ_step;
      else
        pulseHZ = tar_pulseHZ;

    }
    timer1_HZ(pulseHZ);
  }

  if (0)
  {
    count--;
    if (count == 0)
    {
      count = 150;
      /*
        if(tar_pulseHZ!=1000)
        {
        tar_pulseHZ=1000;
        }
        else
        {
        tar_pulseHZ=1;
        }*/


      Serial.print(RBuf.size());
      Serial.print("   ");
      Serial.println(gateInfo.start_pulse);
      RBuf.consumeTail();
    }
  }
}
