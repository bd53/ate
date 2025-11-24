#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "ebind.h"
#include "efunc.h"
#include "utf8.h"
#include "util.h"

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
    struct Row *new_rows = realloc(Editor.row, sizeof(struct Row) * (Editor.buffer_rows + 1));
    if (!new_rows) die("realloc");
    Editor.row = new_rows;
    memmove(&Editor.row[at + 1], &Editor.row[at], sizeof(struct Row) * (Editor.buffer_rows - at));
    Editor.row[at].size = len;
    Editor.row[at].chars = malloc(len + 1);
    if (!Editor.row[at].chars) die("malloc");
    memcpy(Editor.row[at].chars, s, len);
    Editor.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    Editor.row[at].state = malloc(hl_size);
    if (!Editor.row[at].state) {
        free(Editor.row[at].chars);
        memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(struct Row) * (Editor.buffer_rows - at));
        die("malloc");
    }
    memset(Editor.row[at].state, 0, hl_size);
    Editor.buffer_rows++;
}

void append_row(char *s, size_t len) {
    if (len > 0 && s[len - 1] == '\n') len--;
    struct Row *new_rows = realloc(Editor.row, sizeof(struct Row) * (Editor.buffer_rows + 1));
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
    struct Row *row = &Editor.row[at];
    free(row->chars);
    free(row->state);
    memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(struct Row) * (Editor.buffer_rows - at - 1));
    Editor.buffer_rows--;
    if (Editor.buffer_rows == 0) {
        free(Editor.row);
        Editor.row = NULL;
        Editor.cursor_y = 0;
    } else {
        struct Row *new_rows = realloc(Editor.row, sizeof(struct Row) * Editor.buffer_rows);
        if (new_rows) Editor.row = new_rows;
        if (Editor.cursor_y >= Editor.buffer_rows) Editor.cursor_y = Editor.buffer_rows - 1;
    }
    if (Editor.cursor_y < 0) Editor.cursor_y = 0;
    Editor.modified = 1;
}

void insert_character(char c) {
    if (Editor.cursor_y == Editor.buffer_rows) append_row("", 0);
    struct Row *row = &Editor.row[Editor.cursor_y];
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
    if (!utf8_is_char_boundary(row->chars, Editor.cursor_x)) Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
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
    if (Editor.cursor_x < row->size - 1) memmove(&row->state[Editor.cursor_x + 1], &row->state[Editor.cursor_x], row->size - Editor.cursor_x - 1);
    row->state[Editor.cursor_x] = 0;
    Editor.cursor_x++;
    Editor.modified = 1;
}

static int opening_bracket(const char *line, int len) {
    int i = len - 1;
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t')) {
        i--;
    }
    if (i >= 0) {
        char last_char = line[i];
        return (last_char == '{' || last_char == '(' || last_char == '[');
    }
    return 0;
}

static int closing_bracket(const char *line, int len) {
    for (int i = 0; i < len; i++) {
        if (line[i] == ' ' || line[i] == '\t') {
            continue;
        }
        return (line[i] == '}' || line[i] == ')' || line[i] == ']');
    }
    return 0;
}

void insert_new_line() {
    int indent_spaces = 0;
    int add_extra_indent = 0;
    char *second_half = NULL;
    int second_half_len = 0;
    struct Row *current_row = NULL;
    if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        current_row = &Editor.row[Editor.cursor_y];
        indent_spaces = leading_whitespace(current_row->chars, current_row->size);
        add_extra_indent = opening_bracket(current_row->chars, current_row->size);
    }
    if (Editor.cursor_y == Editor.buffer_rows) {
        append_row("", 0);
    } else {
        struct Row *row = &Editor.row[Editor.cursor_y];
        int split_at = Editor.cursor_x;
        if (split_at < 0) split_at = 0;
        if (split_at > row->size) split_at = row->size;
        if (!utf8_is_char_boundary(row->chars, split_at)) {
            split_at = utf8_prev_char_boundary(row->chars, split_at);
        }
        second_half_len = row->size - split_at;
        second_half = malloc(second_half_len + 1);
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
        insert_row(Editor.cursor_y + 1, second_half, second_half_len);
    }
    Editor.cursor_y++;
    Editor.cursor_x = 0;
    Editor.modified = 1;
    if (indent_spaces > 0 || add_extra_indent) {
        int dedent = 0;
        if (second_half && second_half_len > 0) {
            dedent = closing_bracket(second_half, second_half_len);
        }
        int final_indent = indent_spaces;
        if (add_extra_indent && !dedent) {
            final_indent += INDENT_SIZE;
        } else if (dedent && final_indent >= INDENT_SIZE) {
            final_indent -= INDENT_SIZE;
        }
        for (int i = 0; i < final_indent; i++) {
            insert_character(' ');
        }
    }
    if (second_half) {
        free(second_half);
    }
}

