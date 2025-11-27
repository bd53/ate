#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "efunc.h"
#include "estruct.h"
#include "util.h"
#include "version.h"

int char_display_width(char c, int col_pos)
{
        if (c == '\t') {
                return TAB_SIZE - (col_pos % TAB_SIZE);
        }
        return 1;
}

void buffer_to_screen_pos(int line_idx, int cursor_x, int screen_width, int *screen_line, int *screen_col)
{
        if (line_idx < 0 || line_idx >= editor.line_numbers) {
                *screen_line = 0;
                *screen_col = 0;
                return;
        }
        char *line = editor.lines[line_idx];
        int len = strlen(line);
        int col = 0;
        int wrapped_lines = 0;
        for (int i = 0; i < cursor_x && i < len; i++) {
                int char_width = char_display_width(line[i], col);
                if (col + char_width > screen_width) {
                        wrapped_lines++;
                        col = 0;
                }
                col += char_width;
        }
        *screen_line = wrapped_lines;
        *screen_col = col;
}

int get_wrapped_line_count(int line_idx, int screen_width)
{
        if (line_idx < 0 || line_idx >= editor.line_numbers) {
                return 1;
        }
        char *line = editor.lines[line_idx];
        int len = strlen(line);
        if (len == 0) {
                return 1;
        }
        int col = 0;
        int wrapped_lines = 1;
        for (int i = 0; i < len; i++) {
                int char_width = char_display_width(line[i], col);
                if (col + char_width > screen_width) {
                        wrapped_lines++;
                        col = 0;
                }
                col += char_width;
        }
        return wrapped_lines;
}
