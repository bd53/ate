#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "estruct.h"

void load_file(const char *filename)
{
        FILE *fp = fopen(filename, "r");
        if (!fp) {
                return;
        }
        for (int i = 0; i < editor.line_numbers; i++) {
                free(editor.lines[i]);
        }
        free(editor.lines);
        editor.lines = NULL;
        editor.line_numbers = 0;
        char *line = NULL;
        size_t cap = 0;
        ssize_t len;
        while ((len = getline(&line, &cap, fp)) != -1) {
                if (len > 0 && line[len - 1] == '\n') {
                        line[len - 1] = '\0';
                }
                editor.lines = realloc(editor.lines, sizeof(char *) * (editor.line_numbers + 1));
                editor.lines[editor.line_numbers++] = strdup(line);
        }
        free(line);
        fclose(fp);
        if (editor.line_numbers == 0) {
                editor.lines = malloc(sizeof(char *));
                editor.lines[0] = strdup("");
                editor.line_numbers = 1;
        }
        editor.filename = strdup(filename);
}

int save_file()
{
        if (!editor.filename) {
                return 0;
        }
        FILE *fp = fopen(editor.filename, "w");
        if (!fp) {
                return 0;
        }
        for (int i = 0; i < editor.line_numbers; i++) {
                fprintf(fp, "%s\n", editor.lines[i]);
        }
        fclose(fp);
        editor.modified = 0;
        return 1;
}
