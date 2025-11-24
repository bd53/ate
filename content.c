#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "search.h"
#include "tree.h"
#include "utils.h"

int utf8_char_len(const char *s, int max_len) {
    if (s == NULL || max_len <= 0) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return 1;
    if (max_len < 2) return 0;
    if ((c & 0xE0) == 0xC0) return 2;
    if (max_len < 3) return 0;
    if ((c & 0xF0) == 0xE0) return 3;
    if (max_len < 4) return 0;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0;
}

int utf8_decode(const char *s, int max_len, int *codepoint) {
    if (s == NULL || max_len <= 0) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        if (max_len < 2 || (s[1] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        if (*codepoint < 0x80) return -1;
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (max_len < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (*codepoint < 0x800 || (*codepoint >= 0xD800 && *codepoint <= 0xDFFF)) return -1;
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (max_len < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (*codepoint < 0x10000 || *codepoint > 0x10FFFF) return -1;
        return 4;
    }
    return -1;
}

int utf8_is_valid(int codepoint) {
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        return 0;
    }
    if (codepoint >= 0x00 && codepoint <= 0x10FFFF) {
        return 1;
    }
    return 0;
}

int utf8_is_char_boundary(const char *s, int byte_offset) {
    if (s == NULL || byte_offset < 0) return 1;
    if (s[byte_offset] == '\0') return 1;
    unsigned char c = (unsigned char)s[byte_offset];
    return (c & 0xC0) != 0x80;
}

int utf8_prev_char_boundary(const char *s, int byte_offset) {
    if (byte_offset <= 0) return 0;
    byte_offset--;
    while (byte_offset > 0 && !utf8_is_char_boundary(s, byte_offset)) {
        byte_offset--;
    }
    return byte_offset;
}

int utf8_next_char_boundary(const char *s, int byte_offset, int max_len) {
    if (byte_offset >= max_len) return max_len;
    int remaining_len = max_len - byte_offset;
    int len = utf8_char_len(&s[byte_offset], remaining_len);
    if (len == 0) {
        if (remaining_len > 0) return byte_offset + 1;
        return byte_offset;
    }
    byte_offset += len;
    if (byte_offset > max_len) return max_len;
    return byte_offset;
}

void free_rows() {
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

void insert_row(int at, char *s, size_t len) {
    if (at < 0 || at > Editor.buffer_rows) return;
    Row *new_rows = realloc(Editor.row, sizeof(Row) * (Editor.buffer_rows + 1));
    if (!new_rows) die("realloc");
    Editor.row = new_rows;
    memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(Row) * (Editor.buffer_rows - at));
    Editor.row[at].size = len;
    Editor.row[at].chars = malloc(len + 1);
    if (!Editor.row[at].chars) die("malloc");
    memcpy(Editor.row[at].chars, s, len);
    Editor.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    Editor.row[at].state = malloc(hl_size);
    if (!Editor.row[at].state) {
        free(Editor.row[at].chars);
        memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(Row) * (Editor.buffer_rows - at));
        die("malloc");
    }
    memset(Editor.row[at].state, 0, hl_size);
    Editor.buffer_rows++;
}

void append_row(char *s, size_t len) {
    if (len > 0 && s[len - 1] == '\n') len--;
    Row *new_rows = realloc(Editor.row, sizeof(Row) * (Editor.buffer_rows + 1));
    if (new_rows == NULL) die("realloc");
    Editor.row = new_rows;
    int at = Editor.buffer_rows;
    Editor.row[at].size = len;
    Editor.row[at].chars = malloc(len + 1);
    if (Editor.row[at].chars == NULL) die("malloc");
    memcpy(Editor.row[at].chars, s, len);
    Editor.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    Editor.row[at].state = malloc(hl_size);
    if (Editor.row[at].state == NULL) {
        free(Editor.row[at].chars);
        die("malloc");
    }
    memset(Editor.row[at].state, 0, hl_size);
    Editor.buffer_rows++;
}

void delete_row(int at) {
    if (at < 0 || at >= Editor.buffer_rows) return;
    Row *row = &Editor.row[at];
    free(row->chars);
    free(row->state);
    memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(Row) * (Editor.buffer_rows - at - 1));
    Editor.buffer_rows--;
    if (Editor.buffer_rows == 0) {
        free(Editor.row);
        Editor.row = NULL;
        Editor.curor_y = 0;
    } else {
        Row *new_rows = realloc(Editor.row, sizeof(Row) * Editor.buffer_rows);
        if (new_rows) {
            Editor.row = new_rows;
        } else {
            die("realloc (delete_row shrink)");
        }
        if (Editor.curor_y >= Editor.buffer_rows) {
            Editor.curor_y = Editor.buffer_rows - 1;
        }
    }
    if (Editor.curor_y < 0) {
        Editor.curor_y = 0;
    }
    Editor.modified = 1;
}

