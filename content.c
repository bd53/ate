#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "content.h"
#include "tree.h"
#include "utils.h"

void editorFreeRows() {
    for (int j = 0; j < E.numrows; j++) {
        free(E.row[j].chars);
        E.row[j].chars = NULL;
        free(E.row[j].hl);
        E.row[j].hl = NULL;
    }
    free(E.row);
    E.row = NULL;
    E.numrows = 0;
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
}

void editorCleanup() {
    if (E.query) {
        free(E.query);
        E.query = NULL;
    }
    editorFreeRows();
    editorFreeFileEntries();
}

void editorContentInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void editorContentAppendRow(char *s, size_t len) {
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    erow *row = &E.row[at];
    free(row->chars);
    row->chars = NULL;
    free(row->hl);
    row->hl = NULL;
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty = 1;
    if (E.numrows == 0) {
        E.cy = 0;
    } else if (E.cy >= E.numrows) {
        E.cy = E.numrows - 1;
    }
    if (E.cy < 0) {
        E.cy = 0;
    }
}

void editorContentInsertChar(char c) {
    if (E.cy == E.numrows) {
        editorContentAppendRow("", 0);
    }
    erow *row = &E.row[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > row->size) E.cx = row->size;
    char *new = realloc(row->chars, row->size + 2);
    if (new == NULL) die("realloc");
    row->chars = new;
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->hl, new_hl_size);
    if (new_hl == NULL) die("realloc");
    row->hl = new_hl;
    E.cx++;
    E.dirty = 1;
}

void editorContentInsertNewline() {
    if (E.cy == E.numrows) {
        editorContentAppendRow("", 0);
    } else {
        erow *row = &E.row[E.cy];
        int split_at = E.cx;
        if (split_at < 0) split_at = 0;
        if (split_at > row->size) split_at = row->size;
        int second_half_len = row->size - split_at;
        char *second_half = malloc(second_half_len + 1);
        if (second_half == NULL) die("malloc");
        memcpy(second_half, &row->chars[split_at], second_half_len);
        second_half[second_half_len] = '\0';
        row->size = split_at;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars == NULL) {
            free(second_half);
            die("realloc");
        }
        row->chars = new_chars;
        row->chars[row->size] = '\0';
        int hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->hl, hl_size);
        if (new_hl == NULL) {
            free(second_half);
            die("realloc");
        }
        row->hl = new_hl;
        editorContentInsertRow(E.cy + 1, second_half, second_half_len);
        free(second_half);
    }
    E.cy++;
    E.cx = 0;
    E.dirty = 1;
}
