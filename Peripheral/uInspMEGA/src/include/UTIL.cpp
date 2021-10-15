#include "UTIL.hpp"

#include "Arduino.h"

#include <avr/wdt.h>


void DEBUG_printf(const char * format, ...)
{
	char buff[200];
	va_list list;
	va_start(list, format);
	vsnprintf(buff, sizeof(buff), format, list);
	va_end(list);
	DEBUG_print(buff);

}



uint32_t _rotl(uint32_t value, int shift)
{
  if ((shift &= 31) == 0)
    return value;
  return (value << shift) | (value >> (32 - shift));
}
uint32_t cmpCheckSum(int shift, int num_args, ...)
{
  uint32_t val = 0;
  va_list ap;
  int i;

  va_start(ap, num_args);
  for (i = 0; i < num_args; i++)
  {
    val ^= _rotl(va_arg(ap, uint32_t), 3);
    val = _rotl(val, shift);
  }
  va_end(ap);

  return val;
}





void HARD_RESET()
{
  cli();                 // Clear interrupts
  wdt_enable(WDTO_15MS); // Set the Watchdog to 15ms
  while (1)
    ; // Enter an infinite loop
}