
#include <Wire.h>



#define I2C_SDA 32
#define I2C_SCL 33

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.begin(115200);
  Serial.println("\nI2C Scanner");
}

uint8_t I2C_Read(uint8_t addr, uint8_t reg)
{
  uint8_t chr = 0;
  Wire.beginTransmission(addr);  //device address
  Wire.write(reg);  // read register
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 1, true);    //address, quantity, stop
  if (Wire.available()) {
    chr = Wire.read();           //data
  }
  return  chr ;
}


uint16_t I2C_Read_2B(uint8_t addr, uint8_t reg)
{
  uint16_t chr = 0;
  Wire.beginTransmission(addr);  //device address
  Wire.write(reg);  // read register
  Wire.endTransmission(false);
  Wire.requestFrom(addr, 2, true);    //address, quantity, stop
  if (Wire.available()) {
    chr = Wire.read();           //data
    chr<<=8;
    chr|=Wire.read();
  }
  return  chr ;
}

int32_t angleSample(int32_t totalAngle)
{
  //  int Angle = (I2C_Read((0x36),0x0e)<<8)|(I2C_Read((0x36),0x0f));
  uint16_t Angle =I2C_Read_2B((0x36),0x0e);  

  int angleDiff=Angle-(((uint16_t)totalAngle)&0xFFF);
  int angleStep;
  if(angleDiff>0)
  {
    if(angleDiff>2048)
    {
      angleStep=angleDiff-4096;
    }
    else
    {
      angleStep=angleDiff;
    }
  }
  else
  {
    if(angleDiff<-2048)
    {
      angleStep=4096+angleDiff;
    }
    else
    {
      angleStep=angleDiff;
    }
  }
  
  totalAngle+=angleStep;
  return totalAngle;
}




void loop() {
  
  static int32_t totalAngle=0;
  totalAngle=angleSample(totalAngle);
  
  delay(10);

  static int printCounter=0;
  if((printCounter++)>10)
  {
    printCounter=0;
    Serial.print(" tA:");
    Serial.println(totalAngle/1024.0);
  }
}
