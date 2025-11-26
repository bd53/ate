#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

void free_rows(void) {
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

static void resize_row(struct Row *row, int new_size) {
    if (!row) return;
    char *new_chars = realloc(row->chars, new_size + 1);
    if (!new_chars) die("realloc");
    row->chars = new_chars;
    row->chars[new_size] = '\0';
    int hl_size = new_size > 0 ? new_size : 1;
    unsigned char *new_state = realloc(row->state, hl_size);
    if (!new_state) die("realloc");
    row->state = new_state;
    if (new_size > row->size) memset(&row->state[row->size], 0, new_size - row->size);
    row->size = new_size;
}

static void insert_row(int at, char *s, size_t len) {
    if (at < 0 || at > Editor.buffer_rows) return;
    Editor.row = realloc(Editor.row, sizeof(struct Row) * (Editor.buffer_rows + 1));
    if (!Editor.row) die("realloc");
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
        die("malloc");
    }
    memset(Editor.row[at].state, 0, hl_size);
    Editor.buffer_rows++;
}

void append_row(char *s, size_t len) {
    if (len > 0 && s[len - 1] == '\n') len--;
    insert_row(Editor.buffer_rows, s, len);
}

static void delete_row(int at) {
    if (at < 0 || at >= Editor.buffer_rows) return;
    free(Editor.row[at].chars);
    free(Editor.row[at].state);
    memmove(&Editor.row[at], &Editor.row[at + 1], sizeof(struct Row) * (Editor.buffer_rows - at - 1));
    if (--Editor.buffer_rows == 0) {
        free(Editor.row);
        Editor.row = NULL;
        Editor.cursor_y = 0;
    } else {
        Editor.row = realloc(Editor.row, sizeof(struct Row) * Editor.buffer_rows);
        if (Editor.cursor_y >= Editor.buffer_rows) Editor.cursor_y = Editor.buffer_rows - 1;
    }
    if (Editor.cursor_y < 0) Editor.cursor_y = 0;
    Editor.modified = 1;
}

void insert_character(char c) {
    if (Editor.cursor_y == Editor.buffer_rows) append_row("", 0);
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    if (!Editor.row) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
    int old_size = row->size;
    char *new_chars = realloc(row->chars, old_size + 2);
    if (!new_chars) die("realloc");
    row->chars = new_chars;
    int hl_size = (old_size + 1) > 0 ? (old_size + 1) : 1;
    unsigned char *new_state = realloc(row->state, hl_size);
    if (!new_state) die("realloc");
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

void insert_new_line(void) {
    int indent = 0, extra = 0, dedent = 0;
    if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        int split = Editor.cursor_x;
        if (split < 0) split = 0;
        if (split > Editor.row[Editor.cursor_y].size) split = Editor.row[Editor.cursor_y].size;
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
            if (!split_content) die("malloc");
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
    int final_indent = indent + (extra && !dedent ? INDENT_SIZE : 0) - (dedent && indent >= INDENT_SIZE ? INDENT_SIZE : 0);
    for (int i = 0; i < final_indent; i++) insert_character(' ');
}

void auto_dedent(void) {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
    for (int i = 0; i < Editor.cursor_x - 1; i++) if (row->chars[i] != ' ' && row->chars[i] != '\t') return;
    int leading = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == ' ' || row->chars[i] == '\t') leading++;
        else break;
    }
    if (leading >= INDENT_SIZE) {
        int spaces_to_remove = 0;
        for (int i = 0; i < row->size && i < INDENT_SIZE; i++) {
            if (row->chars[i] == ' ') {
                spaces_to_remove++;
            } else {
                break;
            }
        }
        if (spaces_to_remove == 0) return;
        memmove(row->chars, row->chars + spaces_to_remove, row->size - spaces_to_remove + 1);
        row->size -= spaces_to_remove;
        memmove(row->state, row->state + spaces_to_remove, row->size);
        resize_row(row, row->size);
        Editor.cursor_x -= spaces_to_remove;
        if (Editor.cursor_x < 0) Editor.cursor_x = 0;
        Editor.modified = 1;
    }
}

void delete_character(void) {
    if (Editor.cursor_x == 0 && Editor.cursor_y == 0) return;
    if (Editor.cursor_y >= Editor.buffer_rows) return;
    if (!Editor.row) return;
    if (Editor.cursor_x == 0) {
        if (Editor.cursor_y == 0) return;
        struct Row *prev_row = &Editor.row[Editor.cursor_y - 1];
        struct Row *current_row = &Editor.row[Editor.cursor_y];
        int merge_at = prev_row->size;
        char *new_chars = realloc(prev_row->chars, prev_row->size + current_row->size + 1);
        if (!new_chars) die("realloc");
        prev_row->chars = new_chars;
        unsigned char *new_state = realloc(prev_row->state, prev_row->size + current_row->size);
        if (!new_state) die("realloc");
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
        if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
        if (Editor.cursor_x == 0) return;
        int bytes_to_move = row->size - Editor.cursor_x;
        if (bytes_to_move > 0) {
            memmove(&row->chars[Editor.cursor_x - 1], &row->chars[Editor.cursor_x], bytes_to_move);
            memmove(&row->state[Editor.cursor_x - 1], &row->state[Editor.cursor_x], bytes_to_move);
        }
        row->size--;
        row->chars[row->size] = '\0';
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars) row->chars = new_chars;
        int hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_state = realloc(row->state, hl_size);
        if (new_state) row->state = new_state;
        Editor.cursor_x--;
        Editor.modified = 1;
    }
}