void auto_dedent() {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
    int only_whitespace = 1;
    for (int i = 0; i < Editor.cursor_x - 1; i++) {
        if (row->chars[i] != ' ' && row->chars[i] != '\t') {
            only_whitespace = 0;
            break;
        }
    }
    if (only_whitespace) {
        int leading_spaces = leading_whitespace(row->chars, row->size);
        if (leading_spaces >= INDENT_SIZE) {
            trim_leadingspace(INDENT_SIZE);
        }
    }
}

void delete_character() {
    if (Editor.cursor_x == 0 && Editor.cursor_y == 0) return;
    if (Editor.cursor_y >= Editor.buffer_rows) return;
    if (Editor.cursor_x == 0) {
        if (Editor.cursor_y == 0) return;
        Editor.cursor_y--;
        struct Row *row = &Editor.row[Editor.cursor_y];
        struct Row *next_row = &Editor.row[Editor.cursor_y + 1];
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
        delete_row(Editor.cursor_y + 1);
        Editor.modified = 1;
    } else {
        struct Row *row = &Editor.row[Editor.cursor_y];
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

void yank_line() {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
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
    display_message(1, "Yanked");
}

void delete_line() {
    if (Editor.buffer_rows > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        delete_row(Editor.cursor_y);
        if (Editor.buffer_rows == 0) {
            Editor.cursor_y = 0;
        } else if (Editor.cursor_y >= Editor.buffer_rows) {
            Editor.cursor_y = Editor.buffer_rows - 1;
        }
        if (Editor.cursor_y < 0) Editor.cursor_y = 0;
        Editor.cursor_x = 0;
        Editor.modified = 1;
        display_message(2, "Deleted");
    }
}

void goto_line() {
    char *command = prompt("Go to line: ");
    if (command == NULL) {
        Editor.mode = 0;
        refresh_screen();
        return;
    }
    char *endptr;
    long line_number = strtol(command, &endptr, 10);
    if (*endptr == '\0' && line_number > 0 && line_number <= Editor.buffer_rows) {
        int target_row = (int)line_number - 1;
        Editor.cursor_y = target_row;
        Editor.cursor_x = 0;
        if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows)  Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        refresh_screen();
        display_message(2, "Jumped");
    } else {
        display_message(1, "Invalid");
    }
    free(command);
    Editor.mode = 0;
}

void draw_content(struct Buffer *ab) {
    if (!ab) return;
    if (!Editor.row && Editor.buffer_rows > 0) return;
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
        struct Row *row = &Editor.row[filerow];
        int wrapped_lines = (row->size + content_width - 1) / content_width;
        if (wrapped_lines == 0) wrapped_lines = 1;
        for (int wrap_line = 0; wrap_line < wrapped_lines && screen_row < file_content_rows; wrap_line++, screen_row++) {
            char line_num_buf[32];
            int line_num_len;
            if (!Editor.file_tree) {
                append(ab, "\x1b[38;5;244m", 11);
                if (wrap_line == 0) {
                    int abs_num = filerow + 1;
                    if (filerow == Editor.cursor_y) append(ab, "\x1b[32m", 5);
                    line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*d ", Editor.gutter_width - 1, abs_num);
                } else {
                    line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s ", Editor.gutter_width - 1, "");
                }
                append(ab, line_num_buf, line_num_len);
                append(ab, "\x1b[m", 3);
            }
            int start_idx = wrap_line * content_width;
            int end_idx = start_idx + content_width;
            if (end_idx > row->size) end_idx = row->size;
            if (Editor.file_tree) {
                if (filerow < 3) {
                    append(ab, "\x1b[34m", 5);
                } else if (filerow == Editor.cursor_y) {
                    append(ab, "\x1b[7m", 4);
                }
            }
            int current_hl = 0;
            for (int i = start_idx; i < end_idx; i++) {
                char c = row->chars[i];
                if (!Editor.file_tree) {
                    int hl = row->state[i];
                    int match_col = Editor.found_col;
                    int query_len = (Editor.query != NULL) ? strlen(Editor.query) : 0;
                    if (Editor.query && filerow == Editor.found_row && i >= match_col && i < match_col + query_len) hl = 1;
                    if (hl != current_hl) {
                        current_hl = hl;
                        char *color_code = (hl == 1) ? "\x1b[45m" : "\x1b[0m";
                        append(ab, color_code, strlen(color_code));
                    }
                }
                append(ab, &c, 1);
            }
            append(ab, "\x1b[m", 3);
            append(ab, "\x1b[K", 3);
            append(ab, "\r\n", 2);
        }
    }
    while (screen_row < file_content_rows) {
        if (!Editor.file_tree) {
            append(ab, "\x1b[38;5;244m", 11);
            char line_num_buf[32];
            int line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s", Editor.gutter_width, "~");
            append(ab, line_num_buf, line_num_len);
            append(ab, "\x1b[m", 3);
        }
        append(ab, "\x1b[K", 3);
        append(ab, "\r\n", 2);
        screen_row++;
    }
}

