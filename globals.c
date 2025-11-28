#include <stddef.h>

#include "estruct.h"

struct State Editor = {
        .cursor_x = 0,
        .cursor_y = 0,
        .row_offset = 0,
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
