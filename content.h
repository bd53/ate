#ifndef CONTENT_H
#define CONTENT_H

#include <stddef.h>

void insert_row(int at, char *s, size_t len);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_new_line();
void free_rows();
void free_file_entries();
void run_cleanup();

#endif
