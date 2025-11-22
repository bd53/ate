#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <termios.h>
#include <libgen.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define BUFFER_INIT {NULL, 0}

enum editorHighlight {
    HL_NORMAL = 0,
    HL_MATCH,
};

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    CTRL_ARROW_LEFT,
    CTRL_ARROW_RIGHT,
    CTRL_ARROW_UP,
    CTRL_ARROW_DOWN
};

enum editorMode {
    MODE_NORMAL = 0,
    MODE_INSERT = 1,
    MODE_COMMAND = 2
};

enum HealthStatus {
    HEALTH_INFO,
    HEALTH_WARNING
};

typedef struct {
    int size;
    char *chars;
    unsigned char *hl;
    int hl_open_comment;
} erow;

typedef struct {
    char *category;
    char *message;
    enum HealthStatus status;
} HealthReport;

typedef struct {
    const char *name;
    const char *cmd;
    const char *version_flag;
    enum HealthStatus found_status;
    enum HealthStatus missing_status;
} ToolCheck;

typedef struct {
    char *name;
    char *path;
    int is_dir;
    int depth;
} FileEntry;

struct buffer {
    char *b;
    int len;
};

struct editorConfig {
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

extern struct editorConfig E;

#endif
