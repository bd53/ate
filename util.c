#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
        int cursor_screen_row = -1;
        int cursor_screen_col = 0;
        int current_screen_row = 0;
        for (int i = editor.offset_y;
             i < editor.line_numbers && current_screen_row < rows; i++) {
                char *line = editor.lines[i];
                int len = strlen(line);
                int col = 0;
                int line_row_offset = 0;
                int is_cursor_line = (i == editor.cursor_y);
                int cursor_found = 0;
                for (int j = 0; j < len; j++) {
                        char c = line[j];
                        int char_width = char_display_width(c, col);
                        if (col + char_width > cols) {
                                printf("\x1b[K\r\n");
                                line_row_offset++;
                                current_screen_row++;
                                col = 0;
                                if (current_screen_row >= rows) {
                                        break;
                                }
                        }
                        if (is_cursor_line && j == editor.cursor_x
                            && !cursor_found) {
                                cursor_screen_row = current_screen_row;
                                cursor_screen_col = col;
                                cursor_found = 1;
                        }
                        if (c == '\t') {
                                int tab_width = TAB_SIZE - (col % TAB_SIZE);
                                for (int k = 0; k < tab_width; k++) {
                                        printf(" ");
                                }
                                col += tab_width;
                        } else {
                                printf("%c", c);
                                col++;
                        }
                }
                if (is_cursor_line && editor.cursor_x >= len && !cursor_found) {
                        cursor_screen_row = current_screen_row;
                        cursor_screen_col = col;
                }
                printf("\x1b[K\r\n");
                current_screen_row++;
        }
        while (current_screen_row < rows) {
                printf("~\x1b[K\r\n");
                current_screen_row++;
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
        if (cursor_screen_row >= 0) {
                printf("\x1b[%d;%dH", cursor_screen_row + 2, cursor_screen_col + 1);
        }
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
