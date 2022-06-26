//
//    FILE: MCP23017_digitalWrite.ino
//  AUTHOR: Rob Tillaart
//    DATE: 2019-10-14
// PUPROSE: test MCP23017 library


#include "MCP23017.h"
#include "Wire.h"




#include "spi_master.h"
#include <string.h>
#include <stdio.h>
#include "soc/gpio_sig_map.h"
#include "soc/spi_reg.h"
#include "soc/dport_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "rom/ets_sys.h"
#include "esp_types.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/xtensa_api.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/uart_struct.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "esp_heap_alloc_caps.h"


#include <string.h>
#include "esp_system.h"
#include "tftfunc.h"










MCP23017 MCP(0x20);

void scan()
{
  byte error, address; //variable for error and I2C address
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

  delay(5000); // wait 5 seconds for the next I2C scan
}



static void ili_init(spi_device_handle_t spi) 
{
    esp_err_t ret;
    //Initialize non-SPI GPIOs
  gpio_set_direction((gpio_num_t)PIN_NUM_DC, GPIO_MODE_OUTPUT);
	/* Reset and backlit pins are not used
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
	*/

	//Send all the commands
	ret = spi_device_select(spi, 0);
	assert(ret==ESP_OK);

    // commandList(spi, ILI9341_init);

	ret = spi_device_deselect(spi);
	assert(ret==ESP_OK);

    ///Enable backlight
    //gpio_set_level(PIN_NUM_BCKL, 0);
}


#define SPI_BUS VSPI_HOST
static void IRAM_ATTR ili_spi_pre_transfer_callback(spi_transaction_t *t) 
{
    int dc=(int)t->user;
    // gpio_set_level(PIN_NUM_DC, dc);
	// gpio_set_level((gpio_num_t) PIN_NUM_DC, 0);
}

static void IRAM_ATTR ili_spi_post_transfer_callback(spi_transaction_t *t) 
{
    int dc=(int)t->user;
    // gpio_set_level(PIN_NUM_DC, dc);
	// gpio_set_level((gpio_num_t) PIN_NUM_DC, 1);
}

void IRAM_ATTR _disp_spi_transfer_start(spi_device_handle_t handle, int bits) {
	// Load send buffer
	spi_host_t *host = handle->host;
	host->hw->user.usr_mosi_highpart = 0;
	host->hw->mosi_dlen.usr_mosi_dbitlen = bits-1;
	host->hw->user.usr_mosi = 1;
	if (handle->cfg.flags & SPI_DEVICE_HALFDUPLEX) {
		host->hw->miso_dlen.usr_miso_dbitlen = 0;
		host->hw->user.usr_miso = 0;
	}
	else {
		host->hw->miso_dlen.usr_miso_dbitlen = bits-1;
		host->hw->user.usr_miso = 1;
	}
	// Start transfer
	host->hw->cmd.usr = 1;
}


