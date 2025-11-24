#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include "content.h"
#include "file.h"
#include "search.h"
#include "tree.h"
#include "utils.h"

void run_cleanup();
void die(const char *s);
void disable_raw_mode();
void enable_raw_mode();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
void abinit(buffer *ab);
void abappend(buffer *ab, const char *s, int len);
void abfree(buffer *ab);
int leading_whitespace(const char *line, int len);
char *trim_whitespace(char *str);
void trim_leadingspace(int num_spaces);

#endif