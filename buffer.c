#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edef.h"
#include "estruct.h"

void insert_char(char c)
{
        char *line = editor.lines[editor.cursor_y];
        int len = strlen(line);
        line = realloc(line, len + 2);
        memmove(&line[editor.cursor_x + 1], &line[editor.cursor_x], len - editor.cursor_x + 1);
        line[editor.cursor_x] = c;
        editor.lines[editor.cursor_y] = line;
        editor.cursor_x++;
        editor.modified = 1;
}

void insert_newline()
{
        char *line = editor.lines[editor.cursor_y];
        char *new_line = strdup(&line[editor.cursor_x]);
        line[editor.cursor_x] = '\0';
        editor.lines = realloc(editor.lines, sizeof(char *) * (editor.line_numbers + 1));
        memmove(&editor.lines[editor.cursor_y + 2], &editor.lines[editor.cursor_y + 1], sizeof(char *) * (editor.line_numbers - editor.cursor_y - 1));
        editor.lines[editor.cursor_y + 1] = new_line;
        editor.line_numbers++;
        editor.cursor_y++;
        editor.cursor_x = 0;
        editor.modified = 1;
}

void delete_char()
{
        if (editor.cursor_x == 0 && editor.cursor_y == 0) {
                return;
        }
        if (editor.cursor_x > 0) {
                char *line = editor.lines[editor.cursor_y];
                memmove(&line[editor.cursor_x - 1], &line[editor.cursor_x], strlen(line) - editor.cursor_x + 1);
                editor.cursor_x--;
        } else {
                int prev_len = strlen(editor.lines[editor.cursor_y - 1]);
                editor.lines[editor.cursor_y - 1] = realloc(editor.lines[editor.cursor_y - 1], prev_len + strlen(editor.lines[editor.cursor_y]) + 1);
                strcat(editor.lines[editor.cursor_y - 1], editor.lines[editor.cursor_y]);
                free(editor.lines[editor.cursor_y]);
                memmove(&editor.lines[editor.cursor_y], &editor.lines[editor.cursor_y + 1], sizeof(char *) * (editor.line_numbers - editor.cursor_y - 1));
                editor.line_numbers--;
                editor.cursor_y--;
                editor.cursor_x = prev_len;
        }
        editor.modified = 1;
}

void indent_line()
{
        char *line = editor.lines[editor.cursor_y];
        int len = strlen(line);
        line = realloc(line, len + TAB_SIZE + 1);
        memmove(&line[editor.cursor_x + TAB_SIZE], &line[editor.cursor_x], len - editor.cursor_x + 1);
        memset(&line[editor.cursor_x], ' ', TAB_SIZE);
        editor.lines[editor.cursor_y] = line;
        editor.cursor_x += TAB_SIZE;
        editor.modified = 1;
}
