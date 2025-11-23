#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "command.h"
#include "display.h"
#include "file.h"
#include "init.h"
#include "keybinds.h"
#include "search.h"
#include "tree.h"
#include "utils.h"

static int read_esc_sequence() {
    char seq[5];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[' && seq[1] == '1') {
        if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == ';' && read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1) {
            switch (seq[4]) {
                case 'A': return 1006; // CTRL_ARROW_UP
                case 'B': return 1007; // CTRL_ARROW_DOWN
                case 'C': return 1001; // CTRL_ARROW_RIGHT
                case 'D': return 1000; // CTRL_ARROW_LEFT
            }
        }
    }
    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return 1002; // ARROW_UP
            case 'B': return 1003; // ARROW_DOWN
            case 'C': return 1005; // ARROW_RIGHT
            case 'D': return 1004; // ARROW_LEFT
        }
    }
    return '\x1b';
}

int input_read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return (c == '\x1b') ? read_esc_sequence() : c;
}

void cursor_move(int key) {
    erow *row = (E.cy >= 0 && E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    E.match_row = -1;
    E.match_col = -1;
    switch (key) {
        case 1004:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                if (E.cy >= 0 && E.cy < E.numrows) {
                    E.cx = E.row[E.cy].size;
                }
            }
            break;
        case 1005:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case 1002:
            if (E.cy > 0) E.cy--;
            break;
        case 1003:
            if (E.cy < E.numrows - 1) E.cy++;
            break;
        case 1000:
            E.cx = 0;
            break;
        case 1001:
            if (row) E.cx = row->size;
            break;
        case 1006:
        case 1007: {
            int jump = (E.screenrows / 7 < 1) ? 1 : E.screenrows / 7;
            E.cy += (key == 1006) ? -jump : jump;
            if (E.cy < 0) E.cy = 0;
            if (E.cy >= E.numrows) E.cy = E.numrows - 1;
            break;
        }
    }
    row = (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    int len = row ? row->size : 0;
    if (E.cx > len) E.cx = len;
}

static int translate_key(int key) {
    switch (key) {
        case 'h': return 1004;
        case 'j': return 1003;
        case 'k': return 1002;
        case 'l': return 1005;
        default: return key;
    }
}

static void handle_quit() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    run_cleanup();
    exit(0);
}

static void handle_file_tree(int c) {
    switch (c) {
        case CTRL_KEY('q'):
            handle_quit();
            break;
        case CTRL_KEY('p'):
            toggle_file_tree();
            return;
        case 'q':
        case '\x1b':
            toggle_file_tree();
            return;
        case ':':
            E.mode = MODE_COMMAND;
            command_mode();
            return;
        case '\r':
            open_file_tree();
            return;
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        case 1006:
        case 1007:
        case 1000:
        case 1001:
            cursor_move(c);
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            cursor_move(translate_key(c));
            break;
    }
    refresh_screen();
}

static void handle_help(int c) {
    switch (c) {
        case CTRL_KEY('q'):
            handle_quit();
            break;
        case 'q':
        case '\x1b':
            run_cleanup();
            E.is_help_view = 0;
            if (E.numrows == 0) {
                append_row("", 0);
            }
            break;
        case ':':
            E.mode = MODE_COMMAND;
            command_mode();
            return;
        case CTRL_KEY('h'):
            open_help();
            return;
        case CTRL_KEY('p'):
            toggle_file_tree();
            return;
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        case 1006:
        case 1007:
        case 1000:
        case 1001:
            cursor_move(c);
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            cursor_move(translate_key(c));
            break;
    }
    refresh_screen();
}

static void handle_normal_mode(int c) {
    switch (c) {
        case '\r':
            break;
        case ':':
            E.mode = MODE_COMMAND;
            command_mode();
            return;
        case CTRL_KEY('h'):
            open_help();
            return;
        case CTRL_KEY('p'):
            toggle_file_tree();
            return;
        case CTRL_KEY('o'):
            open_file();
            return;
        case CTRL_KEY('f'):
            toggle_find();
            return;
        case CTRL_KEY('s'):
            save_editor();
            return;
        case 'n':
            find_next(1);
            return;
        case 'N':
            find_next(-1);
            return;
        case 'i':
            E.mode = MODE_INSERT;
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            cursor_move(translate_key(c));
            break;
        case 'd':
            if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
                delete_row(E.cy);
                if (E.numrows == 0) {
                    E.cy = 0;
                } else if (E.cy >= E.numrows) {
                    E.cy = E.numrows - 1;
                }
                if (E.cy < 0) E.cy = 0;
                E.cx = 0;
            }
            break;
    }
}

static void handle_backspace() {
    if (E.cy < 0 || E.cy >= E.numrows) return;
    E.dirty = 1;
    if (E.cx > 0) {
        erow *row = &E.row[E.cy];
        E.cx--;
        memmove(&row->chars[E.cx], &row->chars[E.cx + 1], row->size - E.cx);
        row->size--;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars == NULL) die("realloc");
        row->chars = new_chars;
        row->chars[row->size] = '\0';
    } else if (E.cy > 0) {
        erow *prev_row = &E.row[E.cy - 1];
        erow *curr_row = &E.row[E.cy];
        E.cx = prev_row->size;
        char *new_chars = realloc(prev_row->chars, prev_row->size + curr_row->size + 1);
        if (new_chars == NULL) die("realloc");
        prev_row->chars = new_chars;
        memcpy(&prev_row->chars[prev_row->size], curr_row->chars, curr_row->size);
        prev_row->size += curr_row->size;
        prev_row->chars[prev_row->size] = '\0';
        free(curr_row->chars);
        curr_row->chars = NULL;
        free(curr_row->hl);
        curr_row->hl = NULL;
        memmove(&E.row[E.cy], &E.row[E.cy + 1], sizeof(erow) * (E.numrows - E.cy - 1));
        E.numrows--;
        E.cy--;
    }
}

static void handle_insert_mode(int c) {
    switch (c) {
        case '\r':
            E.dirty = 1;
            insert_new_line();
            break;
        case '\t':
            E.dirty = 1;
            for (int i = 0; i < 4; i++) {
                insert_character(' ');
            }
            break;
        case 127:
            handle_backspace();
            break;
        default:
            if (c >= 32 && c < 127) {
                E.dirty = 1;
                insert_character(c);
            }
            break;
    }
}

void process_keypress() {
    int c = input_read_key();
    if (E.is_file_tree) {
        handle_file_tree(c);
        return;
    }
    if (E.is_help_view) {
        handle_help(c);
        return;
    }
    switch (c) {
        case CTRL_KEY('q'):
            handle_quit();
            break;
        case CTRL_KEY('p'):
            toggle_file_tree();
            break;
        case '\x1b':
            if (E.mode == MODE_INSERT) {
                E.mode = MODE_NORMAL;
                if (E.cx > 0) E.cx--;
            } else if (E.mode == MODE_COMMAND) {
                E.mode = MODE_NORMAL;
            }
            break;
        case 1002:
        case 1003:
        case 1004:
        case 1005:
        case 1006:
        case 1007:
        case 1000:
        case 1001:
            cursor_move(c);
            break;
        default:
            if (E.mode == MODE_NORMAL) {
                handle_normal_mode(c);
            } else if (E.mode == MODE_INSERT) {
                handle_insert_mode(c);
            }
            break;
    }
    refresh_screen();
}