#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "efunc.h"
#include "estruct.h"
#include "util.h"
#include "version.h"

int get_window_rows(void)
{
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_row == 0) {
                return 24;
        }
        return ws.ws_row;
}

int get_window_cols(void)
{
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
                return 80;
        }
        return ws.ws_col;
}
