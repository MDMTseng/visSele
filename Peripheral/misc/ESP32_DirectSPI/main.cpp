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
int pin_LED=2;

int counter=0;


uint32_t latestInput=0;
static void display_test(spi_device_handle_t spi) 
{

	while (direct_spi_in_use(spi));
	gpio_set_level((gpio_num_t) pin_SH_165, 0);//switch to load
	gpio_set_level((gpio_num_t) PIN_NUM_CS, 1);//trigger pin change


  //prepare
  int count=1;
  // spi->host->hw->data_buf[0] =0x55;
  uint32_t input = spi->host->hw->data_buf[0];
  latestInput=input;
  gpio_set_level((gpio_num_t)pin_LED, input&(1<<(5+8*3)));

  for(int i=0;i<count;i++)
  {
    //  spi->host->hw->data_buf[i]=input;
    spi->host->hw->data_buf[i]=counter<<0;
    spi->host->hw->data_buf[i]|=counter<<8;
    spi->host->hw->data_buf[i]|=counter<<16;
    spi->host->hw->data_buf[i]|=counter<<24;
  }
  
	gpio_set_level((gpio_num_t) pin_SH_165, 1);//switch to keep to 165 register
	gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 1);
	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
  direct_spi_transfer(spi,sizeof(spi->host->hw->data_buf[0])*8*count);
  // direct_spi_transfer(spi,32);

	// gpio_set_level((gpio_num_t) PIN_NUM_CS, 0);
  counter++;
  counter&=0xFF;
}

hw_timer_t *timer = NULL;
spi_device_handle_t spi1;
void IRAM_ATTR onTimer()
{
  display_test(spi1);
  float freq=30*1000*2;
  int tick=20*1000*1000/freq;
  timerAlarmWrite(timer,tick, true);
}

void setup()
{
  Serial.begin(921600);
  spi1= direct_spi_init(1,40*1000*1000,PIN_NUM_MOSI,PIN_NUM_MISO,PIN_NUM_CLK,PIN_NUM_CS);

  gpio_set_direction((gpio_num_t)pin_SH_165,GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)pin_LED,GPIO_MODE_OUTPUT);
  
  dspi_device_select(spi1,1);
  // while(1)
  // {
  //  
  // }

  
  timer = timerBegin(0, 4, true);//20M
  
  timerAttachInterrupt(timer, &onTimer, true);

  timerAlarmWrite(timer, 100, true);
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
