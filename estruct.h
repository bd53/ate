#ifndef ESTRUCT_H
#define ESTRUCT_H

#include <termios.h>

#include "edef.h"

extern struct termios original;
extern struct EditorState editor;
extern struct FileTree filetree;
extern struct SearchState search_state;
extern struct GotoState goto_state;
extern int browse_mode;
extern int search_mode;
extern int help_mode;
extern int goto_mode;

struct EditorState {
        char **lines;
        int line_numbers;
        int cursor_x;
        int cursor_y;
        int offset_y;
        char *filename;
        int modified;
};

struct FileEntry {
        char *name;
        int is_directory;
};

struct FileTree {
        struct FileEntry *entries;
        int entry_count;
        int selected_index;
        int offset;
        char *current_path;
};

struct SearchResult {
        char *filepath;
        int line_number;
        char *line_content;
        int score;
};

struct SearchState {
        struct SearchResult results[MAX_SEARCH_RESULTS];
        int result_count;
        int selected_index;
        int scroll_offset;
        char query[MAX_SEARCH_QUERY];
        int query_len;
};

struct GotoState {
        char input[MAX_GOTO_INPUT];
        int input_len;
        char error_msg[64];
};

#endif
