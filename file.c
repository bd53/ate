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

int editorOpen(char *filename) {
    if (filename == NULL) return -1;
    char *new_filename = strdup(filename);
    if (new_filename == NULL) die("strdup");
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        editorFreeRows();
        editorFreeFileEntries();
        if (E.query) {
            free(E.query);
            E.query = NULL;
        }
        E.filename = new_filename;
        E.is_help_view = 0;
        E.is_file_tree = 0;
        editorContentAppendRow("", 0);
        return -1;
    }
    editorFreeRows();
    editorFreeFileEntries();
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
        editorContentAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
    return 0;
}

void editorSave() {
    if (E.filename == NULL) {
        char *filename = editorPrompt("Save as: %s (ESC to cancel)");
        if (filename == NULL) {
            editorRefreshScreen();
            return;
        }
        E.filename = filename;
    }
    FILE *fp = fopen(E.filename, "w");
    if (fp == NULL) {
        editorRefreshScreen();
        return;
    }
    for (int i = 0; i < E.numrows; i++) {
        fwrite(E.row[i].chars, 1, E.row[i].size, fp);
        fputc('\n', fp);
    }
    if (fclose(fp) == EOF) {
        editorRefreshScreen();
        return;
    }
    E.dirty = 0;
    editorRefreshScreen();
}

void editorOpenFile() {
    char *filename = editorPrompt("Open file: %s (ESC to cancel)");
    if (!filename) {
        editorRefreshScreen();
        return;
    }
    if (E.is_file_tree) {
        editorFreeFileEntries();
    }
    E.is_file_tree = 0;
    E.is_help_view = 0;
    editorOpen(filename);
    free(filename);
    if (E.numrows == 0) {
        editorContentAppendRow("", 0);
    }
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    editorRefreshScreen();
}
