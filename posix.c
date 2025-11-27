#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "estruct.h"

#define TBUFSIZ 128
static char tobuf[TBUFSIZ];

void ttclose()
{
        printf("\x1b[?1049l");
        fflush(stdout);
        tcsetattr(0, TCSADRAIN, &original);
}

void ttopen()
{
        tcgetattr(0, &original);
        struct termios raw = original;
        raw.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | INLCR | IGNCR | ICRNL | IXON);
        raw.c_oflag &= ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET);
        raw.c_cflag |= ~(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH | TOSTOP | ECHOCTL | ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(0, TCSADRAIN, &raw);
        setbuffer(stdout, &tobuf[0], TBUFSIZ);
        printf("\x1b[?1049h");
        fflush(stdout);
}
