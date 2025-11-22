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
#include "editor.h"
#include "health.h"
#include "keybinds.h"
#include "utils.h"

static FileEntry *file_entries = NULL;
static int num_entries = 0;
static int entries_capacity = 0;

void editorFreeRows() {
    for (int j = 0; j < E.numrows; j++) {
        free(E.row[j].chars);
        E.row[j].chars = NULL;
        free(E.row[j].hl);
        E.row[j].hl = NULL;
    }
    free(E.row);
    E.row = NULL;
    E.numrows = 0;
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
}

static void editorFreeFileEntries() {
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

void editorCleanup() {
    if (E.query) {
        free(E.query);
        E.query = NULL;
    }
    editorFreeRows();
    editorFreeFileEntries();
}

void editorContentInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void editorContentAppendRow(char *s, size_t len) {
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    erow *row = &E.row[at];
    free(row->chars);
    row->chars = NULL;
    free(row->hl);
    row->hl = NULL;
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty = 1;
    if (E.numrows == 0) {
        E.cy = 0;
    } else if (E.cy >= E.numrows) {
        E.cy = E.numrows - 1;
    }
    if (E.cy < 0) {
        E.cy = 0;
    }
}

void editorContentInsertChar(char c) {
    if (E.cy == E.numrows) {
        editorContentAppendRow("", 0);
    }
    erow *row = &E.row[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > row->size) E.cx = row->size;
    char *new = realloc(row->chars, row->size + 2);
    if (new == NULL) die("realloc");
    row->chars = new;
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->hl, new_hl_size);
    if (new_hl == NULL) die("realloc");
    row->hl = new_hl;
    E.cx++;
    E.dirty = 1;
}

void editorContentInsertNewline() {
    if (E.cy == E.numrows) {
        editorContentAppendRow("", 0);
    } else {
        erow *row = &E.row[E.cy];
        int split_at = E.cx;
        if (split_at < 0) split_at = 0;
        if (split_at > row->size) split_at = row->size;
        int second_half_len = row->size - split_at;
        char *second_half = malloc(second_half_len + 1);
        if (second_half == NULL) die("malloc");
        memcpy(second_half, &row->chars[split_at], second_half_len);
        second_half[second_half_len] = '\0';
        row->size = split_at;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars == NULL) {
            free(second_half);
            die("realloc");
        }
        row->chars = new_chars;
        row->chars[row->size] = '\0';
        int hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->hl, hl_size);
        if (new_hl == NULL) {
            free(second_half);
            die("realloc");
        }
        row->hl = new_hl;
        editorContentInsertRow(E.cy + 1, second_half, second_half_len);
        free(second_half);
    }
    E.cy++;
    E.cx = 0;
    E.dirty = 1;
}

int editorOpen(char *filename) {
    if (filename == NULL) return -1;
    char *new_filename = strdup(filename);
    if (new_filename == NULL) die("strdup");
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        editorFreeRows();
        editorFreeFileEntries();
        if (E.query) {
            free(E.query);
            E.query = NULL;
        }
        E.filename = new_filename;
        E.is_help_view = 0;
        E.is_file_tree = 0;
        editorContentAppendRow("", 0);
        return -1;
    }
    editorFreeRows();
    editorFreeFileEntries();
    if (E.query) {
        free(E.query);
        E.query = NULL;
    }
    E.filename = new_filename;
    E.is_help_view = 0;
    E.is_file_tree = 0;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        editorContentAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
    return 0;
}

void editorSave() {
    if (E.filename == NULL) {
        char *filename = editorPrompt("Save as: %s (ESC to cancel)");
        if (filename == NULL) {
            editorRefreshScreen();
            return;
        }
        E.filename = filename;
    }
    FILE *fp = fopen(E.filename, "w");
    if (fp == NULL) {
        editorRefreshScreen();
        return;
    }
    for (int i = 0; i < E.numrows; i++) {
        fwrite(E.row[i].chars, 1, E.row[i].size, fp);
        fputc('\n', fp);
    }
    if (fclose(fp) == EOF) {
        editorRefreshScreen();
        return;
    }
    E.dirty = 0;
    editorRefreshScreen();
}

void editorOpenFile() {
    char *filename = editorPrompt("Open file: %s (ESC to cancel)");
    if (!filename) {
        editorRefreshScreen();
        return;
    }
    if (E.is_file_tree) {
        editorFreeFileEntries();
    }
    E.is_file_tree = 0;
    E.is_help_view = 0;
    int result = editorOpen(filename);
    free(filename);
    if (E.numrows == 0) {
        editorContentAppendRow("", 0);
    }
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    editorRefreshScreen();
}

