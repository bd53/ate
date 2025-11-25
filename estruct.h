#ifndef ESTRUCT_H
#define ESTRUCT_H

#include <termios.h>

#include "edef.h"

struct Row {
    int size;
    char *chars;
    unsigned char *state;
};

struct Buffer {
    char *b;
    int length;
};

struct Setup {
    int cursor_x, cursor_y;
    int row_offset;
    int col_offset;
    int buffer_rows;
    struct Row *row;
    int editor_rows;
    int editor_cols;
    int gutter_width;
    int mode;
    char *query;
    int found_row;
    int found_col;
    struct termios original;
    char *filename;
    int modified;
    int help_view;
    int file_tree;
    int tag_view;
};

#ifdef GLOBALS
struct Setup Editor = {
    .cursor_x = 0,
    .cursor_y = 0,
    .row_offset = 0,
    .col_offset = 0,
    .buffer_rows = 0,
    .row = NULL,
    .editor_rows = 0,
    .editor_cols = 0,
    .gutter_width = 0,
    .mode = 0,
    .query = NULL,
    .found_row = -1,
    .found_col = -1,
    .filename = NULL,
    .modified = 0,
    .help_view = 0,
    .file_tree = 0,
    .tag_view = 0
};
#else
extern struct Setup Editor;
#endif

#endif