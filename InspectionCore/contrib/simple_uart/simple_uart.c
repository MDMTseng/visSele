#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifndef WIN32
#include <regex.h>      // POSIX regex -- mingw has no equivalent header
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <glob.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <libgen.h>
#else
#include <windows.h>
#include <timeapi.h>    /* timeBeginPeriod -- raise timer res so Sleep(1)~=1ms */
#endif

#ifdef __linux__
#include <pty.h>
#include <linux/serial.h>
#endif

#include "simple_uart.h"

#if defined(__linux__) || defined(__APPLE__)
/* This requires you to link against -lrt
 */
#define mseconds() (int)({ struct timespec _ts; \
                      clock_gettime(CLOCK_MONOTONIC, &_ts); \
                      _ts.tv_sec * 1000 + _ts.tv_nsec / (1000 * 1000); \
                   })
#else
#define mseconds() GetTickCount()
#endif

#ifndef TIOCSRS485
#define TIOCSRS485   0x542F
#endif
#ifndef SER_RS485_RX_DURING_TX
#define SER_RS485_RX_DURING_TX  (1 << 4)
#endif


struct simple_uart
{
#ifdef WIN32
    HANDLE port;
#else
    int fd;
#endif
    int char_delay_us;
    FILE *logfile;
};

int simple_uart_read(struct simple_uart *sc, void *buffer, int max_len)
{
    return simple_uart_read_timed(sc, buffer, max_len,50);
}


int simple_uart_read_timed(struct simple_uart *sc, void *buffer, int max_len,int wait_ms)
{
    
    int r;
#ifdef WIN32
    /* Immediate-return reads (MSDN: ReadIntervalTimeout == MAXDWORD with both
     * total-timeout fields == 0).  ReadFile returns AT ONCE with whatever bytes
     * are already buffered -- it never blocks the handle.  This matters twice
     * over on a non-overlapped COM handle:
     *   1. A reply is delivered the instant we poll for it, instead of being
     *      held hostage by a blocking read (the old MAXDWORD/MAXDWORD/30 config
     *      stalled replies for tens of ms up to ~1.8s).
     *   2. Synchronous I/O on a non-overlapped handle is serialized, so a
     *      blocking ReadFile also starved the concurrent WriteFile on the TX
     *      thread.  An instant read never holds the handle, so writes flow.
     * We then poll with a plain Sleep(1).  Because the port is opened with
     * timeBeginPeriod(1) (see simple_uart_set_config) a Sleep(1) is really ~1ms,
     * so a reply is picked up within ~1ms of arriving -- giving ~6ms end-to-end
     * with essentially zero CPU.  Deliberately NO busy-spin here: this read runs
     * on a dedicated RX thread, and spinning would steal a core from the WS /
     * BPG-forwarding thread that has to relay each reply back to the WebUI,
     * which (ironically) inflated the round-trip the caller actually sees. */
    COMMTIMEOUTS commTimeout;
    if (GetCommTimeouts(sc->port, &commTimeout)) {
        commTimeout.ReadIntervalTimeout        = MAXDWORD;
        commTimeout.ReadTotalTimeoutMultiplier = 0;
        commTimeout.ReadTotalTimeoutConstant   = 0;
        SetCommTimeouts(sc->port, &commTimeout);
    }
    LARGE_INTEGER freq, t0, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(sc->port, buffer, max_len, &got, NULL)) {
            DWORD err = GetLastError();
            /* A serial line error (overrun / framing / parity / break) LATCHES
             * the port: ReadFile then keeps failing until ClearCommError resets
             * it.  Such errors are transient (a stray byte, or RX/TX racing on
             * this non-overlapped handle) and must NOT tear down the channel --
             * the caller treats any negative return as a hard disconnect, which
             * was causing the connection to flap (UART DESTRUCT -> reopen loop).
             * Clear the latch and keep polling; only a genuinely dead handle
             * (device unplugged / access lost) is reported as fatal. */
            DWORD commErr = 0; COMSTAT cs;
            ClearCommError(sc->port, &commErr, &cs);
            if (err == ERROR_INVALID_HANDLE  || err == ERROR_ACCESS_DENIED ||
                err == ERROR_BAD_COMMAND     || err == ERROR_DEVICE_REMOVED ||
                err == ERROR_GEN_FAILURE     || err == ERROR_FILE_NOT_FOUND)
                return -(int)err;        /* device really gone -> caller disconnects */
            got = 0;   /* transient line error: cleared, keep polling */
        }
        if (got != 0) { r = (int)got; break; }
        QueryPerformanceCounter(&now);
        double elapsed_ms = (double)(now.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;
        if (elapsed_ms >= wait_ms) { r = 0; break; }
        Sleep(1);   /* ~1ms at 1ms timer res; no busy-spin, no CPU contention */
    }
