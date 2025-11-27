#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "estruct.h"
#include "util.h"

static char **help_lines = NULL;
static int help_line_count = 0;
static int help_offset = 0;

void toggle_help(void)
{
        if (help_mode) {
                help_mode = 0;
                help_offset = 0;
        } else {
                if (help_lines == NULL) {
                        FILE *fp = fopen("ate.hlp", "r");
                        if (!fp) {
                                help_line_count = 1;
                                help_lines = malloc(sizeof(char *));
                                help_lines[0] = strdup("ate.hlp not found");
                                help_mode = 1;
                                return;
                        }
                        char *line = NULL;
                        size_t cap = 0;
                        ssize_t len;
                        while ((len = getline(&line, &cap, fp)) != -1) {
                                if (len > 0 && line[len - 1] == '\n') {
                                        line[len - 1] = '\0';
                                }
                                help_lines = realloc(help_lines, sizeof(char *) * (help_line_count + 1));
                                help_lines[help_line_count++] = strdup(line);
                        }
                        free(line);
                        fclose(fp);
                        if (help_line_count == 0) {
                                help_line_count = 1;
                                help_lines = malloc(sizeof(char *));
                                help_lines[0] = strdup("ate.hlp is empty");
                        }
                }
                help_mode = 1;
                help_offset = 0;
        }
}

void help_move_up(void)
{
        if (help_offset > 0) {
                help_offset--;
        }
}

void help_move_down(void)
{
        int rows = get_window_rows() - 2;
        if (help_offset + rows < help_line_count) {
                help_offset++;
        }
}

void render_help(void)
{
        int rows = get_window_rows() - 2;
        int cols = get_window_cols();
        printf("\033[7m");
        printf("HELP - Press CTRL+H to close");
        for (int i = strlen("HELP - Press CTRL+H to close"); i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m\r\n");
        for (int i = 0; i < rows; i++) {
                int line_idx = help_offset + i;
                if (line_idx < help_line_count) {
                        printf("%s", help_lines[line_idx]);
                }
                printf("\033[K\r\n");
        }
        printf("\033[7m");
        char status[256];
        snprintf(status, sizeof(status), "Lines %d-%d of %d", help_offset + 1, (help_offset + rows < help_line_count) ? help_offset + rows : help_line_count, help_line_count);
        printf("%s", status);
        for (int i = strlen(status); i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m");
        fflush(stdout);
}

void cleanup_help(void)
{
        if (help_lines) {
                for (int i = 0; i < help_line_count; i++) {
                        free(help_lines[i]);
                }
                free(help_lines);
                help_lines = NULL;
                help_line_count = 0;
                help_offset = 0;
        }
}
