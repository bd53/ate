#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "keybinds.h"
#include "utils.h"

int open_editor(char *filename) {
    if (filename == NULL) return -1;
    char *new_filename = strdup(filename);
    if (new_filename == NULL) die("strdup");
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        free_rows();
        free_file_entries();
        if (Editor.query) {
            free(Editor.query);
            Editor.query = NULL;
        }
        Editor.filename = new_filename;
        Editor.help_view = 0;
        Editor.file_tree = 0;
        append_row("", 0);
        return -1;
    }
    free_rows();
    free_file_entries();
    if (Editor.query) {
        free(Editor.query);
        Editor.query = NULL;
    }
    Editor.filename = new_filename;
    Editor.help_view = 0;
    Editor.file_tree = 0;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
        append_row(line, linelen);
    }
    free(line);
    fclose(fp);
    return 0;
}

void open_help() {
    if (Editor.help_view) {
        run_cleanup();
        Editor.help_view = 0;
        Editor.file_tree = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
    } else {
        run_cleanup();
        open_editor("help.txt");
        if (Editor.buffer_rows == 0) append_row("", 0);
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
        Editor.row_offset = 0;
        Editor.help_view = 1;
        Editor.file_tree = 0;
    }
    refresh_screen();
}

void scroll_editor() {
    if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
    if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
}

char *prompt(const char *prompt) {
    if (prompt == NULL) return NULL;
    size_t bufsize = 256;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    size_t buflen = 0;
    buf[0] = '\0';
    int prompt_row = Editor.editor_rows + 2;
    char static_prompt[256];
    strncpy(static_prompt, prompt, sizeof(static_prompt) - 1);
    static_prompt[sizeof(static_prompt) - 1] = '\0';
    char *format_pos = strstr(static_prompt, "%s");
    if (format_pos) *format_pos = '\0';
    int prompt_len = strlen(static_prompt);
    while(1) {
        buffer ab = BUFFER_INIT;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        append(&ab, pos_buf, strlen(pos_buf));
        append(&ab, "\x1b[K", 3);
        append(&ab, static_prompt, prompt_len);
        append(&ab, buf, buflen);
        int cursor_col = prompt_len + buflen + 1;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", prompt_row, cursor_col);
        append(&ab, pos_buf, strlen(pos_buf));
        if (write(STDOUT_FILENO, ab.b, ab.length) == -1) {
            free(ab.b);
            free(buf);
            die("write");
        }
        free(ab.b);
        int c = input_read_key();
        if (c == '\r') {
            if (buflen > 0) return buf;
            free(buf);
            return NULL;
        } else if (c == '\x1b') {
            free(buf);
            return NULL;
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

void draw_status(buffer *ab) {
    if (ab == NULL) return;
    append(ab, "\x1b[7m", 4);
    char status[80];
    const char *filetype_name = "txt";
    char filename_status[64];
    if (Editor.file_tree) {
        snprintf(filename_status, sizeof(filename_status), "netft");
    } else {
        snprintf(filename_status, sizeof(filename_status), "%s%s", Editor.filename ? Editor.filename : "[No Name]", Editor.modified ? " *" : "");
    }
    const char *mode_str;
    if (Editor.mode == MODE_NORMAL) mode_str = "NORMAL";
    else if (Editor.mode == MODE_INSERT) mode_str = "INSERT";
    else if (Editor.mode == MODE_COMMAND) mode_str = "COMMAND";
    else mode_str = "UNKNOWN";
    int len = snprintf(status, sizeof(status), " %s | %s | R:%d L:%d", mode_str, filename_status, Editor.cursor_y + 1, Editor.buffer_rows > 0 ? Editor.buffer_rows : 1);
    if (len > Editor.editor_cols) len = Editor.editor_cols;
    append(ab, status, len);
    char rstatus[80];
    int rlen = snprintf(rstatus, sizeof(rstatus), " %s | C:%d ", filetype_name, Editor.cursor_x + 1);
    while (len < Editor.editor_cols - rlen) {
        append(ab, " ", 1);
        len++;
    }
    append(ab, rstatus, rlen);
    append(ab, "\x1b[m", 3);
    append(ab, "\r\n", 2);
}

void refresh_screen() {
    scroll_editor();
    buffer ab = BUFFER_INIT;
    append(&ab, "\x1b[?25l", 6);
    append(&ab, "\x1b[H", 3);
    draw_content(&ab);
    draw_status(&ab);
    int content_width = Editor.editor_cols - Editor.gutter_width;
    int screen_row = 0;
    for (int filerow = Editor.row_offset; filerow < Editor.cursor_y && filerow < Editor.buffer_rows; filerow++) {
        Row *row = &Editor.row[filerow];
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
    if (Editor.mode == MODE_INSERT) {
        append(&ab, "\x1b[5 q", 5);
    } else {
        append(&ab, "\x1b[2 q", 5);
    }
    append(&ab, "\x1b[?25h", 6);
    if (write(STDOUT_FILENO, ab.b, ab.length) == -1) die("write");
    free(ab.b);
}

void display_message(int type, const char *message) {
    const char *color = (type == 1) ? "\x1b[32m" : "\x1b[31m";
    int prompt_row = Editor.editor_rows + 2;
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
    write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
    write(STDOUT_FILENO, "\x1b[K", 3);
    write(STDOUT_FILENO, color, strlen(color));
    write(STDOUT_FILENO, message, strlen(message));
    write(STDOUT_FILENO, "\x1b[0m", 4);
}