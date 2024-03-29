

#define private public //dirty trick
#include "Eth_Extra.h"
#include <Ethernet.h>
#include <utility/w5100.h>
#undef private
 
inline void SPI_Write(unsigned int addr, unsigned char data)
{
  char Tmp;
  // Activate the CS pin
  SPI_PORT &= ~(1 << SPI_CS);
  // Start Wiznet W5100 Write OpCode transmission
  SPDR = WIZNET_WRITE_OPCODE;
  Tmp = (addr & 0xFF00) >> 8;
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));
  // Start Wiznet W5100 Address High Bytes transmission
  SPDR = Tmp;
  Tmp = addr & 0x00FF;
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));
  // Start Wiznet W5100 Address Low Bytes transmission
  SPDR = Tmp;
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));

  // Start Data transmission
  SPDR = data;
  Tmp = SPI_PORT | (1 << SPI_CS);
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));
  // CS pin is not active
  SPI_PORT = Tmp;
}
inline unsigned char SPI_Read(unsigned int addr)
{
  char Tmp;
  // Activate the CS pin
  SPI_PORT &= ~(1 << SPI_CS);
  // Start Wiznet W5100 Read OpCode transmission
  SPDR = WIZNET_READ_OPCODE;
  // Wait for transmission complete

  while (!(SPSR & (1 << SPIF)))Tmp = (addr & 0xFF00) >> 8;
  // Start Wiznet W5100 Address High Bytes transmission
  SPDR = Tmp;

  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)))Tmp = addr & 0x00FF;
  // Start Wiznet W5100 Address Low Bytes transmission
  SPDR = Tmp;
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));

  // Send Dummy transmission for reading the data
  SPDR = 0x00;

  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF))) Tmp = SPI_PORT | (1 << SPI_CS);

  // CS pin is not active
  SPI_PORT = Tmp;
  return (SPDR);
}
unsigned int SPI_Read16(unsigned int addr)
{
  unsigned int L = 0;
  L = SPI_Read(addr) << 8;
  L |= SPI_Read(addr + 1);
  return L;
}
unsigned int SPI_Write16(unsigned int addr, unsigned int data)
{
  SPI_Write(addr  , data >> 8);
  SPI_Write(addr + 1, data);
}

byte ReadInfo(byte CMD,byte _sock)
{
	return SPI_Read(ADDR(CMD,_sock));
}

byte ReadSnIR(byte _sock)
{
	return SPI_Read(ADDR(SnIR_,_sock));
}
byte ReadSnSR(byte _sock)
{
	return SPI_Read(ADDR(SnSR_,_sock));
}

void SetSnCR(byte data,byte _sock) 
{
  SPI_Write(ADDR(SnCR,_sock), data);
}


static void write2byte(uint16_t addr,uint16_t _data) {       
  uint8_t buf[2];                               
  buf[0] = _data >> 8;                          
  buf[1] = _data & 0xFF;                        
  W5100Class::write(addr, buf, 2);                       
}


static void write1byte(uint16_t addr,uint8_t _data) {  
  W5100Class::write(addr, &_data, 1);                       
}

void setRetryTimeout(byte retryTimes,unsigned int TimeOut100us)
{
  write2byte(RTR,TimeOut100us);//RTR
  write1byte(RCR,retryTimes);//RCR
  //W5100Class::writeRTR(TimeOut100us);//The address in Eth lib is wrong
  //W5100Class::writeRCR(retryTimes);


}

void testAlive(byte _sock)
{
  W5100Class::writeSnCR(_sock,SnCR_SEND_KEEP);
  ///W5100Class::writeSn(_sock,0x002F, 1);//writeSn_KPALVTR
}

void getIP(byte* buff,byte _sock)
{
	byte i=0;
	for(;i<4;i++)
		buff[i]= SPI_Read(ADDR(SnDIPR,_sock)+i);
}
void getMAC(byte* buff,byte _sock)
{
	byte i=0;
	for(;i<6;i++)
		buff[i]= SPI_Read(ADDR(SnDHAR,_sock)+i);
}
