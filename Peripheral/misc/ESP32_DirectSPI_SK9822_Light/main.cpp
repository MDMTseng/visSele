//
//    FILE: MCP23017_digitalWrite.ino
//  AUTHOR: Rob Tillaart
//    DATE: 2019-10-14
// PUPROSE: test MCP23017 library


// #include "MCP23017.h"
#include "Wire.h"



#include "Arduino.h"
#include <string.h>
#include "esp_system.h"
#include "driver/timer.h"

extern "C" {
#include "direct_spi.h"
}


void print_binary(uint32_t var,int digit=32) {
  uint32_t test =((uint32_t)1)<<(digit-1);

  for (; test; test >>= 1) {
    Serial.write(var  & test ? '1' : '0');
  }
}

int pin_SH_165=17;
int pin_Camera=32;
int pin_LED=2;

int counter=0;

uint32_t latestInput=0;
static void display_test(spi_device_handle_t spi) 
{

	while (direct_spi_in_use(spi));
	gpio_set_level((gpio_num_t) pin_SH_165, 0);//switch to load
	gpio_set_level((gpio_num_t) PIN_NUM_CS, 1);//trigger pin change


  //prepare
  int count=1+15;
  // spi->host->hw->data_buf[0] =0x55;
  uint32_t input = spi->host->hw->data_buf[0];
  latestInput=input;
  gpio_set_level((gpio_num_t)pin_LED, input&(1<<(5+8*3)));

  // for(int i=0;i<count;i++)
  // {
  //   //  spi->host->hw->data_buf[i]=input;
  //   spi->host->hw->data_buf[i]=counter<<0;
  //   spi->host->hw->data_buf[i]|=counter<<8;
  //   spi->host->hw->data_buf[i]|=counter<<16;
  //   spi->host->hw->data_buf[i]|=counter<<24;
  // }
  spi->host->hw->data_buf[0]=0;
  int step=counter&0x3FFF;
  int stepSep=400;
  int stepSep2=800;
  
  int bri=0;//=(step<=2)?0x1F:0;
  if(step<stepSep)
  {
    switch(step )
    {
      case 0:
        bri=0x1F;
      break;
      case 1:
        
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 1);//switch to load
      break;

      
      case 2:
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 0);//switch to load
      break;


    }
      
    for(int i=1;i<count;i++)
    {
      // if(i==count-1)bri=0;
      //  spi->host->hw->data_buf[i]=input;
      spi->host->hw->data_buf[i] =(111<<5)+((i<count/2)?bri:0);
      // spi->host->hw->data_buf[i]|=0xFF<<8;
      // spi->host->hw->data_buf[i]|=0xFF<<16;
      spi->host->hw->data_buf[i]|=0xFF<<24;
    }


  }
  else if(step<stepSep2)
  {

    switch(step-stepSep)
    {
      case 0:
        bri=0x1F;
      break;
      case 1:
        
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 1);//switch to load
      break;

      
      case 2:
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 0);//switch to load
      break;


    }
    for(int i=1;i<count;i++)
    {
      // if(i==count-1)bri=0;
      //  spi->host->hw->data_buf[i]=input;
      spi->host->hw->data_buf[i] =(111<<5)+((i<count/2)?0:bri);
      spi->host->hw->data_buf[i]|=0xFF<<8;
      // spi->host->hw->data_buf[i]|=0xFF<<16;
      // spi->host->hw->data_buf[i]|=0xFF<<24;
    }


  }

  else
  {

    switch(step-stepSep2)
    {
      case 0:
        bri=0x1F;
      break;
      case 1:
        
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 1);//switch to load
      break;

      
      case 2:
        bri=0x1F;
        gpio_set_level((gpio_num_t) pin_Camera, 0);//switch to load
      break;


    }
    for(int i=1;i<count;i++)
    {
      // if(i==count-1)bri=0;
      //  spi->host->hw->data_buf[i]=input;
      spi->host->hw->data_buf[i] =(111<<5)+bri;
      spi->host->hw->data_buf[i]|=0xFF<<8;
      spi->host->hw->data_buf[i]|=0xFF<<16;
      spi->host->hw->data_buf[i]|=0xFF<<24;
    }


  }

  // spi->host->hw->data_buf[count-1]=0xFFFFFFFF;

	gpio_set_level((gpio_num_t) pin_SH_165, 1);//switch to keep to 165 register
	gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 1);
	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
  direct_spi_transfer(spi,32+(count-1)*32);
  // direct_spi_transfer(spi,32);

	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
  counter++;
}

hw_timer_t *timer = NULL;
spi_device_handle_t spi1;
void IRAM_ATTR onTimer()
{

  gpio_set_level((gpio_num_t)pin_LED, 1);
  gpio_set_level((gpio_num_t)pin_LED, 0);
  display_test(spi1);
  float freq=10000;
  int tick=20*1000*1000/freq;
  gpio_set_level((gpio_num_t)pin_LED, 1);
  // timerAlarmWrite(timer,tick, true);
  timer_set_alarm_value(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, (uint64_t)tick);
  gpio_set_level((gpio_num_t)pin_LED, 0);
}

void setup()
{
  Serial.begin(921600);
  spi1= direct_spi_init(1,40*1000*1000,PIN_NUM_MOSI,PIN_NUM_MISO,PIN_NUM_CLK,PIN_NUM_CS);

  gpio_set_direction((gpio_num_t)PIN_NUM_CS,GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)pin_SH_165,GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)pin_LED,GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)pin_Camera,GPIO_MODE_OUTPUT);

  
  
  dspi_device_select(spi1,1);
  // while(1)
  // {
  //  
  // }

  
  timer = timerBegin(0, 4, true);//20M
  
  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, 100000, true);
  timerAlarmEnable(timer);
}


void loop()
{
  delay(100);
  Serial.write("IN:");
  print_binary(latestInput,32);
  Serial.write("\n");
}


// -- END OF FILE --