void editorOpenHelp() {
    if (E.is_help_view) {
        editorCleanup();
        E.is_help_view = 0;
        E.is_file_tree = 0;
        if (E.numrows == 0) {
            editorContentAppendRow("", 0);
        }
    } else {
        editorCleanup();
        int result = editorOpen("help.txt");
        if (E.numrows == 0) {
            editorContentAppendRow("", 0);
        }
        E.cx = 0;
        E.cy = 0;
        E.rowoff = 0;
        E.is_help_view = 1;
        E.is_file_tree = 0;
    }
    editorRefreshScreen();
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
        int result = editorOpen(entry->path);
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

char *editorPrompt(const char *prompt) {
    if (prompt == NULL) return NULL;
    size_t bufsize = 256;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    size_t buflen = 0;
    buf[0] = '\0';
    int prompt_row = E.screenrows + 2;
    char static_prompt[256];
    strncpy(static_prompt, prompt, sizeof(static_prompt) - 1);
    static_prompt[sizeof(static_prompt) - 1] = '\0';
    char *format_pos = strstr(static_prompt, "%s");
    if (format_pos) {
        *format_pos = '\0';
    }
    int prompt_len = strlen(static_prompt);
    while(1) {
        struct buffer ab = BUFFER_INIT;
        abInit(&ab);
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        abAppend(&ab, pos_buf, strlen(pos_buf));
        abAppend(&ab, "\x1b[K", 3);
        abAppend(&ab, static_prompt, prompt_len);
        abAppend(&ab, buf, buflen);
        int cursor_col = prompt_len + buflen + 1;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", prompt_row, cursor_col);
        abAppend(&ab, pos_buf, strlen(pos_buf));
        if (write(STDOUT_FILENO, ab.b, ab.len) == -1) {
            abFree(&ab);
            free(buf);
            die("write");
        }
        abFree(&ab);
        int c = inputReadKey();
        if (c == '\r') {
            if (buflen > 0) {
                return buf;
            }
            free(buf);
            return NULL;
        } else if (c == '\x1b') {
            free(buf);
            return NULL;
        } else if (c == 127 || c == CTRL_KEY('h')) {
            if (buflen > 0) {
                buflen--;
                buf[buflen] = '\0';
            }
        } else if (c >= 32 && c < 127) {
            if (buflen < bufsize - 1) {
                buf[buflen++] = c;
                buf[buflen] = '\0';
            } else {
                bufsize *= 2;
                char *new_buf = realloc(buf, bufsize);
                if (new_buf == NULL) {
                    free(buf);
                    die("realloc");
                }
                buf = new_buf;
                buf[buflen++] = c;
                buf[buflen] = '\0';
            }
        }
    }
}

void editorScroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
}

void editorFindNext(int direction) {
    if (E.query == NULL || strlen(E.query) == 0) return;
    E.match_row = -1;
    E.match_col = -1;
    if (E.numrows == 0) return;
    int start_row = E.search_start_row;
    int start_col = E.search_start_col;
    if (direction == 1) {
        for (int i = 0; i < E.numrows; i++) {
            int r = (start_row + i) % E.numrows;
            if (r < 0 || r >= E.numrows) break;
            erow *row = &E.row[r];
            int search_offset = (r == start_row) ? start_col + 1 : 0;
            if (search_offset < 0) search_offset = 0;
            if (search_offset >= row->size) continue;
            char *match_ptr = strstr(row->chars + search_offset, E.query);
            if (match_ptr) {
                int match_col = match_ptr - row->chars;
                E.cy = r;
                E.cx = match_col;
                E.match_row = r;
                E.match_col = match_col;
                E.search_start_row = r;
                E.search_start_col = match_col;
                editorScroll();
                editorRefreshScreen();
                return;
            }
        }
    }
    else {
        for (int i = 0; i < E.numrows; i++) {
            int r = start_row - i;
            if (r < 0) r = E.numrows - 1;
            if (r < 0 || r >= E.numrows) break;
            erow *row = &E.row[r];
            if (row->chars == NULL) continue;
            int best_match_col = -1;
            char *p = row->chars;
            while (1) {
                char *match_ptr = strstr(p, E.query);
                if (match_ptr == NULL) break;
                int match_col = match_ptr - row->chars;
                if (r != start_row || match_col < start_col) {
                    best_match_col = match_col;
                }
                p = match_ptr + 1;
                if (p >= row->chars + row->size) break;
            }
            if (best_match_col != -1) {
                E.cy = r;
                E.cx = best_match_col;
                E.match_row = r;
                E.match_col = best_match_col;
                E.search_start_row = r;
                E.search_start_col = best_match_col;
                editorScroll();
                editorRefreshScreen();
                return;
            }
        }
    }
}

void editorFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    E.match_row = -1;
    E.match_col = -1;
    char *query = editorPrompt("Find text: %s (ESC to cancel)");
    if (query == NULL) {
        E.cx = saved_cx;
        E.cy = saved_cy;
        editorRefreshScreen();
        return;
    }
    if (E.query) free(E.query);
    E.query = query;
    E.search_start_row = saved_cy;
    E.search_start_col = saved_cx;
    editorFindNext(1);
    editorRefreshScreen();
}

void editorViewDrawContent(struct buffer *ab) {
    if (ab == NULL) return;
    int file_content_rows = E.screenrows;
    int max_num = E.numrows > file_content_rows ? E.numrows : file_content_rows;
    E.gutter_width = 1;
    if (max_num + 1 > 9) E.gutter_width = 2;
    if (max_num + 1 > 99) E.gutter_width = 3;
    if (max_num + 1 > 999) E.gutter_width = 4;
    E.gutter_width += 1;
    if (E.is_file_tree) {
        E.gutter_width = 0;
    }
    for (int y = 0; y < file_content_rows; y++) {
        int filerow = y + E.rowoff;
        char line_num_buf[32];
        int line_num_len;
        if (!E.is_file_tree) {
            abAppend(ab, "\x1b[38;5;244m", 11);
            if (filerow >= E.numrows) {
                line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s", E.gutter_width, "~");
                abAppend(ab, line_num_buf, line_num_len);
            } else {
                if (E.mode == MODE_NORMAL) {
                    if (filerow == E.cy) {
                        abAppend(ab, "\x1b[33m", 5);
                        line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*s ", E.gutter_width - 1, ">>");
                    } else {
                        int rel_num = abs(filerow - E.cy);
                        line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*d ", E.gutter_width - 1, rel_num);
                    }
                } else {
                    int abs_num = filerow + 1;
                    line_num_len = snprintf(line_num_buf, sizeof(line_num_buf), "%*d ", E.gutter_width - 1, abs_num);
                }
                abAppend(ab, line_num_buf, line_num_len);
            }
            abAppend(ab, "\x1b[m", 3);
        }
        if (filerow < E.numrows) {
            erow *row = &E.row[filerow];
            int current_hl = HL_NORMAL;
            int content_width = E.screencols - E.gutter_width;
            int len_to_draw = row->size;
            if (len_to_draw > content_width) len_to_draw = content_width;
            if (E.is_file_tree) {
                if (filerow < 3) {
                    abAppend(ab, "\x1b[34m", 5);
                } else if (filerow == E.cy) {
                    abAppend(ab, "\x1b[7m", 4);
                }
            }
            for (int i = 0; i < len_to_draw; i++) {
                char c = row->chars[i];
                if (!E.is_file_tree) {
                    int hl = row->hl[i];
                    int match_col = E.match_col;
                    int query_len = E.query ? strlen(E.query) : 0;
                    if (E.query && filerow == E.match_row && i >= match_col && i < match_col + query_len) {
                        hl = HL_MATCH;
                    }
                    if (hl != current_hl) {
                        current_hl = hl;
                        char *color_code = (hl == HL_MATCH) ? "\x1b[45m" : "\x1b[0m";
                        abAppend(ab, color_code, strlen(color_code));
                    }
                }
                abAppend(ab, &c, 1);
            }
            abAppend(ab, "\x1b[m", 3);
        }
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorViewDrawStatusBar(struct buffer *ab) {
    if (ab == NULL) return;
    abAppend(ab, "\x1b[7m", 4);
    char status[80];
    const char *filetype_name = "txt";
    char filename_status[64];
    if (E.is_file_tree) {
        snprintf(filename_status, sizeof(filename_status), "netft");
    } else {
        snprintf(filename_status, sizeof(filename_status), "%s%s", E.filename ? E.filename : "[No Name]", E.dirty ? " *" : "");
    }
    const char *mode_str;
    if (E.mode == MODE_NORMAL) mode_str = "NORMAL";
    else if (E.mode == MODE_INSERT) mode_str = "INSERT";
    else if (E.mode == MODE_COMMAND) mode_str = "COMMAND";
    else mode_str = "UNKNOWN";
    int len = snprintf(status, sizeof(status), " %s | %s | R:%d L:%d", mode_str, filename_status, E.cy + 1, E.numrows > 0 ? E.numrows : 1);
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    char rstatus[80];
    int rlen = snprintf(rstatus, sizeof(rstatus), " %s | C:%d ", filetype_name, E.cx + 1); 
    while (len < E.screencols - rlen) {
        abAppend(ab, " ", 1);
        len++;
    }
    abAppend(ab, rstatus, rlen);
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorRefreshScreen() {
    editorScroll();
    struct buffer ab = BUFFER_INIT;
    abInit(&ab);
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);
    editorViewDrawContent(&ab);
    editorViewDrawStatusBar(&ab);
    char buf[32];
    int cur_y = (E.cy - E.rowoff) + 1;
    int cur_x = E.cx + 1 + E.gutter_width;
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cur_y, cur_x);
    abAppend(&ab, buf, strlen(buf));
    if (E.mode == MODE_INSERT) {
        abAppend(&ab, "\x1b[5 q", 5);
    } else {
        abAppend(&ab, "\x1b[2 q", 5);
    }
    abAppend(&ab, "\x1b[?25h", 6);
    if (write(STDOUT_FILENO, ab.b, ab.len) == -1) die("write");
    abFree(&ab);
}

void editorExecuteCommand(char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    cmd = trim_whitespace(cmd);
    if (cmd == NULL || strlen(cmd) == 0) return;
    if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        editorCleanup();
        exit(0);
    }
    else if (strcmp(cmd, "Ex") == 0) {
        editorToggleFileTree();
    }
    else if (strcmp(cmd, "bd") == 0) {
        if (E.is_file_tree) {
            editorToggleFileTree();
        }
    }
    else if (strncmp(cmd, "w", 1) == 0) {
        char *arg = NULL;
        if (strcmp(cmd, "w") == 0 || strcmp(cmd, "write") == 0) {
            arg = NULL;
        } else {
            if (strncmp(cmd, "w ", 2) == 0) arg = cmd + 2;
            else if (strncmp(cmd, "write ", 6) == 0) arg = cmd + 6;
        }
        if (arg) {
            arg = trim_whitespace(arg);
            if (arg && *arg) {
                char *new_filename = strdup(arg);
                if (!new_filename) die("strdup");
                if (E.filename) free(E.filename);
                E.filename = new_filename;
            }
        }
        if (E.filename == NULL) {
            char *filename = editorPrompt("Save as: %s (ESC to cancel)");
            if (!filename) {
                editorRefreshScreen();
                return;
            }
            E.filename = filename;
        }
        editorSave();
    }
    else if (strcmp(cmd, "help") == 0) {
        editorOpenHelp();
    }
    else if (strcmp(cmd, "checkhealth") == 0) {
        editorCheckHealth();
    }
}

