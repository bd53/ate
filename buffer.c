#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

void free_rows(void)
{
        for (int j = 0; j < Editor.buffer_rows; j++) {
                free(Editor.row[j].chars);
                free(Editor.row[j].state);
        }
        free(Editor.row);
        Editor.row = NULL;
        Editor.buffer_rows = 0;
        if (Editor.filename) {
                free(Editor.filename);
                Editor.filename = NULL;
        }
}

void resize_row(struct Row *row, int new_size)
{
        if (!row)
                return;
        char *new_chars = realloc(row->chars, new_size + 1);
        if (!new_chars)
                die("realloc");
        row->chars = new_chars;
        row->chars[new_size] = '\0';
        int hl_size = new_size > 0 ? new_size : 1;
        unsigned char *new_state = realloc(row->state, hl_size);
        if (!new_state)
                die("realloc");
        row->state = new_state;
        if (new_size > row->size)
                memset(&row->state[row->size], 0, new_size - row->size);
        row->size = new_size;
}

static void insert_row(int at, char *s, size_t len)
{
        if (at < 0 || at > Editor.buffer_rows)
                return;
        Editor.row = realloc(Editor.row, sizeof(struct Row) * (Editor.buffer_rows + 1));
        if (!Editor.row)
                die("realloc");
        memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(struct Row) * (Editor.buffer_rows - at));
        Editor.row[at].size = len;
        Editor.row[at].chars = malloc(len + 1);
        if (!Editor.row[at].chars)
                die("malloc");
        memcpy(Editor.row[at].chars, s, len);
        Editor.row[at].chars[len] = '\0';
        int hl_size = len > 0 ? len : 1;
        Editor.row[at].state = malloc(hl_size);
        if (!Editor.row[at].state) {
                free(Editor.row[at].chars);
                die("malloc");
        }
        memset(Editor.row[at].state, 0, hl_size);
        Editor.buffer_rows++;
}

void append_row(char *s, size_t len)
{
        if (len > 0 && s[len - 1] == '\n')
                len--;
        insert_row(Editor.buffer_rows, s, len);
}

void delete_row(int at)
{
        if (at < 0 || at >= Editor.buffer_rows)
                return;
        free(Editor.row[at].chars);
        free(Editor.row[at].state);
        memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(struct Row) * (Editor.buffer_rows - at - 1));
        if (--Editor.buffer_rows == 0) {
                free(Editor.row);
                Editor.row = NULL;
                Editor.cursor_y = 0;
        } else {
                Editor.row = realloc(Editor.row, sizeof(struct Row) * Editor.buffer_rows);
                if (Editor.cursor_y >= Editor.buffer_rows)
                        Editor.cursor_y = Editor.buffer_rows - 1;
        }
        if (Editor.cursor_y < 0)
                Editor.cursor_y = 0;
        Editor.modified = 1;
}

void insert_character(char c)
{
        if (Editor.cursor_y == Editor.buffer_rows)
                append_row("", 0);
        if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows)
                return;
        if (!Editor.row)
                return;
        struct Row *row = &Editor.row[Editor.cursor_y];
        if (Editor.cursor_x < 0)
                Editor.cursor_x = 0;
        if (Editor.cursor_x > row->size)
                Editor.cursor_x = row->size;
        int old_size = row->size;
        char *new_chars = realloc(row->chars, old_size + 2);
        if (!new_chars)
                die("realloc");
        row->chars = new_chars;
        int hl_size = (old_size + 1) > 0 ? (old_size + 1) : 1;
        unsigned char *new_state = realloc(row->state, hl_size);
        if (!new_state)
                die("realloc");
        row->state = new_state;
        if (Editor.cursor_x < old_size) {
                memmove(&row->chars[Editor.cursor_x + 1], &row->chars[Editor.cursor_x], old_size - Editor.cursor_x);
                memmove(&row->state[Editor.cursor_x + 1], &row->state[Editor.cursor_x], old_size - Editor.cursor_x);
        }
        row->chars[Editor.cursor_x] = c;
        row->state[Editor.cursor_x] = 0;
        row->size = old_size + 1;
        row->chars[row->size] = '\0';
        Editor.cursor_x++;
        Editor.modified = 1;
}

