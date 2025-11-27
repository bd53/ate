#ifndef EFUNC_H
#define EFUNC_H

/* buffer.c */
void insert_char(char c);
void insert_newline(void);
void delete_char(void);
void indent_line(void);

/* file.c */
void load_file(const char *filename);
int save_file(void);

/* help.c */
void toggle_help(void);
void help_move_up(void);
void help_move_down(void);
void render_help(void);
void cleanup_help(void);

/* posix.c */
void ttopen(void);
void ttclose(void);

/* tree.c */
void init_filetree(const char *path);
void free_filetree(void);
void load_directory(const char *path);
void render_filetree(void);
void filetree_move_up(void);
void filetree_move_down(void);
void filetree_select(void);
void filetree_go_parent(void);

/* search.c */
void init_search(void);
void free_search(void);
void search_directory(const char *query);
void render_search_interface(void);
void search_move_up(void);
void search_move_down(void);
void search_select(void);
void search_add_char(char c);
void search_remove_char(void);
int fuzzy_match(const char *pattern, const char *str);

#endif
