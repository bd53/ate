#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "file.h"
#include "tree.h"
#include "utils.h"

static FileEntry *file_entries = NULL;
static int num_entries = 0;
static int entries_capacity = 0;

void editorFreeFileEntries() {
    for (int i = 0; i < num_entries; i++) {
        free(file_entries[i].name);
        file_entries[i].name = NULL;
        free(file_entries[i].path);
        file_entries[i].path = NULL;
    }
    free(file_entries);
    file_entries = NULL;
    num_entries = 0;
    entries_capacity = 0;
}

static void editorScanDirectory(const char *path, int depth) {
    if (depth > 5) return;
    if (path == NULL) return;
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    FileEntry *temp_entries = NULL;
    int temp_count = 0;
    int temp_capacity = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (entry->d_name[0] == '.')
            continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == -1) continue;
        if (temp_count >= temp_capacity) {
            temp_capacity = temp_capacity == 0 ? 32 : temp_capacity * 2;
            FileEntry *new_entries = realloc(temp_entries, sizeof(FileEntry) * temp_capacity);
            if (!new_entries) {
                for (int i = 0; i < temp_count; i++) {
                    free(temp_entries[i].name);
                    free(temp_entries[i].path);
                }
                free(temp_entries);
                closedir(dir);
                die("realloc");
            }
            temp_entries = new_entries;
        }
        temp_entries[temp_count].name = strdup(entry->d_name);
        temp_entries[temp_count].path = strdup(full_path);
        if (!temp_entries[temp_count].name || !temp_entries[temp_count].path) {
            for (int i = 0; i <= temp_count; i++) {
                free(temp_entries[i].name);
                free(temp_entries[i].path);
            }
            free(temp_entries);
            closedir(dir);
            die("strdup");
        }
        temp_entries[temp_count].is_dir = S_ISDIR(st.st_mode);
        temp_entries[temp_count].depth = depth;
        temp_count++;
    }
    closedir(dir);
    if (temp_count > 0) {
        for (int i = 0; i < temp_count - 1; i++) {
            for (int j = i + 1; j < temp_count; j++) {
                int swap = 0;
                if (temp_entries[i].is_dir && !temp_entries[j].is_dir) {
                    swap = 0;
                } else if (!temp_entries[i].is_dir && temp_entries[j].is_dir) {
                    swap = 1;
                } else {
                    swap = strcmp(temp_entries[i].name, temp_entries[j].name) > 0;
                }
                if (swap) {
                    FileEntry temp = temp_entries[i];
                    temp_entries[i] = temp_entries[j];
                    temp_entries[j] = temp;
                }
            }
        }
        for (int i = 0; i < temp_count; i++) {
            if (num_entries >= entries_capacity) {
                entries_capacity = entries_capacity == 0 ? 64 : entries_capacity * 2;
                FileEntry *new_entries = realloc(file_entries, sizeof(FileEntry) * entries_capacity);
                if (!new_entries) {
                    for (int k = 0; k < temp_count; k++) {
                        free(temp_entries[k].name);
                        free(temp_entries[k].path);
                    }
                    free(temp_entries);
                    die("realloc");
                }
                file_entries = new_entries;
            }
            file_entries[num_entries].name = temp_entries[i].name;
            file_entries[num_entries].path = temp_entries[i].path;
            file_entries[num_entries].is_dir = temp_entries[i].is_dir;
            file_entries[num_entries].depth = temp_entries[i].depth;
            num_entries++;
        }
    }
    free(temp_entries);
}

void editorBuildFileTree(const char *root_path) {
    editorFreeRows();
    editorFreeFileEntries();
    char cwd[1024];
    const char *scan_path = root_path ? root_path : getcwd(cwd, sizeof(cwd));
    if (!scan_path) {
        editorContentAppendRow("Could not get current directory.", 32);
        return;
    }
    editorScanDirectory(scan_path, 0);
    char title[256];
    snprintf(title, sizeof(title), "netft - %s", scan_path);
    editorContentAppendRow(title, strlen(title));
    editorContentAppendRow("Press ENTER to open file/directory, q to close", 47);
    editorContentAppendRow("==============================================================================", 78);
    editorContentAppendRow("../", 3);
    for (int i = 0; i < num_entries; i++) {
        char line[512];
        int offset = 0;
        for (int d = 0; d < file_entries[i].depth; d++) {
            line[offset++] = ' ';
            line[offset++] = ' ';
        }
        if (file_entries[i].is_dir) {
            snprintf(line + offset, sizeof(line) - offset, "%s/", file_entries[i].name);
        } else {
            snprintf(line + offset, sizeof(line) - offset, "%s", file_entries[i].name);
        }
        editorContentAppendRow(line, strlen(line));
    }
}

void editorFileTreeOpen() {
    if (E.cy < 0 || E.cy >= E.numrows) return;
    erow *current_row = &E.row[E.cy];
    char *row_content = current_row->chars;
    if (E.cy == 3 && current_row->size >= 2 && strncmp(row_content, "../", 3) == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            char parent_path[1024];
            snprintf(parent_path, sizeof(parent_path), "%s", cwd);
            char *parent = dirname(parent_path);
            if (parent && chdir(parent) == 0) {
                editorBuildFileTree(NULL);
                E.cx = 0;
                E.cy = 0;
                E.rowoff = 0;
            }
        }
        return;
    }
    if (E.cy < 4) return;
    int entry_index = E.cy - 4;
    if (entry_index < 0 || entry_index >= num_entries) return;
    FileEntry *entry = &file_entries[entry_index];
    if (entry->is_dir) {
        if (chdir(entry->path) == 0) {
            editorBuildFileTree(NULL);
            E.cx = 0;
            E.cy = 0;
            E.rowoff = 0;
        }
    } else {
        E.is_file_tree = 0;
        E.is_help_view = 0;
        editorOpen(entry->path);
        if (E.numrows == 0) {
            editorContentAppendRow("", 0);
        }
        E.cx = 0;
        E.cy = 0;
        E.rowoff = 0;
    }
}

void editorToggleFileTree() {
    if (E.is_file_tree) {
        editorCleanup();
        E.is_file_tree = 0;
        if (E.numrows == 0) {
            editorContentAppendRow("", 0);
        }
    } else {
        editorCleanup();
        E.is_file_tree = 1;
        editorBuildFileTree(NULL);
        E.cx = 0;
        E.cy = 0;
        E.rowoff = 0;
    }
    editorRefreshScreen();
}