#else
    fd_set readfds, exceptfds;
    struct timeval t;

    // Have a timeout of 50ms to avoid just thrashing the CPU
    FD_ZERO(&readfds);
    FD_SET(sc->fd, &readfds);
    FD_ZERO(&exceptfds);
    FD_SET(sc->fd, &exceptfds);
    t.tv_sec = wait_ms/1000;
    t.tv_usec = (wait_ms%1000) * 1000;

    r = select(sc->fd + 1, &readfds, NULL, &exceptfds, &t);
    if (r < 0)
        return -errno;
    if (r == 0)
        return 0;
    r = read (sc->fd, buffer, max_len);
    if (r < 0)
        r = -errno;
#endif
    if (r > 0 && sc->logfile) {
        fwrite(buffer, r, 1, sc->logfile);
        fflush(sc->logfile);
    }
    return r;
}



int simple_uart_write(struct simple_uart *sc, const void *buffer, int len)
{
#ifdef WIN32
    int r = 0;
    /* TODO: Support char_delay_us */
    /* A failed WriteFile used to be reported as "0 bytes written", which every
     * caller counts as success -- on the deployed platform an unplugged or
     * wedged port looked exactly like a healthy quiet one, and verdicts
     * vanished without a counter moving. */
    if (!WriteFile (sc->port, buffer, len, (LPDWORD)&r, NULL))
        return -1;
#else
    int r;
    if (sc->char_delay_us > 0) {
        const uint8_t *buf8 = (const uint8_t *)buffer;
        for (int i = 0; i < len; i++) {
            int e = write(sc->fd, &buf8[i], 1);
            if (e < 0)
                return e;
            if (e == 0)
                return i;
            usleep(sc->char_delay_us);
        }
        r = len;
    } else {
        r = write (sc->fd, buffer, len);
        if (r < 0)
            r = -errno;
    }
#endif

    if (r > 0 && sc->logfile) {
        fwrite(buffer, r, 1, sc->logfile);
        fflush(sc->logfile);
    }

    return r;
}

