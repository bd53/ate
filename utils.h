#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void die(const char *s);
void disable_raw_mode();
void enable_raw_mode();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void abinit(struct buffer *ab);
void abappend(struct buffer *ab, const char *s, int len);
void abfree(struct buffer *ab);
char *trim_whitespace(char *str);

#endif
