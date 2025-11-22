#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "display.h"
#include "keybinds.h"
#include "utils.h"

void editorScroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
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