static int simple_uart_set_config(struct simple_uart *sc, int speed, const char *mode_string)
{
#ifdef WIN32
    DCB dcbConfig;
    if (GetCommState (sc->port, &dcbConfig)) {
        // Parse mode_string: "<DataBits><Parity><StopBits>[ <flow>]"
        //   e.g. "8N1"          -> 8 data, No parity, 1 stop, NO flow control
        //        "7E1 rtscts"   -> 7 data, Even parity, 1 stop, HW (RTS/CTS)
        //        "8N1 xonxoff"  -> software flow control
        // Flow defaults to "none" -- a USB-UART (e.g. CH340) with a floating,
        // undriven CTS would otherwise make WriteFile block for seconds
        // waiting on a CTS that never asserts.
        int  dataBits = 8;
        char parity   = 'N';
        int  stopBits = 1;
        char flow[16] = "none";
        if (mode_string && strlen(mode_string) >= 3) {
            if (mode_string[0] >= '5' && mode_string[0] <= '8') dataBits = mode_string[0] - '0';
            char p = mode_string[1];
            if (p >= 'a' && p <= 'z') p -= 32;                       /* toupper */
            if (p == 'N' || p == 'E' || p == 'O') parity = p;
            if (mode_string[2] == '1' || mode_string[2] == '2') stopBits = mode_string[2] - '0';
            const char *sp = strpbrk(mode_string, " ,");             /* optional flow token */
            if (sp) {
                while (*sp == ' ' || *sp == ',') sp++;
                int i = 0;
                while (sp[i] && sp[i] != ' ' && i < (int)sizeof(flow) - 1) {
                    char c = sp[i]; if (c >= 'A' && c <= 'Z') c += 32; /* tolower */
                    flow[i++] = c;
                }
                flow[i] = '\0';
            }
        }
        int useRtsCts = (strcmp(flow, "rtscts") == 0 || strcmp(flow, "hw") == 0);
        int useXon    = (strcmp(flow, "xonxoff") == 0 || strcmp(flow, "sw") == 0);

        dcbConfig.BaudRate = speed;
        dcbConfig.ByteSize = (BYTE)dataBits;
        dcbConfig.Parity   = (parity == 'E') ? EVENPARITY : (parity == 'O') ? ODDPARITY : NOPARITY;
        dcbConfig.StopBits = (stopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;
        dcbConfig.fBinary  = TRUE;
        dcbConfig.fParity  = (parity == 'N') ? FALSE : TRUE;
        // Flow control (default OFF).
        dcbConfig.fOutxCtsFlow      = useRtsCts ? TRUE : FALSE;
        dcbConfig.fRtsControl       = useRtsCts ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
        dcbConfig.fOutxDsrFlow      = FALSE;
        dcbConfig.fDsrSensitivity   = FALSE;
        dcbConfig.fDtrControl       = DTR_CONTROL_ENABLE;
        dcbConfig.fOutX             = useXon ? TRUE : FALSE;
        dcbConfig.fInX              = useXon ? TRUE : FALSE;
        dcbConfig.fTXContinueOnXoff = TRUE;
        dcbConfig.fAbortOnError     = FALSE;
        SetCommState (sc->port, &dcbConfig);
        printf("[simple_uart] %s: %d %d%c%d flow=%s\n",
               sc ? "cfg" : "cfg", speed, dataBits, parity, stopBits, flow);
    }
    /* Raise the system timer resolution to 1ms (default ~15.6ms).  The read
     * poll loop falls back to Sleep(1) when idle; at the default resolution
     * that Sleep is really ~15.6ms, which quantizes a reply that arrives on a
     * *different* thread (the RX thread) up to a full tick -- turning a ~5ms
     * round-trip into ~16ms.  1ms resolution pulls it back to the ~5ms device
     * baseline.  Process-global and idempotent enough to call once per open. */
    {
        static volatile LONG s_timer_res_set = 0;
        if (InterlockedCompareExchange(&s_timer_res_set, 1, 0) == 0)
            timeBeginPeriod(1);
    }
    // Bound the write so a wedged line can never block a write indefinitely
    // (the read path only ever sets the READ timeouts, so writes were
    // unbounded). GetCommTimeouts first to preserve the read-timeout fields.
    {
        COMMTIMEOUTS wt;
        if (GetCommTimeouts (sc->port, &wt)) {
            wt.WriteTotalTimeoutMultiplier = 0;
            wt.WriteTotalTimeoutConstant   = 1000;   // 1s hard cap
            SetCommTimeouts (sc->port, &wt);
        }
    }
    return 0;
#else
    struct termios options;
    int sp;
#ifdef __linux__
    int non_standard = 0;
#endif
    printf("speed:%d<<<<<<<<<<<<<<\n",speed);
    switch (speed)
    {
    case 1200:
        sp = B1200;
        break;
    case 2400:
        sp = B2400;
        break;
    case 4800:
        sp = B4800;
        break;
    case 9600:
        sp = B9600;
        break;
    case 19200:
        sp = B19200;
        break;
    case 38400:
        sp = B38400;
        break;
    case 57600:
        sp = B57600;
        break;
    case 115200:
        sp = B115200;
        break;
    case 230400:
        sp = B230400;
        break;
    case 460800:
        sp = 460800;
        break;
    case 921600:
        sp = 921600;
        break;
    case 1152000:
        sp = 1152000;
        break;
#ifdef __linux__
    case 460800:
        sp = B460800;
        break;
    case 500000:
        sp = B500000;
        break;
    case 576000:
        sp = B576000;
        break;
    case 921600:
        sp = B921600;
        break;
    case 1000000:
        sp = B1000000;
        break;
    case 1152000:
        sp = B1152000;
        break;
    case 1500000:
        sp = B1500000;
        break;
    case 2000000:
        sp = B2000000;
        break;
    case 2500000:
        sp = B2500000;
        break;
    case 3000000:
        sp = B3000000;
        break;
    case 3500000:
        sp = B3500000;
        break;
    case 4000000:
        sp = B4000000;
        break;
#endif

    default:
        sp = speed;
#ifdef __linux__
        non_standard = 1;
#endif
    }

    // printf("sp:%d<<<<<<<<<<<<<<\n",sp);
    if (tcgetattr(sc->fd, &options) < 0)
        return -errno;

    cfsetospeed(&options, sp);
    cfsetispeed(&options, sp);

    options.c_cflag &= ~(HUPCL);

    options.c_cflag |= CREAD | CLOCAL;

#define HAS_OPTION(a) (strchr (mode_string, a) != NULL || strchr (mode_string, tolower(a)) != NULL)

    // parity
    if (HAS_OPTION ('N'))
        options.c_cflag &= ~PARENB;
    else if (HAS_OPTION ('E')) {
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
    } else if (HAS_OPTION ('O')) {
        options.c_cflag |= PARENB;
        options.c_cflag |= PARODD;
    }
    // stop bits
    if (HAS_OPTION ('2'))
        options.c_cflag |= CSTOPB;
    else
        options.c_cflag &= ~CSTOPB;
    /* Flush data on each write */
    if (HAS_OPTION('W'))
        options.c_lflag |= NOFLSH;

    // Character size
    options.c_cflag &= ~CSIZE;
    if (HAS_OPTION ('8'))
        options.c_cflag |= CS8;
    else if (HAS_OPTION ('7'))
        options.c_cflag |= CS7;
    else if (HAS_OPTION ('6'))
        options.c_cflag |= CS6;
    else if (HAS_OPTION ('5'))
        options.c_cflag |= CS5;

    /* Flow control */
    if (HAS_OPTION('F'))
        options.c_cflag |= CRTSCTS;
    else
        options.c_cflag &= ~CRTSCTS;

    // raw input mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // disable software flow control
    options.c_iflag &= ~(IXON | IXOFF | IXANY);

    // maintain carriage return on input, and don't translate it
    options.c_iflag &= ~(IGNCR | ICRNL | INLCR);

    // raw output mode
    options.c_oflag &= ~OPOST;

    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 0;
    tcflush(sc->fd, TCIOFLUSH);
    if (tcsetattr(sc->fd, TCSANOW, &options) < 0)
        return -errno;

#ifdef __linux__
    if (HAS_OPTION('R')) {
        struct serial_rs485 rs485;
        rs485.flags = SER_RS485_ENABLED | SER_RS485_RX_DURING_TX | SER_RS485_RTS_ON_SEND;
        if (ioctl(sc->fd, TIOCSRS485, &rs485) < 0)
            return -errno;
    }

    if (non_standard) {
        struct serial_struct ss;

        /* Get current settings */
        if (ioctl(sc->fd, TIOCGSERIAL, &ss) < 0)
            return -errno;
        /* Check we can divide down */
        if ((ss.baud_base / speed) == 0)
            return -EINVAL;
        ss.flags &= ~(ASYNC_SPD_MASK);
        ss.flags |= ASYNC_SPD_CUST;
        ss.custom_divisor = ss.baud_base / speed;
        if (ioctl(sc->fd, TIOCSSERIAL, &ss) < 0)
            return -errno;
    }
#endif
    return 0; 
#endif
}

int simple_uart_close(struct simple_uart *sc)
{
    if (!sc)
        return -EINVAL;

#if defined(__linux__) || defined(__APPLE__)
    close(sc->fd);
#else
    CloseHandle(sc->port);
#endif
    free(sc);

    return 0;
}

struct simple_uart *simple_uart_open(const char *device, int speed, const char *mode_string)
{
    struct simple_uart *retval;

#ifdef WIN32
    HANDLE port;
    DWORD mode = GENERIC_READ | GENERIC_WRITE;
    char full_port_name[32];

    snprintf(full_port_name, sizeof(full_port_name), "\\\\.\\%s", device);
    full_port_name[sizeof(full_port_name) - 1] = '\0';

    port = CreateFile (full_port_name, mode, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (port == INVALID_HANDLE_VALUE)
        return NULL;
    retval = (struct simple_uart*)calloc(sizeof(struct simple_uart), 1);
    retval->port = port;
#else
    int fd;
    int mode = O_RDWR | O_NDELAY | O_NOCTTY;

    fd = open(device, mode);

    if (fd == -1)
        return NULL;

    signal(SIGIO, SIG_IGN); // so we don't get those 'I/O possible' lines

    fcntl(fd, F_SETFL, 0);
    retval = (struct simple_uart *)calloc(sizeof(struct simple_uart), 1);
    if (!retval)
    {
        close(fd);
        return NULL;
    }
    retval->fd = fd;
#endif
    if (simple_uart_set_config(retval, speed, mode_string) < 0) {
        simple_uart_close(retval);
        return NULL;
    }
    return retval;
}

int simple_uart_set_character_delay(struct simple_uart *sc, int delay_us)
{
    int old_delay = sc->char_delay_us;
    sc->char_delay_us = delay_us;
    return old_delay;
}

int simple_uart_list(char ***namesp, char ***descriptionp)
{
#if defined(__linux__) || defined(__APPLE__)
    glob_t g;
    char **names = NULL;
    char **description = NULL;
    int count = 0;
    int i;

#ifdef __linux__
    if (glob("/sys/class/tty/ttyS[0-9]*", 0, NULL, &g) >= 0) {
        char buffer[100];
        char **new_names;
        new_names = realloc(names, (count + g.gl_pathc) * sizeof(char *));
        if (!new_names) {
            globfree(&g);
            free(names);
            return -ENOMEM;
        }
        names = new_names;
        for (i = count; i < count + g.gl_pathc; i++) {
            sprintf(buffer, "/dev/%s", basename(g.gl_pathv[i - count]));
            names[i] = strdup(buffer);
        }
        count += g.gl_pathc;
        globfree (&g);
    }

    if (glob ("/sys/class/tty/ttyUSB[0-9]*", 0, NULL, &g) >= 0) {
        char buffer[100];
        char **new_names;
        new_names = realloc(names, (count + g.gl_pathc) * sizeof (char *));
        if (!new_names) {
            globfree(&g);
            free(names);
            return -ENOMEM;
        }
        names = new_names;
        for (i = count; i < count + g.gl_pathc; i++) {
            sprintf (buffer, "/dev/%s", basename (g.gl_pathv[i - count]));
            names[i] = strdup (buffer);
        }
        count += g.gl_pathc;
        globfree (&g);
    }
#endif

#ifdef __APPLE__
        if (glob ("/dev/tty.*", 0, NULL, &g) >= 0) {
        char buffer[100];
        char **new_names;
        new_names = (char **)realloc(names, (count + g.gl_pathc) * sizeof (char *));
        if (!new_names) {
            globfree(&g);
            free(names);
            return -ENOMEM;
        }
        names = new_names;
        for (i = count; i < count + g.gl_pathc; i++) {
            sprintf (buffer, "/dev/%s", basename (g.gl_pathv[i - count]));
            names[i] = strdup (buffer);
        }
        count += g.gl_pathc;
        globfree (&g);
    }
#endif

    *namesp = names;
    *descriptionp = description;
    return count;
#else
    int pos = 0;
    char **names = NULL;

    for (int i = 0; i < 255; i++) {
        char buffer[10];
        char target[255];
        sprintf(buffer, "COM%d", i + 1);
        if (QueryDosDevice(buffer, target, sizeof(target)) > 0) {
            char **new_names = (char**)realloc(names, (pos + 1) * sizeof (char *));
            if (!new_names)
                continue;
            names = new_names;
            names[pos] = (char*)malloc(strlen (buffer) + 1);
            strcpy(names[pos], buffer);
            pos++;
        }
    }
    *namesp = names;
    return pos;
#endif
}

int simple_uart_set_logfile(struct simple_uart *uart, const char *logfile, ...)
{
    va_list ap;
    char *buffer;
    int len;

    va_start(ap, logfile);
    len = vasprintf(&buffer, logfile, ap);
    va_end(ap);
    if (len < 0)
        return -errno;
    if (uart->logfile) {
        fclose(uart->logfile);
        uart->logfile = NULL;
    }
    uart->logfile = fopen(buffer, "a");
    if (!uart->logfile) {
        int e = -errno;
        free(buffer);
        return e;
    }
    free(buffer);
    return 0;
}

int simple_uart_read_line(struct simple_uart *uart, char *buffer, int max_len, int timeout)
{
    int pos = 0;
    int last = mseconds();
    int now;

    if (!buffer || max_len == 0)
        return -EINVAL;

    *buffer = '\0';

    do {
        char ch;
        int ret = simple_uart_read(uart, &ch, 1);
        if (ret < 0)
            return ret;
        if (ret > 0) {
            if (ch == '\n' || ch == '\r') {
                break;
            } else {
                last = mseconds();
                buffer[pos] = ch;
                buffer[pos + 1] = '\0';
                pos++;
            }
        }
        now = mseconds();
    } while (pos < max_len - 1 && now - last < timeout);

    /* If we got a carriage return + line feed, just collapse them */
    while (pos > 0 && (buffer[pos - 1] == '\r' || buffer[pos - 1] == '\n')) {
        buffer[pos - 1] = '\0';
        pos--;
    }

    if (now - last >= timeout)
        return -ETIMEDOUT;

    return pos;
}

int simple_uart_send_break(struct simple_uart *uart)
{
#if defined(__linux__) || defined(__APPLE__)
    tcsendbreak(uart->fd, 1);
    return 0;
#else
    SetCommBreak(uart->port);
    // Linux doesn't support durations, it is always 4/10 of a second.
    // Replicate that here.
    usleep(400*1000);
    ClearCommBreak(uart->port);
    return 0;
#endif
}

#ifdef WIN32
HANDLE simple_uart_get_handle(struct simple_uart *uart)
{
    return uart->port;
}
#else
int simple_uart_get_fd(struct simple_uart *uart)
{
    return uart->fd;
}
#endif

int simple_uart_get_pin(struct simple_uart *uart, int pin)
{
#if defined(__linux__) || defined(__APPLE__)
    int status;

    if (ioctl(uart->fd, TIOCMGET, &status) < 0)
        return -errno;

    switch (pin) {
    case SIMPLE_UART_CTS:
        return (status & TIOCM_CTS) ? 1 : 0;
    case SIMPLE_UART_DSR:
        return (status & TIOCM_DSR) ? 1 : 0;
    case SIMPLE_UART_DCD:
        return (status & TIOCM_CAR) ? 1 : 0;
    case SIMPLE_UART_RI:
        return (status & TIOCM_RI) ? 1 : 0;
    default:
        return -EINVAL;
    }
#else
    DWORD status;
    if (!GetCommModemStatus(uart->port, &status))
        return -GetLastError();

    switch (pin) {
    case SIMPLE_UART_CTS:
        return (status & MS_CTS_ON) ? 1 : 0;
    case SIMPLE_UART_DSR:
        return (status & MS_DSR_ON) ? 1 : 0;
    case SIMPLE_UART_DCD:
        return (status & MS_RLSD_ON) ? 1 : 0;
    case SIMPLE_UART_RI:
        return (status & MS_RING_ON) ? 1 : 0;
    default:
        return -EINVAL;
    }
#endif
}

int simple_uart_set_pin(struct simple_uart *uart, int pin, bool high)
{
#if defined(__linux__) || defined(__APPLE__)
    int bits;

    if (!uart || uart->fd < 0)
        return -EINVAL;

    switch (pin) {
    case SIMPLE_UART_RTS:
        bits = TIOCM_RTS;
        break;
    case SIMPLE_UART_DTR:
        bits = TIOCM_DTR;
        break;
    default:
        return -EINVAL;
    }
    if (ioctl(uart->fd, high ? TIOCMBIS : TIOCMBIC, &bits) < 0)
        return -errno;

    return 0;
#else
    bool res;
    switch(pin) {
    case SIMPLE_UART_RTS:
        res = EscapeCommFunction(uart->port, high ? SETRTS : CLRRTS);
        break;
    case SIMPLE_UART_DTR:
        res = EscapeCommFunction(uart->port, high ? SETDTR : CLRDTR);
        break;
    default:
        return -EINVAL;
    }
    if (!res)
        return -GetLastError();
    return 0;
#endif
}
