#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ebind.h"
#include "util.h"

void run_cleanup() {
    if (Editor.query) {
        free(Editor.query);
        Editor.query = NULL;
    }
    free_rows();
    free_file_entries();
    free_workspace_search();
}

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (rows == NULL || cols == NULL) return -1;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (rows == NULL || cols == NULL) return -1;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0 && ws.ws_row != 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    } else {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    }
}

void append(struct Buffer *ab, const char *s, int len) {
    if (ab == NULL || s == NULL || len < 0) return;
    if (len == 0) return;
    char *new = realloc(ab->b, ab->length + len);
    if (new == NULL) die("realloc");
    memcpy(&new[ab->length], s, len);
    ab->b = new;
    ab->length += len;
}

int leading_whitespace(const char *line, int len) {
    int count = 0;
    for (int i = 0; i < len; i++) {
        if (line[i] == ' ' || line[i] == '\t') {
            count++;
        } else {
            break;
        }
    }
    return count;
}

char *trim_whitespace(char *str) {
    if (str == NULL) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void trim_leadingspace(int num_spaces) {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *row = &Editor.row[Editor.cursor_y];
    int spaces_to_remove = 0;
    for (int i = 0; i < row->size && i < num_spaces; i++) {
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
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->state, new_hl_size);
    if (new_hl) row->state = new_hl;
    Editor.cursor_x -= spaces_to_remove;
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
}