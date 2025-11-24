#include <stdlib.h>
#include <unistd.h>

#include "efunc.h"
#include "util.h"

void ttclose() {
    ttopen(false);
}

void ttopen(bool enable) {
    if (enable) {
        if (tcgetattr(STDIN_FILENO, &Editor.original) == -1) die("tcgetattr");
        atexit(ttclose);
        struct termios raw = Editor.original;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
        write(STDOUT_FILENO, "\x1b[?1000l", 9);
        write(STDOUT_FILENO, "\x1b[?1002l", 9);
        write(STDOUT_FILENO, "\x1b[?1003l", 9);
    } else {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.original) == -1) die("tcsetattr");
        write(STDOUT_FILENO, "\x1b[?1049l", 8);
        write(STDOUT_FILENO, "\x1b[?1000l", 9);
        write(STDOUT_FILENO, "\x1b[?1002l", 9);
        write(STDOUT_FILENO, "\x1b[?1003l", 9);
    }
}