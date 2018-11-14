

#include <logctrl.h>
#include <stdarg.h>

static void log_error(const char *fmt, va_list ap)
{

    vprintf(fmt, ap);
}

void logv(const char *fmt, ...)
{
    va_list     ap;
    va_start(ap, fmt);
    log_error(fmt, ap);
    va_end(ap);
}




void logd(const char *fmt, ...)
{
    va_list     ap;
    va_start(ap, fmt);
    log_error(fmt, ap);
    va_end(ap);
}




void logi(const char *fmt, ...)
{
    va_list     ap;
    va_start(ap, fmt);
    log_error(fmt, ap);
    va_end(ap);
}




void loge(const char *fmt, ...)
{
    va_list     ap;
    va_start(ap, fmt);
    log_error(fmt, ap);
    va_end(ap);
}



char* byteArrString(uint8_t *data, int length)
{
    return byteArrString(data,  length,4);
}

char* byteArrString(uint8_t *data, int length,int spaceInterval)
{
    const int buffL = 500;
    static char buff[buffL+30];

    int maxArrL = buffL/(spaceInterval*2+1)*spaceInterval;//spaceInterval=4 => print is 095A4B65 226C7CC8 AABCDBF4
    int pLength = length;
    if(pLength>maxArrL)
    {
        pLength = maxArrL;
    }
    char *strptr=buff;

    int scount=0;
    for (int i = 0; i < pLength; i++)
    {
        strptr+=sprintf(strptr,"%02X", data[i]);
        if(scount++==spaceInterval)
        {
            strptr+=sprintf(strptr," ");
            scount=0;
        }
    }
    if(pLength!=length)
    {
        strptr += sprintf(strptr,"...(%d more)", length - pLength);
    }

    return buff;
}
