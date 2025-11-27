#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "estruct.h"
#include "util.h"

static char **tags_lines = NULL;
static int tags_line_count = 0;
static int tags_offset = 0;

void toggle_tags(void)
{
        if (tags_mode) {
                tags_mode = 0;
                tags_offset = 0;
        } else {
                if (tags_lines == NULL) {
                        FILE *fp = fopen("tags", "r");
                        if (!fp) {
                                tags_line_count = 1;
                                tags_lines = malloc(sizeof(char *));
                                tags_lines[0] = strdup("tags file not found. Generate with 'make tags *'");
                                tags_mode = 1;
                                return;
                        }
                        char *line = NULL;
                        size_t cap = 0;
                        ssize_t len;
                        while ((len = getline(&line, &cap, fp)) != -1) {
                                if (len > 0 && line[len - 1] == '\n') {
                                        line[len - 1] = '\0';
                                }
                                tags_lines = realloc(tags_lines, sizeof(char *) * (tags_line_count + 1));
                                tags_lines[tags_line_count++] = strdup(line);
                        }
                        free(line);
                        fclose(fp);
                        if (tags_line_count == 0) {
                                tags_line_count = 1;
                                tags_lines = malloc(sizeof(char *));
                                tags_lines[0] = strdup("tags file is empty");
                        }
                }
                tags_mode = 1;
                tags_offset = 0;
        }
}

void tags_move_up(void)
{
        if (tags_offset > 0) {
                tags_offset--;
        }
}

void tags_move_down(void)
{
        int rows = get_window_rows() - 2;
        if (tags_offset + rows < tags_line_count) {
                tags_offset++;
        }
}

void render_tags(void)
{
        int rows = get_window_rows() - 2;
        int cols = get_window_cols();
        printf("\033[7m");
        printf("TAGS - Press CTRL+Y to close");
        for (int i = strlen("TAGS - Press CTRL+Y to close"); i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m\r\n");
        for (int i = 0; i < rows; i++) {
                int line_idx = tags_offset + i;
                if (line_idx < tags_line_count) {
                        printf("%s", tags_lines[line_idx]);
                }
                printf("\033[K\r\n");
        }
        printf("\033[7m");
        char status[256];
        snprintf(status, sizeof(status), "Lines %d-%d of %d", tags_offset + 1, (tags_offset + rows < tags_line_count) ? tags_offset + rows : tags_line_count, tags_line_count);
        printf("%s", status);
        for (int i = strlen(status); i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m");
        fflush(stdout);
}

void cleanup_tags(void)
{
        if (tags_lines) {
                for (int i = 0; i < tags_line_count; i++) {
                        free(tags_lines[i]);
                }
                free(tags_lines);
                tags_lines = NULL;
                tags_line_count = 0;
                tags_offset = 0;
        }
}
