#define LOOKER_STUBS_C

#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "looker_stubs.h"

#define EMPTY_INPUT_BUFFER

static int fd;

void looker_delay_1ms(void)
{
    usleep(1000);   //1ms
}

size_t looker_data_available(void)
{
    size_t bytes = 0;
    ioctl(fd, FIONREAD, &bytes);
    return bytes;
}

int looker_get(void *buf, int size)
{
    return read (fd, buf, size);
}

void looker_send(void *buf, int size)
{
    write (fd, buf, size);
}

static int set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        printf ("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_iflag &= ~ICRNL;  //do not translate CR -> LF

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        printf ("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}

static void set_blocking (int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        printf ("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
        printf ("error %d setting term attributes", errno);
}

int serial_init(const char *portname)
{
//Linux
    fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
//Mac OS
//    fd = open (portname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        printf ("error %d opening %s: %s\n", errno, portname, strerror (errno));
        return 1;
    }

    set_interface_attribs (fd, B57600, 0);  // set speed to 57600 bps, 8n1 (no parity)
    set_blocking (fd, 0);                // set no blocking

    //empty input buffer
#ifdef EMPTY_INPUT_BUFFER
    char buf[100];
    int n;
    do {
        n = read (fd, buf, sizeof(buf));
    } while (n > 0);
#endif //EMPTY_INPUT_BUFFER

    return 0;
}

#ifdef DEBUG
void debug_print(const char *s)
{
    printf("%s", s);
}

void debug_println2(const char *s, int i)
{
    printf("%s", s);
    printf("%d\n", i);
}
#endif //DEBUG

