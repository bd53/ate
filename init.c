#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "init.h"
#include "utils.h"

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.mode = MODE_NORMAL;
    E.gutter_width = 0;
    E.query = NULL;
    E.match_row = -1;
    E.match_col = -1;
    E.search_start_row = 0;
    E.search_start_col = 0;
    E.filename = NULL;
    E.dirty = 0;
    E.is_help_view = 0;
    E.is_file_tree = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}
