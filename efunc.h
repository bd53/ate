#ifndef EFUNC_H
#define EFUNC_H

#include "estruct.h"

/* bind.c */
int input_read_key();
void cursor_move(int key);
void process_keypress();

/* buffer.c */
void free_rows();
void insert_row(int at, char *s, size_t len);
void append_row(char *s, size_t len);
void delete_row(int at);
void insert_character(char c);
void insert_new_line();
void auto_dedent();
void delete_character();
void yank_line();
void delete_line();
void draw_content(struct Buffer *ab);
void command_mode();
void execute_command(char *cmd);

/* display.c */
int open_editor(char *filename);
void open_help();
void scroll_editor();
void draw_status(struct Buffer *ab);
void refresh_screen();
void display_message(int type, const char *message);
void ttopen(bool enable);

/* eval.c */
void check_health();

/* file.c */
void save_file();
void open_file();

/* input.c */
char *prompt(const char *prompt);

/* search.c */
void toggle_workspace_find();
void workspace_find_next(int direction);
void free_workspace_search();

/* tree.c */
void build_file_tree(const char *root_path);
void open_file_tree();
void toggle_file_tree();
void free_file_entries();

#endif
