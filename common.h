#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <termios.h>
#include <libgen.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define BUFFER_INIT {NULL, 0}

enum Highlight {
    HL_NORMAL = 0,
    HL_MATCH,
};

enum Mode {
    MODE_NORMAL = 0,
    MODE_INSERT = 1,
    MODE_COMMAND = 2
};

typedef struct {
    int size;
    char *chars;
    unsigned char *hl;
    int hl_open_comment;
} erow;

struct buffer {
    char *b;
    int len;
};

struct Setup {
    int cx, cy;
    int rowoff;
    int numrows;
    erow *row;
    int screenrows;
    int screencols;
    int gutter_width;
    int mode;
    char *query;
    int match_row;
    int match_col;
    struct termios orig_termios;
    int search_start_row;
    int search_start_col;
    char *filename;
    int dirty;
    int is_help_view;
    int is_file_tree;
};

extern struct Setup E;

#endif
