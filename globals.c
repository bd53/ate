#include <stddef.h>

#include "estruct.h"

struct termios original;

struct EditorState editor = {
        .lines = NULL,
        .line_numbers = 0,
        .cursor_x = 0,
        .cursor_y = 0,
        .offset_y = 0,
        .filename = NULL,
        .modified = 0
};

struct FileTree filetree = {
        .entries = NULL,
        .entry_count = 0,
        .selected_index = 0,
        .offset = 0,
        .current_path = NULL
};

int browse_mode = 0;
int search_mode = 0;
struct SearchState search_state = { 0 };
