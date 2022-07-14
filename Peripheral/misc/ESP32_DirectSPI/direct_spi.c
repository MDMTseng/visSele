

#include "direct_spi.h"


#define SPI_BUS VSPI_HOST

void IRAM_ATTR direct_spi_transfer(spi_device_handle_t handle, int bits) {
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




spi_device_handle_t direct_spi_init(int ch,int clkHz,int pin_MOSI,int pin_MISO,int pin_CLK,int pin_CS)
{
	int dmach = ch;
  esp_err_t ret;
  spi_device_handle_t spi;
  spi_bus_config_t buscfg={
      .mosi_io_num=pin_MOSI,
      .miso_io_num=pin_MISO,
      .sclk_io_num=pin_CLK,
      .quadwp_io_num=-1,
      .quadhd_io_num=-1
  };
  spi_device_interface_config_t devcfg={0};

  devcfg.clock_speed_hz=clkHz;                //Initial clock out at 5 MHz
  devcfg.mode=0;                                //SPI mode 0
  devcfg.spics_io_num=-1;                       //we will use external CS pin
  devcfg.spics_ext_io_num=pin_CS;           //external CS pin
  devcfg.queue_size=7;                          //We want to be able to queue 7 transactions at a time
  devcfg.flags=0;//SPI_DEVICE_HALFDUPLEX;           //Set half duplex mode (**IF NOT SET, READ FROM DISPLAY MEMORY WILL NOT WORK**)


//Initialize the SPI bus
  ret=spi_bus_initialize(SPI_BUS, &buscfg, dmach);
  assert(ret==ESP_OK);
  //Attach the LCD to the SPI bus
  ret=spi_bus_add_device(SPI_BUS, &devcfg, &buscfg, &spi);
  assert(ret==ESP_OK);

  return spi;
}

