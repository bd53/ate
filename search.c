#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "edef.h"
#include "efunc.h"
#include "estruct.h"
#include "util.h"

void init_search(void)
{
        memset(&search_state, 0, sizeof(struct SearchState));
        search_state.selected_index = 0;
        search_state.scroll_offset = 0;
        search_state.query[0] = '\0';
        search_state.query_len = 0;
}

void free_search(void)
{
        for (int i = 0; i < search_state.result_count; i++) {
                free(search_state.results[i].filepath);
                free(search_state.results[i].line_content);
        }
        search_state.result_count = 0;
}

int fuzzy_match(const char *pattern, const char *str)
{
        if (!pattern || !str)
                return 0;
        if (strlen(pattern) == 0)
                return 1;
        const char *p = pattern;
        const char *s = str;
        int score = 0;
        int consecutive = 0;
        while (*p && *s) {
                if (tolower(*p) == tolower(*s)) {
                        score += 1 + consecutive * 5;
                        consecutive++;
                        p++;
                } else {
                        consecutive = 0;
                }
                s++;
        }
        return (*p == '\0') ? score : 0;
}

static void search_file(const char *filepath, const char *query)
{
        FILE *fp = fopen(filepath, "r");
        if (!fp)
                return;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        int line_num = 0;
        while ((read = getline(&line, &len, fp)) != -1 && search_state.result_count < MAX_SEARCH_RESULTS) {
                line_num++;
                if (read > 0 && line[read - 1] == '\n') {
                        line[read - 1] = '\0';
                        read--;
                }
                int score = fuzzy_match(query, line);
                if (score > 0) {
                        struct SearchResult *result = &search_state.results[search_state.result_count];
                        result->filepath = strdup(filepath);
                        result->line_number = line_num;
                        result->line_content = strdup(line);
                        result->score = score;
                        search_state.result_count++;
                }
        }
        free(line);
        fclose(fp);
}

static void search_directory_recursive(const char *dirpath, const char *query, int depth)
{
        if (depth > 5)
                return;
        if (search_state.result_count >= MAX_SEARCH_RESULTS)
                return;
        DIR *dir = opendir(dirpath);
        if (!dir)
                return;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                        continue;
                if (entry->d_name[0] == '.')
                        continue;
                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
                struct stat st;
                if (stat(fullpath, &st) == -1)
                        continue;
                if (S_ISDIR(st.st_mode)) {
                        if (strcmp(entry->d_name, ".git") == 0 ||
                                strcmp(entry->d_name, ".o") == 0)
                                continue;
                        search_directory_recursive(fullpath, query, depth + 1);
                } else if (S_ISREG(st.st_mode)) {
                        const char *ext = strrchr(entry->d_name, '.');
                        if (ext && (strcmp(ext, ".c") == 0 ||
                                    strcmp(ext, ".h") == 0 ||
                                    strcmp(ext, ".txt") == 0 ||
                                    strcmp(ext, ".md") == 0 ||
                                    strcmp(ext, ".py") == 0)) {
                                search_file(fullpath, query);
                        }
                }
        }
        closedir(dir);
}

static int compare_results(const void *a, const void *b)
{
        const struct SearchResult *ra = (const struct SearchResult *) a;
        const struct SearchResult *rb = (const struct SearchResult *) b;
        return rb->score - ra->score;
}

void search_directory(const char *query)
{
        free_search();
        if (strlen(query) == 0) {
                return;
        }
        search_directory_recursive(".", query, 0);
        qsort(search_state.results, search_state.result_count, sizeof(struct SearchResult), compare_results);
        search_state.selected_index = 0;
        search_state.scroll_offset = 0;
}

void render_search_interface(void)
{
        int rows = get_window_rows();
        int cols = get_window_cols();
        printf("\033[2J\033[H");
        printf("\033[7m");
        printf(" SEARCH: %s", search_state.query);
        for (int i = strlen(search_state.query) + 9; i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m\r\n");
        int visible_rows = rows - 3;
        int end_index = search_state.scroll_offset + visible_rows;
        if (end_index > search_state.result_count) {
                end_index = search_state.result_count;
        }
        for (int i = search_state.scroll_offset; i < end_index; i++) {
                struct SearchResult *result = &search_state.results[i];
                if (i == search_state.selected_index) {
                        printf("\033[7m");
                }
                char display[512];
                snprintf(display, sizeof(display), " %s:%d: %s", result->filepath, result->line_number, result->line_content);
                if (strlen(display) > (size_t) cols) {
                        display[cols - 4] = '.';
                        display[cols - 3] = '.';
                        display[cols - 2] = '.';
                        display[cols - 1] = '\0';
                }
                printf("%s", display);
                for (int j = strlen(display); j < cols; j++) {
                        printf(" ");
                }
                if (i == search_state.selected_index) {
                        printf("\033[0m");
                }
                printf("\r\n");
        }
        for (int i = end_index - search_state.scroll_offset; i < visible_rows;
             i++) {
                printf("~\r\n");
        }
        printf("\033[7m");
        char status[256];
        snprintf(status, sizeof(status), " %d results", search_state.result_count);
        printf("%s", status);
        for (int i = strlen(status); i < cols; i++) {
                printf(" ");
        }
        printf("\033[0m");
        fflush(stdout);
}

void search_move_up(void)
{
        if (search_state.selected_index > 0) {
                search_state.selected_index--;
                if (search_state.selected_index < search_state.scroll_offset) {
                        search_state.scroll_offset = search_state.selected_index;
                }
        }
}

void search_move_down(void)
{
        if (search_state.selected_index < search_state.result_count - 1) {
                search_state.selected_index++;
                int visible_rows = get_window_rows() - 3;
                if (search_state.selected_index >=
                    search_state.scroll_offset + visible_rows) {
                        search_state.scroll_offset = search_state.selected_index - visible_rows + 1;
                }
        }
}

void search_select(void)
{
        if (search_state.result_count == 0)
                return;
        struct SearchResult *result = &search_state.results[search_state.selected_index];
        load_file(result->filepath);
        editor.cursor_y = result->line_number - 1;
        if (editor.cursor_y >= editor.line_numbers) {
                editor.cursor_y = editor.line_numbers - 1;
        }
        if (editor.cursor_y < 0) {
                editor.cursor_y = 0;
        }
        editor.cursor_x = 0;
        int rows = get_window_rows() - 2;
        int center_offset = editor.cursor_y - (rows / 2);
        if (center_offset < 0) {
                editor.offset_y = 0;
        } else if (center_offset + rows > editor.line_numbers) {
                editor.offset_y = editor.line_numbers - rows;
                if (editor.offset_y < 0) {
                        editor.offset_y = 0;
                }
        } else {
                editor.offset_y = center_offset;
        }
        search_mode = 0;
        free_search();
}

void search_add_char(char c)
{
        if (search_state.query_len < MAX_SEARCH_QUERY - 1) {
                search_state.query[search_state.query_len++] = c;
                search_state.query[search_state.query_len] = '\0';
                search_directory(search_state.query);
        }
}

void search_remove_char(void)
{
        if (search_state.query_len > 0) {
                search_state.query_len--;
                search_state.query[search_state.query_len] = '\0';
                search_directory(search_state.query);
        }
}
