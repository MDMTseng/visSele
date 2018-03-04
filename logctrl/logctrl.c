

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
