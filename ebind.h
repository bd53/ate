#ifndef EBIND_H
#define EBIND_H

#define CTRL_KEY(k) ((k) & 0x1f)

enum Codes {
    KEY_ARROW_LEFT = 1004,
    KEY_ARROW_RIGHT = 1005,
    KEY_ARROW_UP = 1002,
    KEY_ARROW_DOWN = 1003,
    KEY_CTRL_ARROW_LEFT = 1000,
    KEY_CTRL_ARROW_RIGHT = 1001,
    KEY_CTRL_ARROW_UP = 1006,
    KEY_CTRL_ARROW_DOWN = 1007,
    KEY_CTRL_BACKSPACE = 1008
};

/* universal */
#define KEY_QUIT CTRL_KEY('q')
#define KEY_ESCAPE '\x1b'
#define KEY_ENTER '\r'

/* normal mode */
/* navigation */
#define KEY_MOVE_LEFT 'h'
#define KEY_MOVE_DOWN 'j'
#define KEY_MOVE_UP 'k'
#define KEY_MOVE_RIGHT 'l'
#define KEY_INSERT_MODE 'i'
#define KEY_COMMAND_MODE ':'
#define KEY_OPEN_FILE CTRL_KEY('o')
#define KEY_SAVE_FILE CTRL_KEY('s')
#define KEY_TOGGLE_HELP CTRL_KEY('h')
#define KEY_TOGGLE_FIND CTRL_KEY('f')
#define KEY_TOGGLE_TAGS CTRL_KEY('y')
#define KEY_TOGGLE_FILE_TREE CTRL_KEY('p')
#define KEY_FIND_NEXT 'n'
#define KEY_FIND_PREV 'b'
#define KEY_YANK_LINE 'y'
#define KEY_DELETE_LINE 'd'
#define KEY_GO_TO_LINE CTRL_KEY('g')

/* insert mode */
#define KEY_BACKSPACE_ASCII 127
#define KEY_BACKSPACE_ALT 8
#define KEY_TAB '\t'

#endif