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

struct State {
        int cursor_x, cursor_y;
        int row_offset;
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

extern struct State Editor;

#endif