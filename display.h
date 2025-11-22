#ifndef DISPLAY_H
#define DISPLAY_H

#include "common.h"

void editorScroll();
void editorRefreshScreen();
void editorViewDrawContent(struct buffer *ab);
void editorViewDrawStatusBar(struct buffer *ab);
char *editorPrompt(const char *prompt);

#endif