static void display_test(spi_device_handle_t spi, spi_device_handle_t tsspi) 
{


  int color = 0x0000;
  int ret = spi_device_select(spi, 0);
  assert(ret==ESP_OK);
  
	while (spi->host->hw->cmd.usr);


  int count=2;
  spi->host->hw->data_buf[0] =0x55;
  for(int i=0;i<count;i+=4)
  {
    spi->host->hw->data_buf[i] =0xA0+i*10+0;
    spi->host->hw->data_buf[i]|=(0xA0+i*10+1)<<8;
    spi->host->hw->data_buf[i]|=(0xA0+i*10+2)<<16;
    spi->host->hw->data_buf[i]|=(0xA0+i*10+3)<<24;
  }
	gpio_set_level((gpio_num_t) PIN_NUM_DC, 0);
  _disp_spi_transfer_start(spi,32*count);

	while (spi->host->hw->cmd.usr);
	gpio_set_level((gpio_num_t) PIN_NUM_DC, 1);
	gpio_set_level((gpio_num_t) PIN_NUM_DC, 0);
  _disp_spi_transfer_start(spi,32*count);

	ret = spi_device_deselect(spi);

	while (spi->host->hw->cmd.usr);
	gpio_set_level((gpio_num_t) PIN_NUM_DC, 1);

	// disp_spi_transfer_cmd(spi, TFT_CASET);
  // ret = spi_device_deselect(spi);
  // assert(ret==ESP_OK);


}
void SPI_INIT()
{
	int dmach = 1;
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_device_handle_t tsspi = NULL;
    spi_bus_config_t buscfg={
        .mosi_io_num=PIN_NUM_MOSI,
        .miso_io_num=PIN_NUM_MISO,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    spi_device_interface_config_t devcfg={0};

    devcfg.clock_speed_hz=10*1000*1000;                //Initial clock out at 5 MHz
    devcfg.mode=0;                                //SPI mode 0
    devcfg.spics_io_num=-1;                       //we will use external CS pin
		devcfg.spics_ext_io_num=PIN_NUM_CS;           //external CS pin
    devcfg.queue_size=7;                          //We want to be able to queue 7 transactions at a time
    // devcfg.pre_cb=ili_spi_pre_transfer_callback;  //Specify pre-transfer callback to handle D/C line
    // devcfg.post_cb=ili_spi_post_transfer_callback;  //Specify pre-transfer callback to handle D/C line
		devcfg.flags=SPI_DEVICE_HALFDUPLEX;           //Set half duplex mode (**IF NOT SET, READ FROM DISPLAY MEMORY WILL NOT WORK**)
    

#ifdef USE_TOUCH
	spi_device_interface_config_t tsdevcfg={
        .clock_speed_hz=2500000,                //Clock out at 2.5 MHz
        .mode=0,                                //SPI mode 2
        .spics_io_num=PIN_NUM_TCS,              //Touch CS pin
		.spics_ext_io_num=-1,                   //Not using the external CS
        .queue_size=3,                          //We want to be able to queue 3 transactions at a time
        .pre_cb=NULL,                           //No need for pre-transfer callback
    };
#endif

	//Initialize the SPI bus
    ret=spi_bus_initialize(SPI_BUS, &buscfg, dmach);
    assert(ret==ESP_OK);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(SPI_BUS, &devcfg, &buscfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: bus initialized\r\n");

	// Test select/deselect
	ret = spi_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_uses_native_pins(spi) ? "true" : "false");

#ifdef USE_TOUCH
    //Attach the TS to the SPI bus
    ret=spi_bus_add_device(SPI_BUS, &tsdevcfg, &buscfg, &tsspi);
    assert(ret==ESP_OK);

	// Test select/deselect
	ret = spi_device_select(tsspi, 1);
    assert(ret==ESP_OK);
	ret = spi_device_deselect(tsspi);
    assert(ret==ESP_OK);

	printf("SPI: attached TS device, speed=%u\r\n", get_speed(tsspi));
#endif

	//Initialize the LCD
	printf("SPI: display init...\r\n");
    ili_init(spi);
	printf("OK\r\n");
	vTaskDelay(500 / portTICK_RATE_MS);

	while (1) {
		//Start display test
		display_test(spi, tsspi);
		vTaskDelay(1 / portTICK_RATE_MS);
	}
}




void setup()
{
  Serial.begin(921600);
  Serial.print("MCP23017_test version: ");
  Serial.println(MCP23017_LIB_VERSION);
  SPI_INIT();
  Wire.begin();

  MCP.begin();
  pinMode(5,OUTPUT);
  scan();
  MCP.pinMode8(0, 0x00);  // 0 = output , 1 = input
  MCP.pinMode8(1, 0x00);
  Wire.setClock(700*1000);

  Serial.println("TEST digitalWrite(0)");
  for (int i = 0; i < 16; i++)
  {
     // MCP.digitalWrite(0, i &1);  // alternating HIGH/LOW
// 
    // MCP.writeReg(0x12,(i &1)?0xff:0)
    Wire.beginTransmission(0x20);
    Wire.write(0x12);
    Wire.write((i &1)?0xff:0);
    //delay(1);
    digitalWrite(5,1);
    Wire.endTransmission(false);
    digitalWrite(5,0);
    // MCP.write8(0,(i &1)?0xff:0);
    // Serial.print(i % 2);
    // Serial.print('\t');
    // delay(25);
  }
  Serial.println();
  Serial.println();

  Serial.println("TEST digitalWrite(pin)");
  for (int pin = 0; pin < 16; pin++)
  {
    MCP.digitalWrite(pin, 1 - pin % 2); // alternating HIGH/LOW
    Serial.print(1 - pin % 2);
    Serial.print('\t');
  }
  Serial.println();
  Serial.println();

  Serial.println("TEST read back");

  for (int pin = 0; pin < 16; pin++)
  {
    int val = MCP.digitalRead(pin);
    Serial.print(val);
    Serial.print('\t');
  }
  Serial.println();
  Serial.println();
}


void loop()
{
}


// -- END OF FILE --

