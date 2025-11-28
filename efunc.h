#ifndef EFUNC_H
#define EFUNC_H

#include <stdbool.h>

#include "estruct.h"

/* bind.c */
int input_read_key(void);
void process_keypress(void);

/* buffer.c */
void free_rows(void);
void resize_row(struct Row *row, int new_size);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_newline(void);
void delete_character(void);
void draw_content(struct Buffer *ab);

/* display.c */
void refresh(void);
int init(char *filename);
void status(struct Buffer *ab);
void notify(int type, const char *message);

/* eval.c */
void check_health(void);

/* file.c */
void save_file(void);
void open_file(void);

/* input.c */
char *prompt(const char *prompt);

/* posix.c */
void ttopen(bool enable);

/* search.c */
void toggle_workspace_find(void);
void workspace_find_next(int direction);
void free_workspace_search(void);

/* tree.c */
void open_file_tree(void);
void toggle_file_tree(void);
void free_file_entries(void);

#endif
