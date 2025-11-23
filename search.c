#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "display.h"
#include "file.h"
#include "search.h"

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
        if (!new_filepaths) return;
        filepaths = new_filepaths;
        int *new_rows = realloc(rows, sizeof(int) * new_capacity);
        if (!new_rows) return;
        rows = new_rows;
        int *new_cols = realloc(cols, sizeof(int) * new_capacity);
        if (!new_cols) return;
        cols = new_cols;
        char **new_line_contents = realloc(line_contents, sizeof(char *) * new_capacity);
        if (!new_line_contents) return;
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
    if (dirpath == NULL) return;
    DIR *dir = opendir(dirpath);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (entry->d_name[0] == '.') continue;
        char full_path[2048];
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
                while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
                    line[linelen - 1] = '\0';
                    linelen--;
                }
                char *pos = line;
                while ((pos = strstr(pos, query)) != NULL) {
                    int col = pos - line;
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

void toggle_workspace_find() {
    free_workspace_search();
    char *query = prompt("Find in workspace: %s (ESC to cancel)");
    if (query == NULL) {
        refresh_screen();
        return;
    }
    if (strlen(query) == 0) {
        free(query);
        char msg[256];
        snprintf(msg, sizeof(msg), "\x1b[31mSearch cancelled\x1b[0m");
        int prompt_row = E.screenrows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, msg, strlen(msg));
        refresh_screen();
        return;
    }
    if (E.query) free(E.query);
    E.query = strdup(query);
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        free(query);
        char msg[256];
        snprintf(msg, sizeof(msg), "\x1b[31mCould not get current directory\x1b[0m");
        int prompt_row = E.screenrows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, msg, strlen(msg));
        refresh_screen();
        return;
    }
    char search_msg[256];
    snprintf(search_msg, sizeof(search_msg), "\x1b[32mSearching workspace...\x1b[0m");
    int prompt_row = E.screenrows + 2;
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
    write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
    write(STDOUT_FILENO, "\x1b[K", 3);
    write(STDOUT_FILENO, search_msg, strlen(search_msg));
    refresh_screen();
    collect_and_search_files(cwd, query, 0);
    free(query);
    if (num_results > 0) {
        current_index = 0;
        int index = 0;
        char *result_filepath = filepaths[index];
        int result_row = rows[index];
        int result_col = cols[index];
        if (E.dirty && E.filename) {
            save_editor();
        }
        if (E.filename == NULL || strcmp(E.filename, result_filepath) != 0) {
            open_editor(result_filepath);
        }
        if (result_row >= 0 && result_row < E.numrows) {
            E.cy = result_row;
            E.cx = result_col;
            E.match_row = result_row;
            E.match_col = result_col;
            scroll_editor();
        }
        char msg[512];
        const char *display_path = result_filepath;
        char cwd_buf[1024];
        if (getcwd(cwd_buf, sizeof(cwd_buf))) {
            size_t cwd_len = strlen(cwd_buf);
            if (strncmp(result_filepath, cwd_buf, cwd_len) == 0 && result_filepath[cwd_len] == '/') {
                display_path = result_filepath + cwd_len + 1;
            }
        }
        snprintf(msg, sizeof(msg), "\x1b[32mMatch %d/%d: %s:%d\x1b[0m", index + 1, num_results, display_path, result_row + 1);
        prompt_row = E.screenrows + 2;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, msg, strlen(msg));
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "\x1b[31mNo matches found in workspace\x1b[0m");
        int prompt_row = E.screenrows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    refresh_screen();
}

void workspace_find_next(int direction) {
    if (num_results == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "\x1b[31mNo search results (use Ctrl-F to search)\x1b[0m");
        int prompt_row = E.screenrows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, msg, strlen(msg));
        refresh_screen();
        return;
    }
    current_index += direction;
    if (current_index >= num_results) {
        current_index = 0;
    } else if (current_index < 0) {
        current_index = num_results - 1;
    }
    int index = current_index;
    char *result_filepath = filepaths[index];
    int result_row = rows[index];
    int result_col = cols[index];
    if (E.dirty && E.filename) {
        save_editor();
    }
    if (E.filename == NULL || strcmp(E.filename, result_filepath) != 0) {
        open_editor(result_filepath);
    }
    if (result_row >= 0 && result_row < E.numrows) {
        E.cy = result_row;
        E.cx = result_col;
        E.match_row = result_row;
        E.match_col = result_col;
        scroll_editor();
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
    snprintf(msg, sizeof(msg), "\x1b[32mMatch %d/%d: %s:%d\x1b[0m", index + 1, num_results, display_path, result_row + 1);
    int prompt_row = E.screenrows + 2;
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
    write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
    write(STDOUT_FILENO, "\x1b[K", 3);
    write(STDOUT_FILENO, msg, strlen(msg));
    refresh_screen();
}