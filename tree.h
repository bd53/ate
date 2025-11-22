#ifndef TREE_H
#define TREE_H

#include "common.h"

void build_file_tree(const char *root_path);
void open_file_tree();
void toggle_file_tree();
void free_file_entries();

#endif