void insert_newline(void)
{
        int indent = 0, extra = 0, dedent = 0;
        if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                int split = Editor.cursor_x;
                if (split < 0)
                        split = 0;
                if (split > Editor.row[Editor.cursor_y].size)
                        split = Editor.row[Editor.cursor_y].size;
                for (int i = 0; i < Editor.row[Editor.cursor_y].size; i++) {
                        if (Editor.row[Editor.cursor_y].chars[i] == ' ' || Editor.row[Editor.cursor_y].chars[i] == '\t') {
                                indent++;
                        } else {
                                break;
                        }
                }
                for (int i = Editor.row[Editor.cursor_y].size - 1; i >= 0; i--) {
                        if (Editor.row[Editor.cursor_y].chars[i] != ' ' && Editor.row[Editor.cursor_y].chars[i] != '\t') {
                                extra = (Editor.row[Editor.cursor_y].chars[i] == '{' || Editor.row[Editor.cursor_y].chars[i] == '(' || Editor.row[Editor.cursor_y].chars[i] == '[');
                                break;
                        }
                }
                char *split_content = NULL;
                int split_len = 0;
                if (split < Editor.row[Editor.cursor_y].size) {
                        split_len = Editor.row[Editor.cursor_y].size - split;
                        split_content = malloc(split_len + 1);
                        if (!split_content)
                                die("malloc");
                        memcpy(split_content, &Editor.row[Editor.cursor_y].chars[split], split_len);
                        split_content[split_len] = '\0';
                        for (int i = 0; i < split_len; i++) {
                                if (split_content[i] != ' ' && split_content[i] != '\t') {
                                        dedent = (split_content[i] == '}' || split_content[i] == ')' || split_content[i] == ']');
                                        break;
                                }
                        }
                }
                resize_row(&Editor.row[Editor.cursor_y], split);
                if (split_content) {
                        insert_row(Editor.cursor_y + 1, split_content, split_len);
                        free(split_content);
                } else {
                        insert_row(Editor.cursor_y + 1, "", 0);
                }
        } else {
                append_row("", 0);
        }
        Editor.cursor_y++;
        Editor.cursor_x = 0;
        Editor.modified = 1;
        int final_indent = indent + (extra && !dedent ? TAB_SIZE : 0) - (dedent && indent >= TAB_SIZE ? TAB_SIZE : 0);
        for (int i = 0; i < final_indent; i++)
                insert_character(' ');
}

void delete_character(void)
{
        if (Editor.cursor_x == 0 && Editor.cursor_y == 0)
                return;
        if (Editor.cursor_y >= Editor.buffer_rows)
                return;
        if (!Editor.row)
                return;
        if (Editor.cursor_x == 0) {
                if (Editor.cursor_y == 0)
                        return;
                struct Row *prev_row = &Editor.row[Editor.cursor_y - 1];
                struct Row *current_row = &Editor.row[Editor.cursor_y];
                int merge_at = prev_row->size;
                char *new_chars = realloc(prev_row->chars, prev_row->size + current_row->size + 1);
                if (!new_chars)
                        die("realloc");
                prev_row->chars = new_chars;
                unsigned char *new_state = realloc(prev_row->state, prev_row->size + current_row->size);
                if (!new_state)
                        die("realloc");
                prev_row->state = new_state;
                memcpy(&prev_row->chars[merge_at], current_row->chars, current_row->size);
                memcpy(&prev_row->state[merge_at], current_row->state, current_row->size);
                prev_row->size = merge_at + current_row->size;
                prev_row->chars[prev_row->size] = '\0';
                Editor.cursor_y--;
                Editor.cursor_x = merge_at;
                delete_row(Editor.cursor_y + 1);
        } else {
                struct Row *row = &Editor.row[Editor.cursor_y];
                if (Editor.cursor_x > row->size)
                        Editor.cursor_x = row->size;
                if (Editor.cursor_x == 0)
                        return;
                int bytes_to_move = row->size - Editor.cursor_x;
                if (bytes_to_move > 0) {
                        memmove(&row->chars[Editor.cursor_x - 1], &row->chars[Editor.cursor_x], bytes_to_move);
                        memmove(&row->state[Editor.cursor_x - 1], &row->state[Editor.cursor_x], bytes_to_move);
                }
                row->size--;
                row->chars[row->size] = '\0';
                char *new_chars = realloc(row->chars, row->size + 1);
                if (new_chars)
                        row->chars = new_chars;
                int hl_size = row->size > 0 ? row->size : 1;
                unsigned char *new_state = realloc(row->state, hl_size);
                if (new_state)
                        row->state = new_state;
                Editor.cursor_x--;
                Editor.modified = 1;
        }
}