void editorCommandMode() {
    size_t bufsize = 256;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    size_t buflen = 0;
    buf[0] = '\0';
    int prompt_row = E.screenrows + 2;
    while(1) {
        editorScroll();
        struct buffer ab = BUFFER_INIT;
        abInit(&ab);
        abAppend(&ab, "\x1b[?25l", 6);
        abAppend(&ab, "\x1b[H", 3);
        editorViewDrawContent(&ab);
        editorViewDrawStatusBar(&ab);
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        abAppend(&ab, pos_buf, strlen(pos_buf));
        abAppend(&ab, "\x1b[K", 3);
        abAppend(&ab, ":", 1);
        abAppend(&ab, buf, buflen);
        int cursor_col = buflen + 2;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", prompt_row, cursor_col);
        abAppend(&ab, pos_buf, strlen(pos_buf));
        abAppend(&ab, "\x1b[?25h", 6);
        if (write(STDOUT_FILENO, ab.b, ab.len) == -1) {
            abFree(&ab);
            free(buf);
            die("write");
        }
        abFree(&ab);
        int c = inputReadKey();
        if (c == '\r') {
            E.mode = MODE_NORMAL;
            if (buflen > 0) {
                editorExecuteCommand(buf);
            }
            free(buf);
            editorRefreshScreen();
            return;
        } else if (c == '\x1b') {
            E.mode = MODE_NORMAL;
            free(buf);
            editorRefreshScreen();
            return;
        } else if (c == 127 || c == CTRL_KEY('h')) {
            if (buflen > 0) {
                buflen--;
                buf[buflen] = '\0';
            }
        } else if (c >= 32 && c < 127) {
            if (buflen < bufsize - 1) {
                buf[buflen++] = c;
                buf[buflen] = '\0';
            } else {
                bufsize *= 2;
                char *new_buf = realloc(buf, bufsize);
                if (new_buf == NULL) {
                    free(buf);
                    die("realloc");
                }
                buf = new_buf;
                buf[buflen++] = c;
                buf[buflen] = '\0';
            }
        }
    }
}

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
