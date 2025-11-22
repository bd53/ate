#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void die(const char *s);
void disableRawMode();
void enableRawMode();
int getCursorPosition(int *rows, int *cols);
int getWindowSize(int *rows, int *cols);
void abInit(struct buffer *ab);
void abAppend(struct buffer *ab, const char *s, int len);
void abFree(struct buffer *ab);
char *trim_whitespace(char *str);

#endif