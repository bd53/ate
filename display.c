#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "edef.h"
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

void refresh()
{
        printf("\x1b[2J");
        printf("\x1b[H");
        int rows = get_window_rows() - 2;
        int total_cols = get_window_cols();
        int cols = total_cols - LINE_NUMBER_WIDTH;
        printf("\x1b[7m");
        char top_status[256];
        snprintf(top_status, sizeof(top_status), " %s %s", PROGRAM_NAME_LONG, VERSION);
        printf("%s", top_status);
        int top_status_len = strlen(top_status);
        for (int i = top_status_len; i < total_cols; i++) {
                printf(" ");
        }
        printf("\x1b[m\r\n");
        int cursor_screen_row = -1;
        int cursor_screen_col = 0;
        int current_screen_row = 0;
        for (int i = editor.offset_y;
             i < editor.line_numbers && current_screen_row < rows; i++) {
                char line_num_str[LINE_NUMBER_WIDTH + 1];
                snprintf(line_num_str, sizeof(line_num_str), "%*d ", LINE_NUMBER_WIDTH - 1, i + 1);
                printf("\x1b[38;5;242m");
                printf("%s", line_num_str);
                printf("\x1b[m");
                if (i == editor.cursor_y) {
                    cursor_screen_col = LINE_NUMBER_WIDTH;
                }
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
                                printf("\x1b[38;5;242m");
                                printf("%*s", LINE_NUMBER_WIDTH, "");
                                printf("\x1b[m");
                        }
                        if (is_cursor_line && j == editor.cursor_x && !cursor_found) {
                                cursor_screen_row = current_screen_row;
                                cursor_screen_col = col + LINE_NUMBER_WIDTH;
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
                        cursor_screen_col = col + LINE_NUMBER_WIDTH;
                }
                printf("\x1b[K\r\n");
                current_screen_row++;
        }
        while (current_screen_row < rows) {
                printf("\x1b[38;5;242m");
                printf("~");
                printf("\x1b[m");
                printf("\x1b[K\r\n");
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
        for (int i = bottom_status_len; i < total_cols; i++) {
                printf(" ");
        }
        printf("\x1b[m");
        if (cursor_screen_row >= 0) {
                printf("\x1b[%d;%dH", cursor_screen_row + 2, cursor_screen_col + 1);
        }
        fflush(stdout);
}
