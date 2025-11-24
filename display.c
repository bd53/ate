#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

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

void draw_status(struct Buffer *ab) {
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
    if (Editor.mode == 0) mode_str = "NORMAL";
    else if (Editor.mode == 1) mode_str = "INSERT";
    else if (Editor.mode == 2) mode_str = "COMMAND";
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
    struct Buffer ab = BUFFER_INIT;
    append(&ab, "\x1b[?25l", 6);
    append(&ab, "\x1b[H", 3);
    draw_content(&ab);
    draw_status(&ab);
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