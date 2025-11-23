#ifndef CONTENT_H
#define CONTENT_H

#include <stddef.h>

int utf8_char_len(const char *s);
int utf8_is_char_boundary(const char *s, int byte_offset);
int utf8_prev_char_boundary(const char *s, int byte_offset);
int utf8_next_char_boundary(const char *s, int byte_offset, int max_len);
void free_rows();
void run_cleanup();
void insert_row(int at, char *s, size_t len);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_new_line();
void delete_character();
void insert_utf8_character(const char *utf8_char, int char_len);

#endif
