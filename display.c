#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

static void reset_editor_state(char *filename)
{
        free_rows();
        free_file_entries();
        if (Editor.query) {
                free(Editor.query);
                Editor.query = NULL;
        }
        if (Editor.filename) {
                free(Editor.filename);
        }
        Editor.filename = filename;
        Editor.help_view = 0;
        Editor.file_tree = 0;
        Editor.tag_view = 0;
}

void refresh_screen(void)
{
        if (Editor.cursor_y < Editor.row_offset)
                Editor.row_offset = Editor.cursor_y;
        if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows)
                Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
        struct Buffer ab = BUFFER_INIT;
        append(&ab, "\x1b[?25l\x1b[H", 9);
        draw_content(&ab);
        display_status(&ab);
        Editor.gutter_width = calculate();
        int content_width = Editor.editor_cols - Editor.gutter_width;
        if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                struct Row *row = &Editor.row[Editor.cursor_y];
                if (Editor.cursor_x > row->size)
                        Editor.cursor_x = row->size;
                if (Editor.cursor_x < 0)
                        Editor.cursor_x = 0;
        } else {
                Editor.cursor_x = 0;
                Editor.cursor_y = 0;
        }
        int screen_row = 0;
        for (int filerow = Editor.row_offset;
             filerow < Editor.cursor_y && filerow < Editor.buffer_rows;
             filerow++) {
                int wrapped = (Editor.row[filerow].size + content_width - 1) / content_width;
                screen_row += wrapped > 0 ? wrapped : 1;
        }
        int cur_x = 1, cur_y = screen_row + 1;
        if (Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                int wrap_index = Editor.cursor_x / content_width;
                screen_row += wrap_index;
                int col_in_wrap = Editor.cursor_x % content_width;
                cur_x = col_in_wrap + Editor.gutter_width + 1;
                cur_y = screen_row + 1;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH%s\x1b[?25h", cur_y, cur_x, Editor.mode == 1 ? "\x1b[5 q" : "\x1b[2 q");
        append(&ab, buf, strlen(buf));
        if (write(STDOUT_FILENO, ab.b, ab.length) == -1)
                die("write");
        free(ab.b);
}

int display_editor(char *filename)
{
        if (filename == NULL)
                return -1;
        char *new_filename = strdup(filename);
        if (new_filename == NULL)
                die("strdup");
        FILE *fp = fopen(filename, "r");
        if (!fp) {
                reset_editor_state(new_filename);
                append_row("", 0);
                return -1;
        }
        reset_editor_state(new_filename);
        char *line = NULL;
        size_t linecap = 0;
        ssize_t linelen;
        while ((linelen = getline(&line, &linecap, fp)) != -1) {
                while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
                        linelen--;
                append_row(line, linelen);
        }
        free(line);
        fclose(fp);
        return 0;
}

void display_help(void)
{
        if (Editor.help_view) {
                run_cleanup();
                Editor.help_view = 0;
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
        } else {
                run_cleanup();
                display_editor("ate.hlp");
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
                Editor.help_view = 1;
        }
        refresh_screen();
}

void display_tags(void)
{
        if (Editor.tag_view) {
                run_cleanup();
                Editor.tag_view = 0;
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
        } else {
                if (access("tags", F_OK) != 0) {
                        display_message(2, "No tags file found");
                        return;
                }
                run_cleanup();
                display_editor("tags");
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
                Editor.tag_view = 1;
        }
        refresh_screen();
}

void display_status(struct Buffer *ab)
{
        if (ab == NULL)
                return;
        append(ab, "\x1b[7m", 4);
        char status[80];
        const char *filetype_name = "[No Name]";
        if (Editor.filename) {
                char *dot = strrchr(Editor.filename, '.');
                if (dot != NULL && dot[1] != '\0') {
                        filetype_name = dot + 1;
                }
        }
        char filename_status[64];
        if (Editor.file_tree) {
                snprintf(filename_status, sizeof(filename_status), "netft");
        } else {
                snprintf(filename_status, sizeof(filename_status), "%s%s", Editor.filename ? Editor.filename : "[No Name]", Editor.modified ? " *" : "");
        }
        const char *mode_str;
        if (Editor.mode == 0)
                mode_str = "NORMAL";
        else if (Editor.mode == 1)
                mode_str = "INSERT";
        else if (Editor.mode == 2)
                mode_str = "COMMAND";
        else
                mode_str = "UNKNOWN";
        int percentage = (Editor.buffer_rows > 0) ? (int) (((float) (Editor.cursor_y + 1) / Editor.buffer_rows) * 100) : 100;
        int len = snprintf(status, sizeof(status), " %s | %s | R:%d L:%d (%d%%)", mode_str, filename_status, Editor.cursor_y + 1, Editor.buffer_rows > 0 ? Editor.buffer_rows : 1, percentage);
        if (len > Editor.editor_cols)
                len = Editor.editor_cols;
        append(ab, status, len);
        char rstatus[80];
        int rlen = snprintf(rstatus, sizeof(rstatus), " %s | C:%d ", filetype_name, Editor.cursor_x + 1);
        while (len < Editor.editor_cols - rlen) {
                append(ab, " ", 1);
                len++;
        }
        append(ab, rstatus, rlen);
        append(ab, "\x1b[m", 3);
        append(ab, "\r\n", 2);
}

void display_message(int type, const char *message)
{
        const char *color = (type == 1) ? "\x1b[32m" : "\x1b[31m";
        int prompt_row = Editor.editor_rows + 2;
        char pos_buf[32];
        snprintf(pos_buf, sizeof(pos_buf), "\x1b[%d;1H", prompt_row);
        write(STDOUT_FILENO, pos_buf, strlen(pos_buf));
        write(STDOUT_FILENO, "\x1b[K", 3);
        write(STDOUT_FILENO, color, strlen(color));
        write(STDOUT_FILENO, message, strlen(message));
        write(STDOUT_FILENO, "\x1b[0m", 4);
}
