#include "UTIL.hpp"



void DEBUG_printf(const char * format, ...)
{
	char buff[200];
	va_list list;
	va_start(list, format);
	vsnprintf(buff, sizeof(buff), format, list);
	va_end(list);
	DEBUG_print(buff);

}
