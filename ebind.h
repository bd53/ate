#ifndef EBIND_H
#define EBIND_H

#define KEY_CTRL_Q 17
#define KEY_CTRL_S 19
#define KEY_CTRL_P 16
#define KEY_CTRL_F 6
#define KEY_BACKSPACE 127
#define KEY_ENTER 13
#define KEY_ESC 27
#define KEY_TAB 9

#define KEY_ARROW_UP 'A'
#define KEY_ARROW_DOWN 'B'
#define KEY_ARROW_RIGHT 'C'
#define KEY_ARROW_LEFT 'D'

int process_keypress(char c);
int read_esc_sequence(void);

#endif
