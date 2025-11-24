#ifndef CONTENT_H
#define CONTENT_H

#include <stddef.h>
#include "common.h"

void free_rows();
void insert_row(int at, char *s, size_t len);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_new_line();
void auto_dedent();
void delete_character();
void insert_utf8_character(const char *utf8_char, int char_len);
void yank_line();
void delete_line();
void draw_content(buffer *ab);

#endif
