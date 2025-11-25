#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efunc.h"
#include "estruct.h"
#include "util.h"

static char **file_names = NULL;
static char **file_paths = NULL;
static int *file_is_dir = NULL;
static int *file_depth = NULL;
static int num_entries = 0;
static int entries_capacity = 0;

void free_file_entries(void) {
    for (int i = 0; i < num_entries; i++) {
        free(file_names[i]);
        file_names[i] = NULL;
        free(file_paths[i]);
        file_paths[i] = NULL;
    }
    free(file_names);
    free(file_paths);
    free(file_is_dir);
    free(file_depth);
    file_names = NULL;
    file_paths = NULL;
    file_is_dir = NULL;
    file_depth = NULL;
    num_entries = 0;
    entries_capacity = 0;
}

static void scan_directory(const char *path, int depth) {
    if (depth > 5) return;
    if (path == NULL) return;
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    char **temp_names = NULL;
    char **temp_paths = NULL;
    int *temp_is_dir = NULL;
    int *temp_depth = NULL;
    int temp_count = 0;
    int temp_capacity = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.') continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == -1) continue;
        if (temp_count >= temp_capacity) {
            temp_capacity = temp_capacity == 0 ? 32 : temp_capacity * 2;
            char **new_names = realloc(temp_names, sizeof(char*) * temp_capacity);
            char **new_paths = realloc(temp_paths, sizeof(char*) * temp_capacity);
            int *new_is_dir = realloc(temp_is_dir, sizeof(int) * temp_capacity);
            int *new_depth = realloc(temp_depth, sizeof(int) * temp_capacity);
            if (!new_names || !new_paths || !new_is_dir || !new_depth) {
                char **cleanup_names = new_names ? new_names : temp_names;
                char **cleanup_paths = new_paths ? new_paths : temp_paths;
                for (int i = 0; i < temp_count; i++) {
                    if (cleanup_names) free(cleanup_names[i]);
                    if (cleanup_paths) free(cleanup_paths[i]);
                }
                if (new_names) free(new_names); else free(temp_names);
                if (new_paths) free(new_paths); else free(temp_paths);
                if (new_is_dir) free(new_is_dir); else free(temp_is_dir);
                if (new_depth) free(new_depth); else free(temp_depth);
                closedir(dir);
                free_file_entries();
                die("realloc");
            }
            temp_names = new_names;
            temp_paths = new_paths;
            temp_is_dir = new_is_dir;
            temp_depth = new_depth;
        }
        temp_names[temp_count] = strdup(entry->d_name);
        temp_paths[temp_count] = strdup(full_path);
        if (!temp_names[temp_count] || !temp_paths[temp_count]) {
            free(temp_names[temp_count]);
            free(temp_paths[temp_count]);
            for (int i = 0; i < temp_count; i++) {
                free(temp_names[i]);
                free(temp_paths[i]);
            }
            free(temp_names);
            free(temp_paths);
            free(temp_is_dir);
            free(temp_depth);
            closedir(dir);
            free_file_entries();
            die("strdup");
        }
        temp_is_dir[temp_count] = S_ISDIR(st.st_mode);
        temp_depth[temp_count] = depth;
        temp_count++;
    }
    closedir(dir);
    if (temp_count > 0) {
        for (int i = 0; i < temp_count - 1; i++) {
            for (int j = i + 1; j < temp_count; j++) {
                int swap = 0;
                if (temp_is_dir[i] && !temp_is_dir[j]) {
                    swap = 0;
                } else if (!temp_is_dir[i] && temp_is_dir[j]) {
                    swap = 1;
                } else {
                    swap = strcmp(temp_names[i], temp_names[j]) > 0;
                }
                if (swap) {
                    char *temp_name = temp_names[i];
                    temp_names[i] = temp_names[j];
                    temp_names[j] = temp_name;
                    char *temp_path = temp_paths[i];
                    temp_paths[i] = temp_paths[j];
                    temp_paths[j] = temp_path;
                    int temp_dir = temp_is_dir[i];
                    temp_is_dir[i] = temp_is_dir[j];
                    temp_is_dir[j] = temp_dir;
                    int temp_dep = temp_depth[i];
                    temp_depth[i] = temp_depth[j];
                    temp_depth[j] = temp_dep;
                }
            }
        }
        for (int i = 0; i < temp_count; i++) {
            if (num_entries >= entries_capacity) {
                entries_capacity = entries_capacity == 0 ? 64 : entries_capacity * 2;
                char **new_names = realloc(file_names, sizeof(char*) * entries_capacity);
                char **new_paths = realloc(file_paths, sizeof(char*) * entries_capacity);
                int *new_is_dir = realloc(file_is_dir, sizeof(int) * entries_capacity);
                int *new_depth = realloc(file_depth, sizeof(int) * entries_capacity);
                if (!new_names || !new_paths || !new_is_dir || !new_depth) {
                    for (int k = 0; k < temp_count; k++) {
                        free(temp_names[k]);
                        free(temp_paths[k]);
                    }
                    free(temp_names);
                    free(temp_paths);
                    free(temp_is_dir);
                    free(temp_depth);
                    free_file_entries();
                    die("realloc");
                }
                file_names = new_names;
                file_paths = new_paths;
                file_is_dir = new_is_dir;
                file_depth = new_depth;
            }
            file_names[num_entries] = temp_names[i];
            file_paths[num_entries] = temp_paths[i];
            file_is_dir[num_entries] = temp_is_dir[i];
            file_depth[num_entries] = temp_depth[i];
            num_entries++;
        }
    }
    free(temp_names);
    free(temp_paths);
    free(temp_is_dir);
    free(temp_depth);
}

