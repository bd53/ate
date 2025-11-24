#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"
#include "command.h"
#include "content.h"
#include "display.h"
#include "file.h"
#include "keybinds.h"
#include "search.h"
#include "tree.h"
#include "utf8.h"
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
            case '3':
                if (read(STDIN_FILENO, &seq[2], 1) == 1) {
                    if (seq[2] == ';') {
                        if (read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' &&
                            read(STDIN_FILENO, &seq[4], 1) == 1 && seq[4] == '~') {
                            return 1008; // CTRL_BACKSPACE
                        }
                    }
                }
                break;
        }
    }
    return '\x1b';
}

int input_read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) if (nread == -1 && errno != EAGAIN) die("read");
    return (c == '\x1b') ? read_esc_sequence() : c;
}

void cursor_move(int key) {
    Row *row = (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) ? &Editor.row[Editor.cursor_y] : NULL;
    Editor.found_row = -1;
    Editor.found_col = -1;
    switch (key) {
        case 1004:
            if (Editor.cursor_x > 0) {
                if (row) {
                    Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
                } else {
                    Editor.cursor_x--;
                }
            } else if (Editor.cursor_y > 0) {
                Editor.cursor_y--;
                if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) Editor.cursor_x = Editor.row[Editor.cursor_y].size;
            }
            break;
        case 1005:
            if (row && Editor.cursor_x < row->size) {
                Editor.cursor_x = utf8_next_char_boundary(row->chars, Editor.cursor_x, row->size);
            } else if (row && Editor.cursor_x == row->size && Editor.cursor_y < Editor.buffer_rows - 1) {
                Editor.cursor_y++;
                Editor.cursor_x = 0;
            }
            break;
        case 1002:
            if (Editor.cursor_y > 0) Editor.cursor_y--;
            break;
        case 1003:
            if (Editor.cursor_y < Editor.buffer_rows - 1) Editor.cursor_y++;
            break;
        case 1000:
            Editor.cursor_x = 0;
            break;
        case 1001:
            if (row) Editor.cursor_x = row->size;
            break;
        case 1006:
        case 1007: {
            int jump = (Editor.editor_rows / 7 < 1) ? 1 : Editor.editor_rows / 7;
            Editor.cursor_y += (key == 1006) ? -jump : jump;
            if (Editor.cursor_y < 0) Editor.cursor_y = 0;
            if (Editor.cursor_y >= Editor.buffer_rows) Editor.cursor_y = Editor.buffer_rows - 1;
            break;
        }
    }
    row = (Editor.buffer_rows > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) ? &Editor.row[Editor.cursor_y] : NULL;
    int len = row ? row->size : 0;
    if (Editor.cursor_x > len) Editor.cursor_x = len;
    if (row && Editor.cursor_x > 0 && Editor.cursor_x < row->size) {
        if (!utf8_is_char_boundary(row->chars, Editor.cursor_x)) Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
    }
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
    free_workspace_search();
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
            Editor.mode = MODE_COMMAND;
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
            Editor.help_view = 0;
            if (Editor.buffer_rows == 0) append_row("", 0);
            break;
        case ':':
            Editor.mode = MODE_COMMAND;
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
            Editor.mode = MODE_COMMAND;
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
            toggle_workspace_find();
            return;
        case CTRL_KEY('s'):
            save_file();
            return;
        case 'n':
            workspace_find_next(1);
            return;
        case 'b':
            workspace_find_next(-1);
            return;
        case 'i':
            Editor.mode = MODE_INSERT;
            break;
        case 'y':
            yank_line();
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            cursor_move(translate_key(c));
            break;
        case 'd':
            delete_line();
            break;
    }
}

static void handle_insert_mode(int c) {
    switch (c) {
        case '\r':
            Editor.modified = 1;
            insert_new_line();
            break;
        case '\t':
            Editor.modified = 1;
            for (int i = 0; i < 4; i++) insert_character(' ');
            break;
        case 127:
            delete_character();
            break;
        case 8:
        case 1008:
            delete_line();
            break;
        default:
            if (c >= 32 && c < 127) {
                Editor.modified = 1;
                insert_character(c);
            } else if (c >= 128) {
                char utf8_buf[5];
                utf8_buf[0] = (char)c;
                int char_len = 1;
                unsigned char uc = (unsigned char)c;
                if ((uc & 0xE0) == 0xC0) char_len = 2;
                else if ((uc & 0xF0) == 0xE0) char_len = 3;
                else if ((uc & 0xF8) == 0xF0) char_len = 4;
                for (int i = 1; i < char_len; i++) {
                    int next_byte = input_read_key();
                    if (next_byte < 0x80 || next_byte > 0xBF) return;
                    utf8_buf[i] = (char)next_byte;
                }
                utf8_buf[char_len] = '\0';
                Editor.modified = 1;
                insert_utf8_character(utf8_buf, char_len);
            }
            break;
    }
}

void process_keypress() {
    int c = input_read_key();
    if (Editor.file_tree) {
        handle_file_tree(c);
        return;
    }
    if (Editor.help_view) {
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
        case 8:
            if (Editor.mode == MODE_INSERT) {
                delete_line();
            } else if (Editor.mode == MODE_NORMAL) {
                open_help();
            }
            break;
        case 1008:
            if (Editor.mode == MODE_INSERT) delete_line();
            break;
        case '\x1b':
            if (Editor.mode == MODE_INSERT) {
                Editor.mode = MODE_NORMAL;
                if (Editor.cursor_x > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                    Row *row = &Editor.row[Editor.cursor_y];
                    Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
                }
            } else if (Editor.mode == MODE_COMMAND) {
                Editor.mode = MODE_NORMAL;
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
            if (Editor.mode == MODE_NORMAL) {
                handle_normal_mode(c);
            } else if (Editor.mode == MODE_INSERT) {
                handle_insert_mode(c);
            }
            break;
    }
    refresh_screen();
}
