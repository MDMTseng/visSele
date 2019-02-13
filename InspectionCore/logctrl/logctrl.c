

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


char* _SubString(const char* str,int Count){
    static char ______SubString_Buff[100];
    if(str==NULL)return NULL;
    if(Count > (sizeof(______SubString_Buff)-1))
    {
        Count = (sizeof(______SubString_Buff)-1);
    }
    int i=0;
    for(i=0;i<Count;i++)
    {
        if(str[i]=='\0')break;
        ______SubString_Buff[i] = str[i];
    }
    ______SubString_Buff[i]='\0';
    return ______SubString_Buff;
}


char* _SubString_Align(const char* str,int Count){
    static char ______SubString_Buff[100];
    if(str==NULL)return NULL;
    if(Count > (sizeof(______SubString_Buff)-1))
    {
        Count = (sizeof(______SubString_Buff)-1);
    }
    int i=0;
    int isend=0;
    for(i=0;i<Count;i++)
    {
        if(!isend && str[i]=='\0')isend=1;
        if(isend)
            ______SubString_Buff[i] = ' ';
        else
            ______SubString_Buff[i] = str[i];
    }
    ______SubString_Buff[i]='\0';
    return ______SubString_Buff;
}