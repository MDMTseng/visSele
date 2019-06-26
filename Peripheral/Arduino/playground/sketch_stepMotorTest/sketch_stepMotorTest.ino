//Written By Nikodem Bartnik - nikodembartnik.pl
#define STEPPER_PIN_1 8
#define STEPPER_PIN_2 9
#define STEPPER_PIN_3 10
#define STEPPER_PIN_4 11



#define CAMERA_PIN 12

int step_number = 0;


void timer1_HZ(int HZ)
{
  
  TCNT1  = 0;
  OCR1A = 16000000/256/HZ;            // compare match register 16MHz/256/2Hz
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

uint32_t pulseHZ=0;
uint32_t tar_pulseHZ=1000;
uint32_t pulseHZ_step=200;

uint32_t countX=0;
ISR(TIMER1_COMPA_vect)          // timer compare interrupt service routine
{
  OneStepX(false);
  countX++;

  uint32_t rot=(1<<10)-1;
  if((countX&(rot))==(rot))
  {
    tar_pulseHZ=30;
  }
  
  if(((countX-130)&(rot))==(rot))
  {
    
    digitalWrite(CAMERA_PIN,1);
    delay(1);
    digitalWrite(CAMERA_PIN,0);
  }
  
  
  if(((countX-150)&(rot))==(rot))
  {
    tar_pulseHZ=1000;
  }
  
  
}

void setup() {
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);
  pinMode(CAMERA_PIN, OUTPUT);
  timer1Setup(1);
}

int count=50;

void loop() {
  if(pulseHZ!=tar_pulseHZ)
  {
    if(pulseHZ<tar_pulseHZ)
    {
      if(tar_pulseHZ-pulseHZ>pulseHZ_step)
        pulseHZ+=pulseHZ_step;
      else
        pulseHZ=tar_pulseHZ;
        
    }
    if(pulseHZ>tar_pulseHZ)
    {
      if(pulseHZ-tar_pulseHZ>pulseHZ_step)
        pulseHZ-=pulseHZ_step;
      else
        pulseHZ=tar_pulseHZ;
        
    }
    timer1_HZ(pulseHZ);
  }
  delay(50);
  
  if(0)
  {
    count--;
    if(count==0)
    {
      count=50;
      if(tar_pulseHZ!=1000)
      {
        tar_pulseHZ=1000;
      }
      else
      {
        tar_pulseHZ=1;
      }
    }
  }
}



boolean stepM_seq_a[]={1,1,0,0,0,0,0,1};
boolean stepM_seq_b[]={0,1,1,1,0,0,0,0};
boolean stepM_seq_c[]={0,0,0,1,1,1,0,0};
boolean stepM_seq_d[]={0,0,0,0,0,1,1,1};

/*

boolean stepM_seq_a[]={1,0,0,1,0,1,0,0};
boolean stepM_seq_b[]={0,1,0,1,0,0,1,0};
boolean stepM_seq_c[]={0,1,0,0,1,0,0,1};
boolean stepM_seq_d[]={0,0,1,0,0,1,0,1};
*/

int stepX_number = 0;
void OneStepX(bool dir){

  
  digitalWrite(STEPPER_PIN_1,stepM_seq_a[stepX_number]);
  digitalWrite(STEPPER_PIN_2,stepM_seq_b[stepX_number]);
  digitalWrite(STEPPER_PIN_3,stepM_seq_c[stepX_number]);
  digitalWrite(STEPPER_PIN_4,stepM_seq_d[stepX_number]);
  

  if(dir)
  {
    stepX_number++;
    if(stepX_number ==sizeof(stepM_seq_a)){
      stepX_number = 0;
    }
  }
  else
  {
    
    if(stepX_number == 0){
      stepX_number = sizeof(stepM_seq_a)-1;
    }
    else
      stepX_number--;
  }
}
