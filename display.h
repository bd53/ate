#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

int open_editor(char *filename);
void open_help();
void scroll_editor();
char *prompt(const char *prompt);
void draw_status(buffer *ab);
void refresh_screen();
void display_message(int type, const char *message);

#endif
