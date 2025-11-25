#include <stdlib.h>
#include <unistd.h>

#include "edef.h"
#include "efunc.h"
#include "util.h"

static const char *exit_sequences[] = {
    "\x1b[?1049l", // restore
    "\x1b[?1000l", // mouse tracking
    "\x1b[?1002l", // extended mouse tracking
    "\x1b[?1003l" // absolute mouse reporting
};

void ttclose(void) {
    ttopen(false);
}

void ttopen(bool enable) {
    if (enable) {
        if (tcgetattr(STDIN_FILENO, &Editor.original) == -1) die("tcgetattr");
        atexit(ttclose);
        struct termios raw = Editor.original;
        raw.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | INLCR | IGNCR | ICRNL | IXON);
        raw.c_oflag &= ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET);
        raw.c_cflag |= ~(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH | TOSTOP | ECHOCTL | ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw) == -1) die("tcsetattr");
        write(STDOUT_FILENO, "\x1b[?1049h", ESC_SEQ_LEN);
    } else {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.original) == -1) die("tcsetattr");
        for (size_t i = 0; i < sizeof(exit_sequences) / sizeof(exit_sequences[0]); ++i) write(STDOUT_FILENO, exit_sequences[i], ESC_SEQ_LEN);
    }
}
