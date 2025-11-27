#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "estruct.h"
#include "efunc.h"
#include "util.h"

int read_esc_sequence(void)
{
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
                return 0;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
                return 0;
        if (seq[0] == '[') {
                if (seq[1] == '1') {
                        char extra[3];
                        if (read(STDIN_FILENO, &extra[0], 1) != 1)
                                return 0;
                        if (extra[0] == ';') {
                                if (read(STDIN_FILENO, &extra[1], 1) != 1)
                                        return 0;
                                if (extra[1] == '5') {
                                        if (read(STDIN_FILENO, &extra[2], 1) != 1)
                                                return 0;
                                        char direction = extra[2];
                                        if (!search_mode) {
                                                if (browse_mode) {
                                                        switch (direction) {
                                                        case KEY_ARROW_UP:
                                                                for (int i = 0;
                                                                     i < 3; i++)
                                                                        filetree_move_up();
                                                                break;
                                                        case KEY_ARROW_DOWN:
                                                                for (int i = 0;
                                                                     i < 3; i++)
                                                                        filetree_move_down();
                                                                break;
                                                        case KEY_ARROW_LEFT:
                                                                break;
                                                        case KEY_ARROW_RIGHT:
                                                                break;
                                                        }
                                                } else {
                                                        switch (direction) {
                                                        case KEY_ARROW_UP:
                                                                editor.cursor_y = (editor.cursor_y >= 3) ? editor.cursor_y - 3 : 0;
                                                                break;
                                                        case KEY_ARROW_DOWN:
                                                                editor.cursor_y += 3;
                                                                if (editor.cursor_y >= editor.line_numbers)
                                                                        editor.cursor_y = editor.line_numbers - 1;
                                                                break;
                                                        case KEY_ARROW_RIGHT:{
                                                                        int line_len = strlen(editor.lines[editor.cursor_y]);
                                                                        editor.cursor_x += 3;
                                                                        if (editor.cursor_x > line_len)
                                                                                editor.cursor_x = line_len;
                                                                        break;
                                                                }
                                                        case KEY_ARROW_LEFT:
                                                                editor.cursor_x = (editor.cursor_x >= 3) ? editor.cursor_x - 3 : 0;
                                                                break;
                                                        }
                                                }
                                        }
                                }
                        }
                } else {
                        char direction = seq[1];
                        if (!search_mode) {
                                if (browse_mode) {
                                        switch (direction) {
                                        case KEY_ARROW_UP:
                                                filetree_move_up();
                                                break;
                                        case KEY_ARROW_DOWN:
                                                filetree_move_down();
                                                break;
                                        case KEY_ARROW_LEFT:
                                                break;
                                        case KEY_ARROW_RIGHT:
                                                break;
                                        }
                                } else {
                                        switch (direction) {
                                        case KEY_ARROW_UP:
                                                if (editor.cursor_y > 0)
                                                        editor.cursor_y--;
                                                break;
                                        case KEY_ARROW_DOWN:
                                                if (editor.cursor_y < editor.line_numbers - 1)
                                                        editor.cursor_y++;
                                                break;
                                        case KEY_ARROW_RIGHT:{
                                                        int line_len = strlen(editor.lines[editor.cursor_y]);
                                                        if (editor.cursor_x < line_len)
                                                                editor.cursor_x++;
                                                        break;
                                                }
                                        case KEY_ARROW_LEFT:
                                                if (editor.cursor_x > 0)
                                                        editor.cursor_x--;
                                                break;
                                        }
                                }
                        } else {
                                switch (direction) {
                                case KEY_ARROW_UP:
                                        search_move_up();
                                        break;
                                case KEY_ARROW_DOWN:
                                        search_move_down();
                                        break;
                                case KEY_ARROW_LEFT:
                                        break;
                                case KEY_ARROW_RIGHT:
                                        break;
                                }
                        }
                }
        }
        if (!search_mode && !browse_mode) {
                int len = strlen(editor.lines[editor.cursor_y]);
                if (editor.cursor_x > len)
                        editor.cursor_x = len;
                int rows = get_window_rows() - 2;
                if (editor.cursor_y < editor.offset_y)
                        editor.offset_y = editor.cursor_y;
                if (editor.cursor_y >= editor.offset_y + rows)
                        editor.offset_y = editor.cursor_y - rows + 1;
        }
        return 1;
}

int process_keypress(char c)
{
        if (c == KEY_CTRL_Q) {
                return 0;
        }
        if (c == KEY_CTRL_F) {
                if (search_mode) {
                        return 1;
                } else {
                        search_mode = 1;
                        init_search();
                        return 1;
                }
        }
        if (c == KEY_CTRL_P) {
                if (browse_mode) {
                        return 1;
                } else {
                        browse_mode = 1;
                        init_filetree(".");
                        return 1;
                }
        }
        if (search_mode) {
                if (c == KEY_ENTER) {
                        search_select();
                } else if (c == KEY_BACKSPACE) {
                        search_remove_char();
                } else if (c == KEY_ESC) {
                        read_esc_sequence();
                } else if (!iscntrl(c)) {
                        search_add_char(c);
                }
                return 1;
        }
        if (browse_mode) {
                if (c == KEY_ENTER) {
                        filetree_select();
                } else if (c == '-') {
                        filetree_go_parent();
                } else if (c == KEY_ESC) {
                        read_esc_sequence();
                }
                return 1;
        }
        if (c == KEY_CTRL_S) {
                save_file();
        } else if (c == KEY_TAB) {
                indent_line();
        } else if (c == KEY_BACKSPACE) {
                delete_char();
        } else if (c == KEY_ENTER) {
                insert_newline();
        } else if (c == KEY_ESC) {
                read_esc_sequence();
        } else if (!iscntrl(c)) {
                insert_char(c);
        }
        return 1;
}
