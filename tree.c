#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "efunc.h"
#include "estruct.h"
#include "util.h"

static int compare_entries(const void *a, const void *b)
{
        const struct FileEntry *ea = (const struct FileEntry *) a;
        const struct FileEntry *eb = (const struct FileEntry *) b;
        if (ea->is_directory && !eb->is_directory)
                return -1;
        if (!ea->is_directory && eb->is_directory)
                return 1;
        return strcmp(ea->name, eb->name);
}

void init_filetree(const char *path)
{
        char *start_path;
        if (path && strcmp(path, ".") != 0) {
                start_path = strdup(path);
        } else if (editor.filename) {
                char *last_slash = strrchr(editor.filename, '/');
                if (last_slash) {
                        size_t dir_len = last_slash - editor.filename;
                        start_path = malloc(dir_len + 1);
                        strncpy(start_path, editor.filename, dir_len);
                        start_path[dir_len] = '\0';
                } else {
                        start_path = strdup(".");
                }
        } else {
                start_path = strdup(".");
        }
        filetree.entries = NULL;
        filetree.entry_count = 0;
        filetree.selected_index = 0;
        filetree.offset = 0;
        char abspath[1024];
        if (realpath(start_path, abspath)) {
                filetree.current_path = strdup(abspath);
        } else {
                filetree.current_path = strdup(start_path);
        }
        free(start_path);
        load_directory(filetree.current_path);
}

void free_filetree(void)
{
        for (int i = 0; i < filetree.entry_count; i++) {
                free(filetree.entries[i].name);
        }
        free(filetree.entries);
        free(filetree.current_path);
        filetree.entries = NULL;
        filetree.entry_count = 0;
}

void load_directory(const char *path)
{
        DIR *dir = opendir(path);
        if (!dir) {
                return;
        }
        for (int i = 0; i < filetree.entry_count; i++) {
                free(filetree.entries[i].name);
        }
        free(filetree.entries);
        filetree.entries = NULL;
        filetree.entry_count = 0;
        filetree.selected_index = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) {
                        continue;
                }
                struct stat st;
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", path,
                         entry->d_name);
                if (stat(fullpath, &st) == -1) {
                        continue;
                }
                filetree.entries = realloc(filetree.entries, sizeof(struct FileEntry) * (filetree.entry_count + 1));
                filetree.entries[filetree.entry_count].name = strdup(entry->d_name);
                filetree.entries[filetree.entry_count].is_directory = S_ISDIR(st.st_mode);
                filetree.entry_count++;
        }
        closedir(dir);
        qsort(filetree.entries, filetree.entry_count, sizeof(struct FileEntry), compare_entries);
}

void render_filetree(void)
{
        char buffer[4096];
        int len = 0;
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[2J\x1b[H");
        int rows = get_window_rows() - 2;
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[7m %s \x1b[m\r\n", filetree.current_path);
        if (filetree.selected_index < filetree.offset) {
                filetree.offset = filetree.selected_index;
        }
        if (filetree.selected_index >= filetree.offset + rows) {
                filetree.offset = filetree.selected_index - rows + 1;
        }
        for (int i = 0; i < rows; i++) {
                int entry_idx = i + filetree.offset;
                if (entry_idx >= filetree.entry_count) {
                        len += snprintf(buffer + len, sizeof(buffer) - len, "~\r\n");
                } else {
                        struct FileEntry *entry = &filetree.entries[entry_idx];
                        if (entry_idx == filetree.selected_index) {
                                len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[7m");
                        }
                        if (entry->is_directory) {
                                len += snprintf(buffer + len, sizeof(buffer) - len, " %s/\r\n", entry->name);
                        } else {
                                len += snprintf(buffer + len, sizeof(buffer) - len, " %s\r\n", entry->name);
                        }
                        if (entry_idx == filetree.selected_index) {
                                len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[m");
                        }
                }
        }
        char status[256];
        snprintf(status, sizeof(status), "%d/%d", filetree.selected_index + 1, filetree.entry_count);
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[7m%s", status);
        int cols = get_window_cols();
        int status_len = strlen(status);
        for (int i = status_len; i < cols; i++) {
                len += snprintf(buffer + len, sizeof(buffer) - len, " ");
        }
        len += snprintf(buffer + len, sizeof(buffer) - len, "\x1b[m");
        write(STDOUT_FILENO, buffer, len);
}

void filetree_move_up(void)
{
        if (filetree.selected_index > 0) {
                filetree.selected_index--;
        }
}

void filetree_move_down(void)
{
        if (filetree.selected_index < filetree.entry_count - 1) {
                filetree.selected_index++;
        }
}

void filetree_select(void)
{
        if (filetree.entry_count == 0) {
                return;
        }
        struct FileEntry *entry = &filetree.entries[filetree.selected_index];
        if (entry->is_directory) {
                char newpath[1024];
                if (strcmp(entry->name, "..") == 0) {
                        char *last_slash = strrchr(filetree.current_path, '/');
                        if (last_slash && last_slash != filetree.current_path) {
                                *last_slash = '\0';
                        } else {
                                strcpy(filetree.current_path, "..");
                        }
                        snprintf(newpath, sizeof(newpath), "%s", filetree.current_path);
                } else {
                        snprintf(newpath, sizeof(newpath), "%s/%s", filetree.current_path, entry->name);
                }
                char abspath[1024];
                if (realpath(newpath, abspath)) {
                        free(filetree.current_path);
                        filetree.current_path = strdup(abspath);
                        load_directory(filetree.current_path);
                }
        } else {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", filetree.current_path, entry->name);
                browse_mode = 0;
                free_filetree();
                load_file(filepath);
        }
}

void filetree_go_parent(void)
{
        char newpath[1024];
        char *last_slash = strrchr(filetree.current_path, '/');
        if (last_slash && last_slash != filetree.current_path) {
                *last_slash = '\0';
                snprintf(newpath, sizeof(newpath), "%s", filetree.current_path);
        } else {
                snprintf(newpath, sizeof(newpath), "..");
        }
        char abspath[1024];
        if (realpath(newpath, abspath)) {
                free(filetree.current_path);
                filetree.current_path = strdup(abspath);
                load_directory(filetree.current_path);
        }
}
