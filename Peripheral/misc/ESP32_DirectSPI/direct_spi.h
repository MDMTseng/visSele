#ifndef _DIRECT_SPI_
#define _DIRECT_SPI_

#include "spi_master.h"
#include "tftfunc.h"


void IRAM_ATTR direct_spi_transfer(spi_device_handle_t handle, int bits);
spi_device_handle_t direct_spi_init(int ch,int clkHz,int pin_MOSI,int pin_MISO,int pin_CLK,int pin_CS);

#define direct_spi_in_use(spi) (spi->host->hw->cmd.usr)

#endif