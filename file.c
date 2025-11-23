#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "file.h"
#include "utils.h"

int open_editor(char *filename) {
    if (filename == NULL) return -1;
    char *new_filename = strdup(filename);
    if (new_filename == NULL) die("strdup");
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        free_rows();
        free_file_entries();
        if (E.query) {
            free(E.query);
            E.query = NULL;
        }
        E.filename = new_filename;
        E.is_help_view = 0;
        E.is_file_tree = 0;
        append_row("", 0);
        return -1;
    }
    free_rows();
    free_file_entries();
    if (E.query) {
        free(E.query);
        E.query = NULL;
    }
    E.filename = new_filename;
    E.is_help_view = 0;
    E.is_file_tree = 0;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        append_row(line, linelen);
    }
    free(line);
    fclose(fp);
    return 0;
}

void save_editor() {
    if (E.filename == NULL) {
        char *filename = prompt("Save as: %s (ESC to cancel)");
        if (filename == NULL) {
            refresh_screen();
            return;
        }
        if (E.filename) {
            free(E.filename);
        }
        E.filename = filename;
    }
    FILE *fp = fopen(E.filename, "w");
    if (fp == NULL) {
        refresh_screen();
        return;
    }
    for (int i = 0; i < E.numrows; i++) {
        fwrite(E.row[i].chars, 1, E.row[i].size, fp);
        fputc('\n', fp);
    }
    if (fclose(fp) == EOF) {
        refresh_screen();
        return;
    }
    E.dirty = 0;
    refresh_screen();
}

void open_file() {
    char *filename = prompt("Open file: %s (ESC to cancel)");
    if (!filename) {
        refresh_screen();
        return;
    }
    if (E.is_file_tree) {
        free_file_entries();
    }
    E.is_file_tree = 0;
    E.is_help_view = 0;
    open_editor(filename);
    free(filename);
    if (E.numrows == 0) {
        append_row("", 0);
    }
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    refresh_screen();
}