void insert_character(char c) {
    if (Editor.curor_y == Editor.buffer_rows) {
        append_row("", 0);
    }
    Row *row = &Editor.row[Editor.curor_y];
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
    if (!utf8_is_char_boundary(row->chars, Editor.cursor_x)) {
        Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
    }
    char *new_chars = realloc(row->chars, row->size + 2);
    if (!new_chars) die("realloc");
    row->chars = new_chars;
    memmove(&row->chars[Editor.cursor_x + 1], &row->chars[Editor.cursor_x], row->size - Editor.cursor_x + 1);
    row->chars[Editor.cursor_x] = c;
    row->size++;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->state, new_hl_size);
    if (!new_hl) die("realloc");
    row->state = new_hl;
    memmove(&row->state[Editor.cursor_x + 1], &row->state[Editor.cursor_x], row->size - Editor.cursor_x - 1);
    row->state[Editor.cursor_x] = 0;
    Editor.cursor_x++;
    Editor.modified = 1;
}
void insert_new_line() {
    if (Editor.curor_y == Editor.buffer_rows) {
        append_row("", 0);
    } else {
        Row *row = &Editor.row[Editor.curor_y];
        int split_at = Editor.cursor_x;
        if (split_at < 0) split_at = 0;
        if (split_at > row->size) split_at = row->size;
        if (!utf8_is_char_boundary(row->chars, split_at)) {
            split_at = utf8_prev_char_boundary(row->chars, split_at);
        }
        int second_half_len = row->size - split_at;
        char *second_half = malloc(second_half_len + 1);
        if (!second_half) die("malloc");
        memcpy(second_half, &row->chars[split_at], second_half_len);
        second_half[second_half_len] = '\0';
        row->size = split_at;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (!new_chars) {
            free(second_half);
            die("realloc");
        }
        row->chars = new_chars;
        row->chars[row->size] = '\0';
        int hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->state, hl_size);
        if (!new_hl) {
            free(second_half);
            die("realloc");
        }
        row->state = new_hl;
        insert_row(Editor.curor_y + 1, second_half, second_half_len);
        free(second_half);
    }
    Editor.curor_y++;
    Editor.cursor_x = 0;
    Editor.modified = 1;
}

void delete_character() {
    if (Editor.cursor_x == 0 && Editor.curor_y == 0) return;
    if (Editor.curor_y >= Editor.buffer_rows) return;
    if (Editor.cursor_x == 0) {
        if (Editor.curor_y == 0) return;
        Editor.curor_y--;
        Row *row = &Editor.row[Editor.curor_y];
        Row *next_row = &Editor.row[Editor.curor_y + 1];
        Editor.cursor_x = row->size;
        char *new_chars = realloc(row->chars, row->size + next_row->size + 1);
        if (!new_chars) die("realloc");
        row->chars = new_chars;
        memcpy(&row->chars[row->size], next_row->chars, next_row->size);
        row->size += next_row->size;
        row->chars[row->size] = '\0';
        unsigned char *new_hl = realloc(row->state, row->size > 0 ? row->size : 1);
        if (!new_hl) die("realloc");
        row->state = new_hl;
        memcpy(&row->state[Editor.cursor_x], next_row->state, next_row->size);
        delete_row(Editor.curor_y + 1);
        Editor.modified = 1;
    } else {
        Row *row = &Editor.row[Editor.curor_y];
        int prev_pos = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
        int char_len = Editor.cursor_x - prev_pos;
        memmove(&row->chars[prev_pos], &row->chars[Editor.cursor_x], row->size - Editor.cursor_x + 1);
        row->size -= char_len;
        memmove(&row->state[prev_pos], &row->state[Editor.cursor_x], row->size - prev_pos);
        int new_hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->state, new_hl_size);
        if (!new_hl) die("realloc");
        row->state = new_hl;
        Editor.cursor_x = prev_pos;
        Editor.modified = 1;
    }
}

