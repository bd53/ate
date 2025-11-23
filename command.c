#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "common.h"
#include "command.h"
#include "content.h"
#include "display.h"
#include "file.h"
#include "health.h"
#include "keybinds.h"
#include "tree.h"
#include "utils.h"

void open_help() {
    if (E.is_help_view) {
        run_cleanup();
        E.is_help_view = 0;
        E.is_file_tree = 0;
        if (E.numrows == 0) {
            append_row("", 0);
        }
    } else {
        run_cleanup();
        open_editor("help.txt");
        if (E.numrows == 0) {
            append_row("", 0);
        }
        E.cx = 0;
        E.cy = 0;
        E.rowoff = 0;
        E.is_help_view = 1;
        E.is_file_tree = 0;
    }
    refresh_screen();
}

void execute_command(char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    cmd = trim_whitespace(cmd);
    if (cmd == NULL || strlen(cmd) == 0) return;
    if (strcmp(cmd, "q") == 0) {
        run_cleanup();
        E.is_file_tree = 0;
        E.is_help_view = 0;
        if (E.numrows == 0) {
            append_row("", 0);
        }
        E.cx = 0;
        E.cy = 0;
        E.rowoff = 0;
        E.dirty = 0;
        refresh_screen();
        return;
    }
    else if (strcmp(cmd, "quit") == 0) {
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        run_cleanup();
        exit(0);
    }
    else if (strcmp(cmd, "Ex") == 0) {
        toggle_file_tree();
    }
    else if (strcmp(cmd, "bd") == 0) {
        if (E.is_file_tree) {
            toggle_file_tree();
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
                if (E.filename) {
                    free(E.filename);
                    E.filename = NULL;
                }
                E.filename = new_filename;
            }
        }
        if (E.filename == NULL) {
            char *filename = prompt("Save as: %s (ESC to cancel)");
            if (!filename) {
                refresh_screen();
                return;
            }
            E.filename = filename;
        }
        save_editor();
    }
    else if (strcmp(cmd, "help") == 0) {
        open_help();
    }
    else if (strcmp(cmd, "checkhealth") == 0) {
        check_health();
    }
    else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "\x1b[31mE182: Not an editor command: '%s'\x1b[0m", cmd);
        int prompt_row = E.screenrows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, error_msg, strlen(error_msg));
        input_read_key();
        refresh_screen();
    }
}

void command_mode() {
    size_t bufsize = 256;
    char *buf = malloc(bufsize);
    if (!buf) die("malloc");
    size_t buflen = 0;
    buf[0] = '\0';
    int prompt_row = E.screenrows + 2;
    while(1) {
        scroll_editor();
        struct buffer ab = BUFFER_INIT;
        abinit(&ab);
        abappend(&ab, "\x1b[?25l", 6);
        abappend(&ab, "\x1b[H", 3);
        draw_content(&ab);
        draw_status(&ab);
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        abappend(&ab, pos_buf, strlen(pos_buf));
        abappend(&ab, "\x1b[K", 3);
        abappend(&ab, ":", 1);
        abappend(&ab, buf, buflen);
        int cursor_col = buflen + 2;
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;%dH", prompt_row, cursor_col);
        abappend(&ab, pos_buf, strlen(pos_buf));
        abappend(&ab, "\x1b[?25h", 6);
        if (write(STDOUT_FILENO, ab.b, ab.len) == -1) {
            abfree(&ab);
            free(buf);
            die("write");
        }
        abfree(&ab);
        int c = input_read_key();
        if (c == '\r') {
            E.mode = MODE_NORMAL;
            if (buflen > 0) {
                execute_command(buf);
            }
            free(buf);
            refresh_screen();
            return;
        } else if (c == '\x1b') {
            E.mode = MODE_NORMAL;
            free(buf);
            refresh_screen();
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