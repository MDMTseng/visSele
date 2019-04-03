#ifndef ETH_EXTRA_H
#define ETH_EXTRA_H


#include <Arduino.h>
#ifdef __cplusplus
 extern "C" {
#endif
#define _SOCKINC 0x100
#define _SOCKBASE 0x400

#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_CS   PORTB2
// Wiznet W5100 Op Code
#define WIZNET_WRITE_OPCODE 0xF0
#define WIZNET_READ_OPCODE 0x0F
// Wiznet W5100 Register Addresses
#define MR   0x0000   // Mode Register
#define GAR  0x0001   // Gateway Address: 0x0001 to 0x0004
#define SUBR 0x0005   // Subnet mask Address: 0x0005 to 0x0008
#define SAR  0x0009   // Source Hardware Address (MAC): 0x0009 to 0x000E
#define SIPR 0x000F   // Source IP Address: 0x000F to 0x0012
#define RMSR 0x001A   // RX Memory Size Register
#define TMSR 0x001B   // TX Memory Size Register

#define RTR 0x0017   // 16b Timeout 100us
#define RCR 0x0019   // 8b  retry times

#define SnCR      0x1   //sock CMD
#define SnCR_SEND_KEEP 0x22

#define SnDIPR      0xC//sock IP
#define SnDHAR      0x6//sock MAC

//Add _ since w5100 put SnIR SnSR as a class
#define SnSR_      0x3   //sock state
#define SnIR_ 0x2


#define MaxSockNumber 4

#define ADDR(CMD,_SOCK) (_SOCK*_SOCKINC+_SOCKBASE+CMD)


unsigned int SPI_Read16(unsigned int addr);
unsigned int SPI_Write16(unsigned int addr,unsigned int data);
inline void SPI_Write(unsigned int addr,unsigned char data);
inline unsigned char SPI_Read(unsigned int addr);

byte ReadSn_IR(byte _sock);
byte ReadSnSR(byte _sock);
void SetSnCR(byte data,byte _sock);
void setRetryTimeout(byte retryTimes,unsigned int TimeOut100us);
void testAlive(byte _sock);
void getMAC(byte* buff,byte _sock);
void getIP(byte* buff,byte _sock);


#ifdef __cplusplus
}
#endif
#endif