void insert_utf8_character(const char *utf8_char, int char_len) {
    if (Editor.curor_y == Editor.buffer_rows) {
        append_row("", 0);
    }
    Row *row = &Editor.row[Editor.curor_y];
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
    if (!utf8_is_char_boundary(row->chars, Editor.cursor_x)) {
        Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
    }
    int codepoint;
    int decoded_len = utf8_decode(utf8_char, char_len, &codepoint);
    if (decoded_len != char_len || !utf8_is_valid(codepoint)) {
        return;
    }
    char *new_chars = realloc(row->chars, row->size + char_len + 1);
    if (!new_chars) die("realloc");
    row->chars = new_chars;
    memmove(&row->chars[Editor.cursor_x + char_len], &row->chars[Editor.cursor_x], row->size - Editor.cursor_x + 1);
    memcpy(&row->chars[Editor.cursor_x], utf8_char, char_len);
    row->size += char_len;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->state, new_hl_size);
    if (!new_hl) die("realloc");
    row->state = new_hl;
    memmove(&row->state[Editor.cursor_x + char_len], &row->state[Editor.cursor_x], row->size - Editor.cursor_x - char_len);
    for (int i = 0; i < char_len; i++) {
        row->state[Editor.cursor_x + i] = 0;
    }
    Editor.cursor_x += char_len;
    Editor.modified = 1;
}

void yank_line() {
    if (Editor.curor_y < 0 || Editor.curor_y >= Editor.buffer_rows) return;
    Row *row = &Editor.row[Editor.curor_y];
    char header[] = "\x1b]52;c;";
    char trailer[] = "\x07";
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = row->size;
    char *encoded = malloc(((len + 2) / 3) * 4 + 1);
    if (!encoded) return;
    int j = 0;
    for (int i = 0; i < len; i += 3) {
        int n = row->chars[i] << 16;
        if (i + 1 < len) n |= row->chars[i + 1] << 8;
        if (i + 2 < len) n |= row->chars[i + 2];
        encoded[j++] = b64[(n >> 18) & 63];
        encoded[j++] = b64[(n >> 12) & 63];
        encoded[j++] = (i + 1 < len) ? b64[(n >> 6) & 63] : '=';
        encoded[j++] = (i + 2 < len) ? b64[n & 63] : '=';
    }
    encoded[j] = '\0';
    write(STDOUT_FILENO, header, strlen(header));
    write(STDOUT_FILENO, encoded, strlen(encoded));
    write(STDOUT_FILENO, trailer, strlen(trailer));
    free(encoded);
    char msg[512];
    int display_len = (row->size > 50) ? 50 : row->size;
    snprintf(msg, sizeof(msg), "Yanked: '%.*s%s'", display_len, row->chars, (row->size > 50) ? "..." : "");
    display_message(1, msg);
}


void delete_line() {
    if (Editor.buffer_rows > 0 && Editor.curor_y >= 0 && Editor.curor_y < Editor.buffer_rows) {
        Row *row = &Editor.row[Editor.curor_y];
        char saved_line[512];
        int display_len = (row->size > 50) ? 50 : row->size;
        snprintf(saved_line, sizeof(saved_line), "%.*s%s", display_len, row->chars, (row->size > 50) ? "..." : "");
        delete_row(Editor.curor_y);
        if (Editor.buffer_rows == 0) {
            Editor.curor_y = 0;
        } else if (Editor.curor_y >= Editor.buffer_rows) {
            Editor.curor_y = Editor.buffer_rows - 1;
        }
        if (Editor.curor_y < 0) Editor.curor_y = 0;
        Editor.cursor_x = 0;
        Editor.modified = 1;
        char msg[600];
        snprintf(msg, sizeof(msg), "Deleted: '%s'", saved_line);
        display_message(2, msg);
    }
}

