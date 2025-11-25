#ifndef EFUNC_H
#define EFUNC_H

#include <stdbool.h>

#include "estruct.h"

/* bind.c */
int input_read_key(void);
void cursor_move(int key);
void process_keypress(void);

/* buffer.c */
void free_rows(void);
void insert_row(int at, char *s, size_t len);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_new_line(void);
void auto_dedent(void);
void delete_character(void);
void yank_line(void);
void delete_line(void);
void goto_line(void);
void draw_content(struct Buffer *ab);
void command_mode(void);
void execute_command(char *cmd);
void refresh_screen(void);

/* display.c */
int display_editor(char *filename);
void display_help(void);
void display_tags(void);
void display_status(struct Buffer *ab);
void display_message(int type, const char *message);

/* eval.c */
void check_health(void);

/* file.c */
void save_file(void);
void open_file(void);

/* input.c */
char *prompt(const char *prompt);

/* posix.c */
void ttclose(void);
void ttopen(bool enable);

/* search.c */
void toggle_workspace_find(void);
void workspace_find_next(int direction);
void free_workspace_search(void);

/* tree.c */
void build_file_tree(const char *root_path);
void open_file_tree(void);
void toggle_file_tree(void);
void free_file_entries(void);

#endif
