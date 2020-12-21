// MultiStepper.pde
// -*- mode: C++ -*-
// Use MultiStepper class to manage multiple steppers and make them all move to 
// the same position at the same time for linear 2d (or 3d) motion.


#include "src/ESP32Encoder/ESP32Encoder.h"
ESP32Encoder encoder;
ESP32Encoder encoder2;
void Init_Encoder()
{
  ESP32Encoder::useInternalWeakPullResistors=UP;
  encoder.attachFullQuad(32, 33);
////    // Attache pins for use as encoder pins
  encoder2.attachFullQuad(25, 26);
  encoder.setCount(0);
  encoder2.setCount(0);
}


void printEnc()
{
  
  int16_t c1,c2;
  pcnt_get_counter_value(encoder.unit, &c1);
  pcnt_get_counter_value(encoder2.unit, &c2);
  int32_t curEnc1=c1;
  int32_t curEnc2=c2;
  Serial.println("enc1 = "+String(curEnc1/2)+"  enc2 = "+String(curEnc2/2));
}

#include "src/AccelStepper/AccelStepper.h"
#include "src/AccelStepper/MultiStepper.h"

// EG X-Y position bed driven by 2 steppers
// Alas its not possible to build an array of these with different pins for each :-(
AccelStepper stepper1(AccelStepper::DRIVER, 4, 5);
AccelStepper stepper2(AccelStepper::DRIVER, 18,19);
AccelStepper stepper3(AccelStepper::DRIVER, 22,23);

// Up to 10 steppers can be handled as a group by MultiStepper
MultiStepper steppers;

void setup() {
  Init_Encoder();
  Serial.begin(115200);
  pinMode(2,OUTPUT);
  digitalWrite(2,LOW);
  // Configure each stepper
  stepper1.setMaxSpeed(10000);
  stepper1.setAcceleration(10000.0);
  stepper2.setMaxSpeed(40000);
  stepper3.setMaxSpeed(40000);

  // Then give them to MultiStepper to manage
  steppers.addStepper(stepper1);
  steppers.addStepper(stepper2);
  steppers.addStepper(stepper3);
}

void loop() {
  
  stepper1.runToNewPosition(0);
  printEnc();
//  stepper1.runToNewPosition(20000);
//  printEnc();
//  stepper1.runToNewPosition(40000);
//  printEnc();
  stepper1.runToNewPosition(40000);
  printEnc();
}
void loopX() {
  long positions[3]; // Array of desired stepper positions
  
  positions[0] = 0;
  positions[1] = 0;
  positions[2] = 0;
  steppers.moveTo(positions);
  
  positions[0] = 10000/2;
  positions[1] = 6000/2;
  positions[2] = 3000/2;
  while(steppers.run());


  
  steppers.moveTo(positions);
  
  positions[0] = 10000;
  positions[1] = 6000;
  positions[2] = 3000;
  while(steppers.run());
  
  printEnc();
  delay(20);
  // Move to a different coordinate
  steppers.moveTo(positions);
  while(steppers.run());
  delay(20);
  
}