static void build_file_tree(const char *root_path) {
    free_rows();
    free_file_entries();
    char cwd[1024];
    const char *scan_path = root_path ? root_path : getcwd(cwd, sizeof(cwd));
    if (!scan_path) {
        append_row("Could not get current directory.", 32);
        return;
    }
    scan_directory(scan_path, 0);
    char title[256];
    snprintf(title, sizeof(title), "netft - %s", scan_path);
    append_row(title, strlen(title));
    append_row("Press ENTER to open file/directory, q to close", 47);
    append_row("==============================================================================", 78);
    append_row("../", 3);
    for (int i = 0; i < num_entries; i++) {
        char line[512];
        int offset = 0;
        for (int d = 0; d < file_depth[i]; d++) {
            line[offset++] = ' ';
            line[offset++] = ' ';
        }
        if (file_is_dir[i]) {
            snprintf(line + offset, sizeof(line) - offset, "%s/", file_names[i]);
        } else {
            snprintf(line + offset, sizeof(line) - offset, "%s", file_names[i]);
        }
        append_row(line, strlen(line));
    }
}

void open_file_tree(void) {
    if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows) return;
    struct Row *current_row = &Editor.row[Editor.cursor_y];
    char *row_content = current_row->chars;
    if (Editor.cursor_y == 3 && current_row->size >= 2 && strncmp(row_content, "../", 3) == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            char parent_path[1024];
            snprintf(parent_path, sizeof(parent_path), "%s", cwd);
            char *parent = dirname(parent_path);
            if (parent && chdir(parent) == 0) {
                build_file_tree(NULL);
                Editor.cursor_x = 0;
                Editor.cursor_y = 0;
                Editor.row_offset = 0;
            }
        }
        return;
    }
    if (Editor.cursor_y < 4) return;
    int entry_index = Editor.cursor_y - 4;
    if (entry_index < 0 || entry_index >= num_entries) return;
    if (file_is_dir[entry_index]) {
        if (chdir(file_paths[entry_index]) == 0) {
            build_file_tree(NULL);
            Editor.cursor_x = 0;
            Editor.cursor_y = 0;
            Editor.row_offset = 0;
        }
    } else {
        Editor.help_view = 0;
        Editor.file_tree = 0;
        Editor.tag_view = 0;
        display_editor(file_paths[entry_index]);
        if (Editor.buffer_rows == 0) append_row("", 0);
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
        Editor.row_offset = 0;
    }
}

void toggle_file_tree(void) {
    if (Editor.file_tree) {
        run_cleanup();
        Editor.help_view = 0;
        Editor.file_tree = 0;
        Editor.tag_view = 0;
        if (Editor.buffer_rows == 0) append_row("", 0);
    } else {
        run_cleanup();
        Editor.file_tree = 1;
        build_file_tree(NULL);
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
        Editor.row_offset = 0;
    }
    refresh_screen();
}
