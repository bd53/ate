#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <termios.h>
#include <libgen.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define BUFFER_INIT {NULL, 0}

typedef struct termios Termios;

enum Mode {
    MODE_NORMAL = 0,
    MODE_INSERT = 1,
    MODE_COMMAND = 2
};

typedef struct Row {
    int size;
    char *chars;
    unsigned char *state;
} Row;

typedef struct buffer {
    char *b;
    int length;
} buffer;

typedef struct Setup {
    int cursor_x, curor_y;
    int row_offset;
    int buffer_rows;
    Row *row;
    int editor_rows;
    int editor_cols;
    int gutter_width;
    int mode;
    char *query;
    int found_row;
    int found_col;
    Termios original;
    char *filename;
    int modified;
    int help_view;
    int file_tree;
} Setup;

#ifdef GLOBALS
Setup Editor = {
    .cursor_x = 0,
    .curor_y = 0,
    .row_offset = 0,
    .buffer_rows = 0,
    .row = NULL,
    .editor_rows = 0,
    .editor_cols = 0,
    .gutter_width = 0,
    .mode = MODE_NORMAL,
    .query = NULL,
    .found_row = -1,
    .found_col = -1,
    .filename = NULL,
    .modified = 0,
    .help_view = 0,
    .file_tree = 0
};
#else
extern Setup Editor;
#endif

#endif