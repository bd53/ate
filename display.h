#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

void scroll_editor();
void refresh_screen();
void draw_content(struct buffer *ab);
void draw_status(struct buffer *ab);
char *prompt(const char *prompt);

#endif
