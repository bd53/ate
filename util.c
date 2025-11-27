#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "efunc.h"
#include "estruct.h"
#include "util.h"
#include "version.h"

void refresh_screen()
{
        printf("\x1b[2J");
        printf("\x1b[H");
        int rows = get_window_rows() - 2;
        int cols = get_window_cols();
        printf("\x1b[7m");
        char top_status[256];
        snprintf(top_status, sizeof(top_status), " %s %s", PROGRAM_NAME_LONG, VERSION);
        printf("%s", top_status);
        int top_status_len = strlen(top_status);
        for (int i = top_status_len; i < cols; i++) {
                printf(" ");
        }
        printf("\x1b[m\r\n");
        for (int i = 0; i < rows; i++) {
                int line_idx = i + editor.offset_y;
                if (line_idx < editor.line_numbers) {
                        printf("%s", editor.lines[line_idx]);
                } else {
                        printf("~");
                }
                printf("\x1b[K\r\n");
        }
        printf("\x1b[7m");
        char bottom_status[2048];
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
                snprintf(bottom_status, sizeof(bottom_status),
                         " %s | %s %s | Line %d/%d, Col %d", cwd,
                         editor.filename ? strrchr(editor.filename, '/') ? strrchr(editor.filename, '/') + 1 : editor.filename : "[No Name]",
                         editor.modified ? "[+]" : "", editor.cursor_y + 1,
                         editor.line_numbers, editor.cursor_x + 1);
        } else {
                snprintf(bottom_status, sizeof(bottom_status),
                         " %s %s | Line %d/%d, Col %d",
                         editor.filename ? editor.filename : "[No Name]",
                         editor.modified ? "[+]" : "", editor.cursor_y + 1,
                         editor.line_numbers, editor.cursor_x + 1);
        }
        printf("%s", bottom_status);
        int bottom_status_len = strlen(bottom_status);
        for (int i = bottom_status_len; i < cols; i++) {
                printf(" ");
        }
        printf("\x1b[m");
        printf("\x1b[%d;%dH", editor.cursor_y - editor.offset_y + 2, editor.cursor_x + 1);
        fflush(stdout);
}

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
