#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "efunc.h"
#include "util.h"

void save_file(void)
{
        if (Editor.filename == NULL) {
                char *filename = prompt("Save as: %s (ESC to cancel)");
                if (filename == NULL) {
                        refresh();
                        return;
                }
                if (Editor.filename)
                        free(Editor.filename);
                Editor.filename = filename;
        }
        FILE *fp = fopen(Editor.filename, "w");
        if (fp == NULL) {
                refresh();
                return;
        }
        for (int i = 0; i < Editor.buffer_rows; i++) {
                fwrite(Editor.row[i].chars, 1, Editor.row[i].size, fp);
                fputc('\n', fp);
        }
        if (fclose(fp) == EOF) {
                refresh();
                return;
        }
        Editor.modified = 0;
        refresh();
}

void open_file(void)
{
        char *filename = prompt("Open file: %s (ESC to cancel)");
        if (!filename) {
                refresh();
                return;
        }
        if (Editor.file_tree)
                free_file_entries();
        Editor.file_tree = 0;
        Editor.help_view = 0;
        init(filename);
        free(filename);
        if (Editor.buffer_rows == 0)
                append_row("", 0);
        Editor.cursor_x = 0;
        Editor.cursor_y = 0;
        Editor.row_offset = 0;
        refresh();
}