void yank_line(void) {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *encoded = malloc(((row->size + 2) / 3) * 4 + 1);
    if (!encoded) return;
    int j = 0;
    for (int i = 0; i < row->size; i += 3) {
        int n = row->chars[i] << 16;
        if (i + 1 < row->size) n |= row->chars[i + 1] << 8;
        if (i + 2 < row->size) n |= row->chars[i + 2];
        encoded[j++] = b64[(n >> 18) & 63];
        encoded[j++] = b64[(n >> 12) & 63];
        encoded[j++] = (i + 1 < row->size) ? b64[(n >> 6) & 63] : '=';
        encoded[j++] = (i + 2 < row->size) ? b64[n & 63] : '=';
    }
    encoded[j] = '\0';
    dprintf(STDOUT_FILENO, "\x1b]52;c;%s\x07", encoded);
    free(encoded);
    display_message(1, "Yanked");
}

void delete_line(void) {
    if (Editor.buffer_rows > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        delete_row(Editor.cursor_y);
        Editor.cursor_x = 0;
        display_message(2, "Deleted");
    }
}

void goto_line(void) {
    char *command = prompt("Go to line: ");
    if (!command) {
        Editor.mode = 0;
        refresh_screen();
        return;
    }
    char *trimmed = command;
    while (isspace((unsigned char)*trimmed)) trimmed++;
    char *end = trimmed + strlen(trimmed) - 1;
    while (end > trimmed && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    char *endptr;
    long line_number = strtol(trimmed, &endptr, 10);
    while (*endptr && isspace((unsigned char)*endptr)) endptr++;
    int valid = (*endptr == '\0' && line_number > 0 && line_number <= Editor.buffer_rows);
    free(command);
    if (valid) {
        Editor.cursor_y = (int)line_number - 1;
        Editor.cursor_x = 0;
        if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        display_message(2, "Jumped");
    } else {
        display_message(1, "Invalid");
    }
    Editor.mode = 0;
    refresh_screen();
}

static int calc_gutter_width(void) {
    if (Editor.file_tree) return 0;
    int max = Editor.buffer_rows > Editor.editor_rows ? Editor.buffer_rows : Editor.editor_rows;
    return (max < 9 ? 1 : max < 99 ? 2 : max < 999 ? 3 : 4) + 1;
}

static void render_line(struct Buffer *ab, int filerow, int wrap_line, int content_width) {
    struct Row *row = &Editor.row[filerow];
    char buf[32];
    if (!Editor.file_tree) {
        int len = snprintf(buf, sizeof(buf), "\x1b[38;5;244m%s%*d \x1b[m", filerow == Editor.cursor_y ? "\x1b[32m" : "", Editor.gutter_width - 1, wrap_line == 0 ? filerow + 1 : 0);
        if (wrap_line > 0) len = snprintf(buf, sizeof(buf), "\x1b[38;5;244m%*s \x1b[m", Editor.gutter_width - 1, "");
        append(ab, buf, len);
    }
    int start = wrap_line * content_width;
    int end = start + content_width;
    if (end > row->size) end = row->size;
    if (Editor.file_tree && (filerow < 3 || filerow == Editor.cursor_y)) append(ab, filerow < 3 ? "\x1b[34m" : "\x1b[7m", filerow < 3 ? 5 : 4);
    int current_hl = 0;
    for (int i = start; i < end; i++) {
        int hl = row->state[i];
        if (!Editor.file_tree && Editor.query && filerow == Editor.found_row) {
            int match_col = Editor.found_col;
            if (i >= match_col && i < match_col + (int)strlen(Editor.query)) hl = 1;
        }
        if (hl != current_hl) {
            current_hl = hl;
            append(ab, hl == 1 ? "\x1b[45m" : "\x1b[0m", hl == 1 ? 5 : 4);
        }
        append(ab, &row->chars[i], 1);
    }
    append(ab, "\x1b[m\x1b[K\r\n", 8);
}

static void draw_content(struct Buffer *ab) {
    if (!ab) return;
    if (Editor.buffer_rows > 0 && !Editor.row) return;
    Editor.gutter_width = calc_gutter_width();
    int content_width = Editor.editor_cols - Editor.gutter_width;
    if (content_width <= 0) return;
    int screen_row = 0;
    for (int filerow = Editor.row_offset; filerow < Editor.buffer_rows && screen_row < Editor.editor_rows; filerow++) {
        if (!Editor.row) break;
        int wrapped = (Editor.row[filerow].size + content_width - 1) / content_width;
        if (wrapped == 0) wrapped = 1;
        for (int wrap = 0; wrap < wrapped && screen_row < Editor.editor_rows; wrap++, screen_row++) render_line(ab, filerow, wrap, content_width);
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

static void handle_command(char *cmd) {
    char *trimmed_cmd = cmd;
    while (isspace((unsigned char)*trimmed_cmd)) trimmed_cmd++;
    const struct { const char *name; void (*func)(void); } extra[] = {
        {"Ex", toggle_file_tree},
        {"find", toggle_workspace_find},
        {"help", display_help},
        {"tags", display_tags},
        {"checkhealth", check_health},
        {NULL, NULL}
    };
    for (int i = 0; extra[i].name; i++) {
        if (strcmp(trimmed_cmd, extra[i].name) == 0) {
            extra[i].func();
            return;
        }
    }
    if (strcmp(trimmed_cmd, "q") == 0) {
        run_cleanup();
        Editor.file_tree = Editor.help_view = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
        Editor.cursor_x = Editor.cursor_y = Editor.row_offset = Editor.modified = 0;
        refresh_screen();
    } else if (strcmp(trimmed_cmd, "quit") == 0) {
        write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
        run_cleanup();
        exit(0);
    } else if (strcmp(trimmed_cmd, "bd") == 0) {
        if (Editor.file_tree) toggle_file_tree();
    } else if (strncmp(trimmed_cmd, "w", 1) == 0) {
        char *arg = NULL;
        if (strncmp(trimmed_cmd, "w ", 2) == 0) {
            arg = trimmed_cmd + 2;
            while (isspace((unsigned char)*arg)) arg++;
        } else if (strncmp(trimmed_cmd, "write ", 6) == 0) {
            arg = trimmed_cmd + 6;
            while (isspace((unsigned char)*arg)) arg++;
        }
        if (arg && *arg) {
            if (Editor.filename) free(Editor.filename);
            Editor.filename = strdup(arg);
            if (!Editor.filename) die("strdup");
        }
        if (!Editor.filename) {
            char *filename = prompt("Save as: %s (ESC to cancel)");
            if (!filename) { refresh_screen(); return; }
            Editor.filename = filename;
        }
        save_file();
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "E182: Not an editor command: '%s'", trimmed_cmd);
        display_message(2, msg);
        input_read_key();
        refresh_screen();
    }
}

void command_mode(void) {
    size_t bufsize = 256, buflen = 0;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    buf[0] = '\0';
    while(1) {
        if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        struct Buffer ab = BUFFER_INIT;
        append(&ab, "\x1b[?25l\x1b[H", 9);
        draw_content(&ab);
        display_status(&ab);
        char pos[64];
        snprintf(pos, sizeof(pos), "\x1b[%d;1H\x1b[K:%s\x1b[%d;%dH\x1b[?25h", Editor.editor_rows + 2, buf, Editor.editor_rows + 2, (int)buflen + 2);
        append(&ab, pos, strlen(pos));
        if (write(STDOUT_FILENO, ab.b, ab.length) == -1) {
            free(ab.b);
            free(buf);
            die("write");
        }
        free(ab.b);
        int c = input_read_key();
        if (c == '\r') {
            if (buflen > 0) handle_command(buf);
            Editor.mode = 0;
            free(buf);
            refresh_screen();
            return;
        } else if (c == '\x1b') {
            Editor.mode = 0;
            free(buf);
            refresh_screen();
            return;
        } else if (c == 127 || c == CTRL_KEY('h')) {
            if (buflen > 0) buf[--buflen] = '\0';
        } else if (c >= 32 && c < 127) {
            if (buflen >= bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
                if (!buf) die("realloc");
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

void refresh_screen(void) {
    if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
    if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
    struct Buffer ab = BUFFER_INIT;
    append(&ab, "\x1b[?25l\x1b[H", 9);
    draw_content(&ab);
    display_status(&ab);
    Editor.gutter_width = calc_gutter_width();
    int content_width = Editor.editor_cols - Editor.gutter_width;
    if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        struct Row *row = &Editor.row[Editor.cursor_y];
        if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
        if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    } else {
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
    }
    int screen_row = 0;
    for (int filerow = Editor.row_offset; filerow < Editor.cursor_y && filerow < Editor.buffer_rows; filerow++) {
        int wrapped = (Editor.row[filerow].size + content_width - 1) / content_width;
        screen_row += wrapped > 0 ? wrapped : 1;
    }
    int cur_x = 1, cur_y = screen_row + 1;
    if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
        int wrap_index = Editor.cursor_x / content_width;
        screen_row += wrap_index;
        int col_in_wrap = Editor.cursor_x % content_width;
        if (wrap_index == 0) {
            cur_x = col_in_wrap + Editor.gutter_width + 1;
        } else {
            cur_x = col_in_wrap + 1;
        }
        cur_y = screen_row + 1;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH%s\x1b[?25h", cur_y, cur_x, Editor.mode == 1 ? "\x1b[5 q" : "\x1b[2 q");
    append(&ab, buf, strlen(buf));
    if (write(STDOUT_FILENO, ab.b, ab.length) == -1) die("write");
    free(ab.b);
}
