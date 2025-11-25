#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "utf8.h"
#include "util.h"

static int read_esc_sequence(void) {
    char seq[5];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESCAPE;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESCAPE;
    if (seq[0] == '[' && seq[1] == '1') {
        if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == ';' && read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1) {
            switch (seq[4]) {
                case 'A': return KEY_CTRL_ARROW_UP;
                case 'B': return KEY_CTRL_ARROW_DOWN;
                case 'C': return KEY_CTRL_ARROW_RIGHT;
                case 'D': return KEY_CTRL_ARROW_LEFT;
            }
        }
    }
    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return KEY_ARROW_UP;
            case 'B': return KEY_ARROW_DOWN;
            case 'C': return KEY_ARROW_RIGHT;
            case 'D': return KEY_ARROW_LEFT;
            case '3':
                if (read(STDIN_FILENO, &seq[2], 1) == 1) {
                    if (seq[2] == ';') {
                        if (read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1 && seq[4] == '~') {
                            return KEY_CTRL_BACKSPACE;
                        }
                    }
                }
                break;
        }
    }
    return KEY_ESCAPE;
}

int input_read_key(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) if (nread == -1 && errno != EAGAIN) die("read");
    return (c == KEY_ESCAPE) ? read_esc_sequence() : c;
}

void cursor_move(int key) {
    struct Row *row = (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) ? &Editor.row[Editor.cursor_y] : NULL;
    Editor.found_row = -1;
    Editor.found_col = -1;
    switch (key) {
        case KEY_ARROW_LEFT:
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
        case KEY_ARROW_RIGHT:
            if (row && Editor.cursor_x < row->size) {
                Editor.cursor_x = utf8_next_char_boundary(row->chars, Editor.cursor_x, row->size);
            } else if (row && Editor.cursor_x == row->size && Editor.cursor_y < Editor.buffer_rows - 1) {
                Editor.cursor_y++;
                Editor.cursor_x = 0;
            }
            break;
        case KEY_ARROW_UP:
            if (Editor.cursor_y > 0) Editor.cursor_y--;
            break;
        case KEY_ARROW_DOWN:
            if (Editor.cursor_y < Editor.buffer_rows - 1) Editor.cursor_y++;
            break;
        case KEY_CTRL_ARROW_LEFT:
            Editor.cursor_x = 0;
            break;
        case KEY_CTRL_ARROW_RIGHT:
            if (row) Editor.cursor_x = row->size;
            break;
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN: {
            int jump = (Editor.editor_rows / 7 < 1) ? 1 : Editor.editor_rows / 7;
            Editor.cursor_y += (key == KEY_CTRL_ARROW_UP) ? -jump : jump;
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
        case KEY_MOVE_LEFT: return KEY_ARROW_LEFT;
        case KEY_MOVE_DOWN: return KEY_ARROW_DOWN;
        case KEY_MOVE_UP: return KEY_ARROW_UP;
        case KEY_MOVE_RIGHT: return KEY_ARROW_RIGHT;
        default: return key;
    }
}

static void handle_quit(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    free_workspace_search();
    run_cleanup();
    exit(0);
}

static void handle_file_tree(int c) {
    switch (c) {
        case KEY_QUIT:
            handle_quit();
            break;
        case KEY_TOGGLE_FILE_TREE:
        case KEY_ESCAPE:
        case 'q':
            toggle_file_tree();
            return;
        case KEY_COMMAND_MODE:
            Editor.mode = 2;
            command_mode();
            return;
        case KEY_ENTER:
            open_file_tree();
            return;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
            cursor_move(c);
            break;
        case KEY_MOVE_LEFT:
        case KEY_MOVE_DOWN:
        case KEY_MOVE_UP:
        case KEY_MOVE_RIGHT:
            cursor_move(translate_key(c));
            break;
    }
    refresh_screen();
}

static void handle_help(int c) {
    switch (c) {
        case KEY_QUIT:
            handle_quit();
            break;
        case KEY_ESCAPE:
        case 'q':
            run_cleanup();
            Editor.help_view = 0;
            if (Editor.buffer_rows == 0) append_row("", 0);
            break;
        case KEY_COMMAND_MODE:
            Editor.mode = 2;
            command_mode();
            return;
        case KEY_TOGGLE_HELP:
            display_help();
            return;
        case KEY_TOGGLE_FILE_TREE:
            toggle_file_tree();
            return;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
            cursor_move(c);
            break;
        case KEY_MOVE_LEFT:
        case KEY_MOVE_DOWN:
        case KEY_MOVE_UP:
        case KEY_MOVE_RIGHT:
            cursor_move(translate_key(c));
            break;
    }
    refresh_screen();
}

static void handle_normal_mode(int c) {
    switch (c) {
        case KEY_ENTER:
            break;
        case KEY_INSERT_MODE:
            Editor.mode = 1;
            break;
        case KEY_COMMAND_MODE:
            Editor.mode = 2;
            command_mode();
            return;
        case KEY_OPEN_FILE:
            open_file();
            return;
        case KEY_SAVE_FILE:
            save_file();
            return;
        case KEY_TOGGLE_HELP:
            display_help();
            return;
        case KEY_TOGGLE_FIND:
            toggle_workspace_find();
            return;
        case KEY_TOGGLE_TAGS:
             display_tags();
             return;
        case KEY_TOGGLE_FILE_TREE:
            toggle_file_tree();
            return;
        case KEY_FIND_NEXT:
            workspace_find_next(1);
            return;
        case KEY_FIND_PREV:
            workspace_find_next(-1);
            return;
        case KEY_YANK_LINE:
            yank_line();
            break;
        case KEY_DELETE_LINE:
            delete_line();
            break;
        case KEY_GO_TO_LINE:
            goto_line();
            return;
        case KEY_MOVE_LEFT:
        case KEY_MOVE_DOWN:
        case KEY_MOVE_UP:
        case KEY_MOVE_RIGHT:
            cursor_move(translate_key(c));
            break;
    }
}

static void handle_insert_mode(int c) {
    switch (c) {
        case KEY_ENTER:
            Editor.modified = 1;
            insert_new_line();
            break;
        case KEY_TAB:
            Editor.modified = 1;
            for (int i = 0; i < 4; i++) insert_character(' ');
            break;
        case KEY_BACKSPACE_ASCII:
            delete_character();
            break;
        case KEY_BACKSPACE_ALT:
        case KEY_CTRL_BACKSPACE:
            delete_line();
            break;
        default:
            if (c >= 32 && c < 127) {
                Editor.modified = 1;
                insert_character(c);
                if (c == '}' || c == ')' || c == ']') {
                    auto_dedent();
                }
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

void process_keypress(void) {
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
        case KEY_QUIT:
            handle_quit();
            break;
        case KEY_TOGGLE_TAGS:
            if (Editor.mode == 0) display_tags();
            break;
        case KEY_TOGGLE_FILE_TREE:
            if (Editor.mode == 0) toggle_file_tree();
            break;
        case KEY_BACKSPACE_ALT:
            if (Editor.mode == 1) {
                delete_line();
            } else if (Editor.mode == 0) {
                display_help();
            }
            break;
        case KEY_CTRL_BACKSPACE:
            if (Editor.mode == 1) delete_line();
            break;
        case KEY_ESCAPE:
            if (Editor.mode == 1) {
                Editor.mode = 0;
                if (Editor.cursor_x > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                    struct Row *row = &Editor.row[Editor.cursor_y];
                    Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
                }
            } else if (Editor.mode == 2) {
                Editor.mode = 0;
            }
            break;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
            cursor_move(c);
            break;
        default:
            if (Editor.mode == 0) {
                handle_normal_mode(c);
            } else if (Editor.mode == 1) {
                handle_insert_mode(c);
            }
            break;
    }
    refresh_screen();
}
