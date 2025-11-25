#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

int display_editor(char *filename) {
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
        Editor.tag_view = 0;
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
    Editor.tag_view = 0;
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

void display_help(void) {
    if (Editor.help_view) {
        run_cleanup();
        Editor.help_view = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
    } else {
        run_cleanup();
        display_editor("help.txt");
        if (Editor.buffer_rows == 0) append_row("", 0);
        Editor.help_view = 1;
    }
    refresh_screen();
}

void display_tags(void) {
    if (Editor.tag_view) {
        run_cleanup();
        Editor.tag_view = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
    } else {
        FILE *fp = fopen("tags", "r");
        if (!fp) {
            display_message(2, "No tags file found.");
            return;
        }
        run_cleanup();
        Editor.tag_view = 1;
        char *line = NULL;
        size_t linecap = 0;
        ssize_t linelen;
        while ((linelen = getline(&line, &linecap, fp)) != -1) {
            while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
            append_row(line, linelen);
        }
        free(line);
        fclose(fp);
        if (Editor.buffer_rows == 0) append_row("Tags file is empty", 18);
    }
}

void display_status(struct Buffer *ab) {
    if (ab == NULL) return;
    append(ab, "\x1b[7m", 4);
    char status[80];
    const char *filetype_name = "[No Name]";
    if (Editor.filename) {
        char *dot = strrchr(Editor.filename, '.');
        if (dot && *(dot + 1) != '\0') {
            filetype_name = dot + 1;
        }
    }
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
    int percentage = (Editor.buffer_rows > 0) ? (int)(((float)(Editor.cursor_y + 1) / Editor.buffer_rows) * 100) : 100;
    int len = snprintf(status, sizeof(status), " %s | %s | R:%d L:%d (%d%%)", mode_str, filename_status, Editor.cursor_y + 1, Editor.buffer_rows > 0 ? Editor.buffer_rows : 1, percentage);
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