static void render_line(struct Buffer *ab, int filerow, int wrap_line, int content_width)
{
        struct Row *row = &Editor.row[filerow];
        char buf[32];
        if (!Editor.file_tree) {
                int len = snprintf(buf, sizeof(buf), "\x1b[38;5;244m%s%*d \x1b[m", filerow == Editor.cursor_y ? "\x1b[32m" : "", Editor.gutter_width - 1, wrap_line == 0 ? filerow + 1 : 0);
                if (wrap_line > 0)
                        len = snprintf(buf, sizeof(buf), "\x1b[38;5;244m%*s \x1b[m", Editor.gutter_width - 1, "");
                append(ab, buf, len);
        }
        int start = wrap_line * content_width;
        int end = start + content_width;
        if (end > row->size)
                end = row->size;
        if (Editor.file_tree && (filerow < 3 || filerow == Editor.cursor_y))
                append(ab, filerow < 3 ? "\x1b[34m" : "\x1b[7m", filerow < 3 ? 5 : 4);
        int current_hl = 0;
        for (int i = start; i < end; i++) {
                int hl = row->state[i];
                if (!Editor.file_tree && Editor.query && filerow == Editor.found_row) {
                        int match_col = Editor.found_col;
                        if (i >= match_col && i < match_col + (int) strlen(Editor.query))
                                hl = 1;
                }
                if (hl != current_hl) {
                        current_hl = hl;
                        append(ab, hl == 1 ? "\x1b[45m" : "\x1b[0m",
                               hl == 1 ? 5 : 4);
                }
                append(ab, &row->chars[i], 1);
        }
        append(ab, "\x1b[m\x1b[K\r\n", 8);
}

void draw_content(struct Buffer *ab)
{
        if (!ab)
                return;
        if (Editor.buffer_rows > 0 && !Editor.row)
                return;
        Editor.gutter_width = calculate();
        int content_width = Editor.editor_cols - Editor.gutter_width;
        if (content_width <= 0)
                return;
        int screen_row = 0;
        for (int filerow = Editor.row_offset;
             filerow < Editor.buffer_rows && screen_row < Editor.editor_rows;
             filerow++) {
                if (!Editor.row)
                        break;
                int wrapped = (Editor.row[filerow].size + content_width - 1) / content_width;
                if (wrapped == 0)
                        wrapped = 1;
                for (int wrap = 0;
                     wrap < wrapped && screen_row < Editor.editor_rows;
                     wrap++, screen_row++)
                        render_line(ab, filerow, wrap, content_width);
        }
        while (screen_row < Editor.editor_rows) {
                if (!Editor.file_tree) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "\x1b[38;5;244m%*s\x1b[m", Editor.gutter_width, "~");
                        append(ab, buf, strlen(buf));
                }
                append(ab, "\x1b[K\r\n", 5);
                screen_row++;
        }
}
