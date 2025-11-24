#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efunc.h"
#include "util.h"

static char **filepaths = NULL;
static int *rows = NULL;
static int *cols = NULL;
static char **line_contents = NULL;

static int num_results = 0;
static int current_index = -1;
static int capacity = 0;

static void add_search_result(const char *filepath, int row, int col, const char *line_content) {
    if (num_results >= capacity) {
        int new_capacity = capacity == 0 ? 64 : capacity * 2;
        char **new_filepaths = realloc(filepaths, sizeof(char *) * new_capacity);
        int *new_rows = realloc(rows, sizeof(int) * new_capacity);
        int *new_cols = realloc(cols, sizeof(int) * new_capacity);
        char **new_line_contents = realloc(line_contents, sizeof(char *) * new_capacity);
        if (!new_filepaths || !new_rows || !new_cols || !new_line_contents) {
            if (new_filepaths && new_filepaths != filepaths) free(new_filepaths);
            if (new_rows && new_rows != rows) free(new_rows);
            if (new_cols && new_cols != cols) free(new_cols);
            if (new_line_contents && new_line_contents != line_contents) free(new_line_contents);
            return;
        }
        filepaths = new_filepaths;
        rows = new_rows;
        cols = new_cols;
        line_contents = new_line_contents;
        capacity = new_capacity;
    }
    char *fp_copy = strdup(filepath);
    char *lc_copy = strdup(line_content);
    if (!fp_copy || !lc_copy) {
        free(fp_copy);
        free(lc_copy);
        return;
    }
    filepaths[num_results] = fp_copy;
    rows[num_results] = row;
    cols[num_results] = col;
    line_contents[num_results] = lc_copy;
    num_results++;
}

static void collect_and_search_files(const char *dirpath, const char *query, int depth) {
    if (depth > 10) return;
    if (dirpath == NULL || query == NULL) return;
    DIR *dir = opendir(dirpath);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.') continue;
        char full_path[2048];
        size_t dirpath_len = strlen(dirpath);
        size_t name_len = strlen(entry->d_name);
        if (dirpath_len + name_len + 2 >= sizeof(full_path)) continue;
        int n = snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);
        if (n < 0 || n >= (int)sizeof(full_path)) continue;
        struct stat st;
        if (stat(full_path, &st) == -1) continue;
        if (S_ISDIR(st.st_mode)) {
            collect_and_search_files(full_path, query, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            FILE *fp = fopen(full_path, "rb");
            if (!fp) continue;
            unsigned char buffer[512];
            size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
            fclose(fp);
            int is_binary = 0;
            for (size_t i = 0; i < bytes_read; i++) {
                if (buffer[i] == 0) {
                    is_binary = 1;
                    break;
                }
            }
            if (is_binary) continue;
            fp = fopen(full_path, "r");
            if (!fp) continue;
            char *line = NULL;
            size_t linecap = 0;
            ssize_t linelen;
            int row_num = 0;
            while ((linelen = getline(&line, &linecap, fp)) != -1) {
                if (linelen > 0 && line[linelen - 1] == '\n') {
                    line[linelen - 1] = '\0';
                    linelen--;
                }
                if (linelen > 0 && line[linelen - 1] == '\r') {
                    line[linelen - 1] = '\0';
                    linelen--;
                }
                char *pos = line;
                while ((pos = strstr(pos, query)) != NULL) {
                    int col = (int)(pos - line);
                    add_search_result(full_path, row_num, col, line);
                    pos++;
                }
                row_num++;
            }
            free(line);
            fclose(fp);
        }
    }
    closedir(dir);
}

void free_workspace_search() {
    if (filepaths) {
        for (int i = 0; i < num_results; i++) {
            free(filepaths[i]);
            free(line_contents[i]);
        }
        free(filepaths);
        free(rows);
        free(cols);
        free(line_contents);
    }
    filepaths = NULL;
    rows = NULL;
    cols = NULL;
    line_contents = NULL;
    num_results = 0;
    current_index = -1;
    capacity = 0;
}

static void jump_to_result(int index) {
    if (index < 0 || index >= num_results) return;
    char *result_filepath = filepaths[index];
    int result_row = rows[index];
    int result_col = cols[index];
    if (Editor.modified && Editor.filename) save_file();
    if (Editor.filename == NULL || strcmp(Editor.filename, result_filepath) != 0) display_editor(result_filepath);
    if (result_row >= 0 && result_row < Editor.buffer_rows) {
        Editor.cursor_y = result_row;
        Editor.cursor_x = result_col;
        Editor.found_row = result_row;
        Editor.found_col = result_col;
        int content_width = Editor.editor_cols - Editor.gutter_width;
        if (content_width <= 0) content_width = 80; // fallback
        if (Editor.cursor_y < Editor.row_offset) Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows) Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        int wrap_line = Editor.cursor_x / content_width;
        int screen_rows_before = 0;
        for (int i = Editor.row_offset; i < Editor.cursor_y && i < Editor.buffer_rows; i++) {
            struct Row *row = &Editor.row[i];
            int wrapped_lines = (row->size + content_width - 1) / content_width;
            if (wrapped_lines == 0) wrapped_lines = 1;
            screen_rows_before += wrapped_lines;
        }
        int total_screen_row = screen_rows_before + wrap_line;
        if (total_screen_row >= Editor.editor_rows) {
            int target_offset = Editor.cursor_y - Editor.editor_rows / 2;
            if (target_offset < 0) target_offset = 0;
            Editor.row_offset = target_offset;
        }
    }
    char msg[512];
    const char *display_path = result_filepath;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        size_t cwd_len = strlen(cwd);
        if (strncmp(result_filepath, cwd, cwd_len) == 0 && result_filepath[cwd_len] == '/') {
            display_path = result_filepath + cwd_len + 1;
        }
    }
    snprintf(msg, sizeof(msg), "Match %d/%d: %s:%d:%d", index + 1, num_results, display_path, result_row + 1, result_col + 1);
    display_message(1, msg);
}

void toggle_workspace_find() {
    free_workspace_search();
    char *query = prompt("Find in workspace: %s (ESC to cancel)");
    if (query == NULL) {
        refresh_screen();
        return;
    }
    if (strlen(query) == 0) {
        free(query);
        refresh_screen();
        return;
    }
    if (Editor.query) free(Editor.query);
    Editor.query = strdup(query);
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        free(query);
        display_message(2, "Could not get current directory");
        refresh_screen();
        return;
    }
    refresh_screen();
    collect_and_search_files(cwd, query, 0);
    free(query);
    if (num_results > 0) {
        current_index = 0;
        jump_to_result(0);
    } else {
        display_message(2, "No matches found in workspace");
    }
    refresh_screen();
}

void workspace_find_next(int direction) {
    if (num_results == 0) {
        refresh_screen();
        return;
    }
    current_index += direction;
    if (current_index >= num_results) {
        current_index = 0;
    } else if (current_index < 0) {
        current_index = num_results - 1;
    }
    jump_to_result(current_index);
    refresh_screen();
}