void draw_content(buffer *ab) {
    if (!ab) return;
    int file_content_rows = Editor.editor_rows;
    int max_num = Editor.buffer_rows > file_content_rows ? Editor.buffer_rows : file_content_rows;
    Editor.gutter_width = 1;
    if (max_num + 1 > 9) Editor.gutter_width = 2;
    if (max_num + 1 > 99) Editor.gutter_width = 3;
    if (max_num + 1 > 999) Editor.gutter_width = 4;
    Editor.gutter_width += 1;
    if (Editor.file_tree) Editor.gutter_width = 0;
    int content_width = Editor.editor_cols - Editor.gutter_width;
    int screen_row = 0;
    for (int filerow = Editor.row_offset; filerow < Editor.buffer_rows && screen_row < file_content_rows; filerow++) {
        Row *row = &Editor.row[filerow];
        int wrapped_lines = (row->size + content_width - 1) / content_width;
        if (wrapped_lines == 0) wrapped_lines = 1;
        for (int wrap_line = 0; wrap_line < wrapped_lines && screen_row < file_content_rows; wrap_line++, screen_row++) {
            char line_num_buf[32];
            int line_num_len;
            if (!Editor.file_tree) {
                abappend(ab, "\x1b[38;5;244m", 11);
                if (wrap_line == 0) {
                    if (Editor.mode == MODE_NORMAL) {
                        if (filerow == Editor.curor_y) {
                            abappend(ab, "\x1b[33m", 5);
                            line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s ", Editor.gutter_width - 1, ">>");
                        } else {
                            int rel_num = abs(filerow - Editor.curor_y);
                            line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*d ", Editor.gutter_width - 1, rel_num);
                        }
                    } else {
                        int abs_num = filerow + 1;
                        line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*d ", Editor.gutter_width - 1, abs_num);
                    }
                } else {
                    line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s ", Editor.gutter_width - 1, "");
                }
                abappend(ab, line_num_buf, line_num_len);
                abappend(ab, "\x1b[m", 3);
            }
            int start_idx = wrap_line * content_width;
            int end_idx = start_idx + content_width;
            if (end_idx > row->size) end_idx = row->size;
            if (Editor.file_tree) {
                if (filerow < 3) {
                    abappend(ab, "\x1b[34m", 5);
                } else if (filerow == Editor.curor_y) {
                    abappend(ab, "\x1b[7m", 4);
                }
            }
            int current_hl = 0;
            for (int i = start_idx; i < end_idx; i++) {
                char c = row->chars[i];
                if (!Editor.file_tree) {
                    int hl = row->state[i];
                    int match_col = Editor.found_col;
                    int query_len = Editor.query ? strlen(Editor.query) : 0;
                    if (Editor.query && filerow == Editor.found_row && i >= match_col && i < match_col + query_len) {
                        hl = 1;
                    }
                    if (hl != current_hl) {
                        current_hl = hl;
                        char *color_code = (hl == 1) ? "\x1b[45m" : "\x1b[0m";
                        abappend(ab, color_code, strlen(color_code));
                    }
                }
                abappend(ab, &c, 1);
            }
            abappend(ab, "\x1b[m", 3);
            abappend(ab, "\x1b[K", 3);
            abappend(ab, "\r\n", 2);
        }
    }
    while (screen_row < file_content_rows) {
        if (!Editor.file_tree) {
            abappend(ab, "\x1b[38;5;244m", 11);
            char line_num_buf[32];
            int line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s", Editor.gutter_width, "~");
            abappend(ab, line_num_buf, line_num_len);
            abappend(ab, "\x1b[m", 3);
        }
        abappend(ab, "\x1b[K", 3);
        abappend(ab, "\r\n", 2);
        screen_row++;
    }
}