void execute_command(char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    cmd = trim_whitespace(cmd);
    if (cmd == NULL || strlen(cmd) == 0) return;
    if (strcmp(cmd, "q") == 0) {
        run_cleanup();
        Editor.file_tree = 0;
        Editor.help_view = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
        Editor.row_offset = 0;
        Editor.modified = 0;
        refresh_screen();
        return;
    }
    else if (strcmp(cmd, "quit") == 0) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        run_cleanup();
        exit(0);
    }
    else if (strcmp(cmd, "Ex") == 0) {
        toggle_file_tree();
    }
    else if (strcmp(cmd, "bd") == 0) {
        if (Editor.file_tree) toggle_file_tree();
    }
    else if (strncmp(cmd, "w", 1) == 0) {
        char *arg = NULL;
        if (strcmp(cmd, "w") == 0 || strcmp(cmd, "write") == 0) {
            arg = NULL;
        } else {
            if (strncmp(cmd, "w ", 2) == 0) arg = cmd + 2;
            else if (strncmp(cmd, "write ", 6) == 0) arg = cmd + 6;
        }
        if (arg) {
            arg = trim_whitespace(arg);
            if (arg && *arg) {
                char *new_filename = strdup(arg);
                if (!new_filename) die("strdup");
                if (Editor.filename) {
                    free(Editor.filename);
                    Editor.filename = NULL;
                }
                Editor.filename = new_filename;
            }
        }
        if (Editor.filename == NULL) {
            char *filename = prompt("Save as: %s (ESC to cancel)");
            if (!filename) {
                refresh_screen();
                return;
            }
            Editor.filename = filename;
        }
        save_file();
    }
    else if (strcmp(cmd, "help") == 0) {
        display_help();
    }
    else if (strcmp(cmd, "checkhealth") == 0) {
        check_health();
    }
    else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "E182: Not an editor command: '%s'", cmd);
        display_message(2, error_msg);
        input_read_key();
        refresh_screen();
    }
}

void command_mode() {
    size_t bufsize = 256;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    size_t buflen = 0;
    buf[0] = '\0';
    int prompt_row = Editor.editor_rows + 2;
    while(1) {
        if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        struct Buffer ab = BUFFER_INIT;
        append(&ab, "\x1b[?25l", 6);
        append(&ab, "\x1b[H", 3);
        draw_content(&ab);
        display_status(&ab);
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        append(&ab, pos_buf, strlen(pos_buf));
        append(&ab, "\x1b[K", 3);
        append(&ab, ":", 1);
        append(&ab, buf, buflen);
        int cursor_col = buflen + 2;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", prompt_row, cursor_col);
        append(&ab, pos_buf, strlen(pos_buf));
        append(&ab, "\x1b[?25h", 6);
        if (write(STDOUT_FILENO, ab.b, ab.length) == -1) {
            free(ab.b);
            free(buf);
            die("write");
        }
        free(ab.b);
        int c = input_read_key();
        if (c == '\r') {
            Editor.mode = 0;
            if (buflen > 0) execute_command(buf);
            free(buf);
            refresh_screen();
            return;
        } else if (c == '\x1b') {
            Editor.mode = 0;
            free(buf);
            refresh_screen();
            return;
        } else if (c == 127 || c == CTRL_KEY('h')) {
            if (buflen > 0) {
                buflen--;
                buf[buflen] = '\0';
            }
        } else if (c >= 32 && c < 127) {
            if (buflen < bufsize - 1) {
                buf[buflen++] = c;
                buf[buflen] = '\0';
            } else {
                bufsize *= 2;
                char *new_buf = realloc(buf, bufsize);
                if (new_buf == NULL) {
                    free(buf);
                    die("realloc");
                }
                buf = new_buf;
                buf[buflen++] = c;
                buf[buflen] = '\0';
            }
        }
    }
}

void refresh_screen() {
    if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
    if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
    struct Buffer ab = BUFFER_INIT;
    append(&ab, "\x1b[?25l", 6);
    append(&ab, "\x1b[H", 3);
    draw_content(&ab);
    display_status(&ab);
    int content_width = Editor.editor_cols - Editor.gutter_width;
    int screen_row = 0;
    for (int filerow = Editor.row_offset; filerow < Editor.cursor_y && filerow < Editor.buffer_rows; filerow++) {
        struct Row *row = &Editor.row[filerow];
        int wrapped_lines = (row->size + content_width - 1) / content_width;
        if (wrapped_lines == 0) wrapped_lines = 1;
        screen_row += wrapped_lines;
    }
    if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        int wrap_offset = Editor.cursor_x / content_width;
        screen_row += wrap_offset;
    }
    int cur_x = (Editor.cursor_x % content_width) + Editor.gutter_width + 1;
    int cur_y = screen_row + 1;
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cur_y, cur_x);
    append(&ab, buf, strlen(buf));
    if (Editor.mode == 1) {
        append(&ab, "\x1b[5 q", 5);
    } else {
        append(&ab, "\x1b[2 q", 5);
    }
    append(&ab, "\x1b[?25h", 6);
    if (write(STDOUT_FILENO, ab.b, ab.length) == -1) die("write");
    free(ab.b);
}
