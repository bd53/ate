#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

void scroll_editor();
char *prompt(const char *prompt);
void draw_content(struct buffer *ab);
void draw_status(struct buffer *ab);
void refresh_screen();
void display_message(int type, const char *message);

#endif
