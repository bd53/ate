#ifndef UTIL_H
#define UTIL_H

#include "efunc.h"

void run_cleanup();
void die(const char *s);
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void append(struct Buffer *ab, const char *s, int len);
int leading_whitespace(const char *line, int len);
char *trim_whitespace(char *str);
void trim_leadingspace(int num_spaces);

#endif