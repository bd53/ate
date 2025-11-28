#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"

static int read_escape_sequence(void)
{
        struct pollfd fds = {.fd = STDIN_FILENO,.events = POLLIN };
        if (poll(&fds, 1, 10) <= 0)
                return KEY_ESCAPE;
        char seq[5];
        if (read(STDIN_FILENO, &seq[0], 1) != 1 || seq[0] != '[')
                return KEY_ESCAPE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
                return KEY_ESCAPE;
        if (seq[1] == '1') {
                if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == ';' && read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1) {
                        switch (seq[4]) {
                        case 'A':
                                return KEY_CTRL_ARROW_UP;
                        case 'B':
                                return KEY_CTRL_ARROW_DOWN;
                        case 'C':
                                return KEY_CTRL_ARROW_RIGHT;
                        case 'D':
                                return KEY_CTRL_ARROW_LEFT;
                        }
                }
                return KEY_ESCAPE;
        }
        switch (seq[1]) {
        case 'A':
                return KEY_ARROW_UP;
        case 'B':
                return KEY_ARROW_DOWN;
        case 'C':
                return KEY_ARROW_RIGHT;
        case 'D':
                return KEY_ARROW_LEFT;
        case '3':
                if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == ';' && read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1 && seq[4] == '~')
                        return KEY_CTRL_BACKSPACE;
                break;
        }
        return (int) KEY_ESCAPE;
}

int input_read_key(void)
{
        ssize_t nread;
        char c;
        while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
                if (nread == -1 && errno != EAGAIN)
                        die("read");
        }
        if ((unsigned char) c == 0x9B)
                return read_escape_sequence();
        if (c == KEY_ESCAPE)
                return read_escape_sequence();
        return (int) c;
}

static void cursor_move(int key)
{
        struct Row *row = (Editor.row && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) ? &Editor.row[Editor.cursor_y] : NULL;
        Editor.found_row = -1;
        Editor.found_col = -1;
        switch (key) {
        case KEY_ARROW_LEFT:
        case KEY_MOVE_LEFT:
                if (Editor.cursor_x > 0) {
                        Editor.cursor_x--;
                } else if (Editor.cursor_y > 0) {
                        Editor.cursor_y--;
                        if (Editor.row && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows)
                                Editor.cursor_x = Editor.row[Editor.cursor_y].size;
                }
                break;
        case KEY_ARROW_RIGHT:
        case KEY_MOVE_RIGHT:
                if (row && Editor.cursor_x < row->size) {
                        Editor.cursor_x++;
                } else if (row && Editor.cursor_x == row->size && Editor.cursor_y < Editor.buffer_rows - 1) {
                        Editor.cursor_y++;
                        Editor.cursor_x = 0;
                }
                break;
        case KEY_ARROW_UP:
        case KEY_MOVE_UP:
                if (Editor.cursor_y > 0)
                        Editor.cursor_y--;
                break;
        case KEY_ARROW_DOWN:
        case KEY_MOVE_DOWN:
                if (Editor.cursor_y < Editor.buffer_rows - 1)
                        Editor.cursor_y++;
                break;
        case KEY_CTRL_ARROW_LEFT:
                Editor.cursor_x = 0;
                break;
        case KEY_CTRL_ARROW_RIGHT:
                if (row)
                        Editor.cursor_x = row->size;
                break;
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:{
                        int jump = (Editor.editor_rows / 7 < 1) ? 1 : Editor.editor_rows / 7;
                        Editor.cursor_y += (key == KEY_CTRL_ARROW_UP) ? -jump : jump;
                        if (Editor.cursor_y < 0)
                                Editor.cursor_y = 0;
                        if (Editor.cursor_y >= Editor.buffer_rows)
                                Editor.cursor_y = Editor.buffer_rows - 1;
                        break;
                }
        }
        row = (Editor.row && Editor.buffer_rows > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) ? &Editor.row[Editor.cursor_y] : NULL;
        int len = row ? row->size : 0;
        if (Editor.cursor_x > len)
                Editor.cursor_x = len;
}

static void handle_quit(void)
{
        write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
        run_cleanup();
        exit(0);
}

static void handle_help(void)
{
        if (Editor.help_view) {
                run_cleanup();
                Editor.help_view = 0;
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
        } else {
                run_cleanup();
                init("ate.hlp");
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
                Editor.help_view = 1;
        }
        refresh();
}

static void handle_tags(void)
{
        if (Editor.tag_view) {
                run_cleanup();
                Editor.tag_view = 0;
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
        } else {
                if (access("tags", F_OK) != 0) {
                        notify(2, "No tags file found");
                        return;
                }
                run_cleanup();
                init("tags");
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
                Editor.tag_view = 1;
        }
        refresh();
}

static void handle_command(char *cmd)
{
        while (isspace((unsigned char) *cmd))
                cmd++;
        static const struct {
                const char *name;
                void (*func)(void);
        } commands[] = {
                { "Ex", toggle_file_tree },
                { "find", toggle_workspace_find },
                { "help", handle_help },
                { "tags", handle_tags },
                { "checkhealth", check_health },
                { NULL, NULL }
        };
        for (int i = 0; commands[i].name; i++) {
                if (strcmp(cmd, commands[i].name) == 0) {
                        commands[i].func();
                        return;
                }
        }
        if (strcmp(cmd, "q") == 0) {
                run_cleanup();
                Editor.file_tree = Editor.help_view = 0;
                if (Editor.buffer_rows == 0)
                        append_row("", 0);
                Editor.cursor_x = Editor.cursor_y = Editor.row_offset = Editor.modified = 0;
                refresh();
        } else if (strcmp(cmd, "quit") == 0) {
                handle_quit();
        } else if (strcmp(cmd, "bd") == 0) {
                if (Editor.file_tree)
                        toggle_file_tree();
        } else if (strncmp(cmd, "w", 1) == 0) {
                char *arg = NULL;
                if (strncmp(cmd, "w ", 2) == 0) {
                        arg = cmd + 2;
                        while (isspace((unsigned char) *arg))
                                arg++;
                } else if (strncmp(cmd, "write ", 6) == 0) {
                        arg = cmd + 6;
                        while (isspace((unsigned char) *arg))
                                arg++;
                }
                if (arg && *arg) {
                        if (Editor.filename)
                                free(Editor.filename);
                        Editor.filename = strdup(arg);
                        if (!Editor.filename)
                                die("strdup");
                }
                if (!Editor.filename) {
                        char *filename = prompt("Save as: %s (ESC to cancel)");
                        if (!filename) {
                                refresh();
                                return;
                        }
                        Editor.filename = filename;
                }
                save_file();
        } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "E182: Not an editor command: '%s'", cmd);
                notify(2, msg);
                input_read_key();
                refresh();
        }
}

static void command_mode(void)
{
        size_t bufsize = 256, buflen = 0;
        char *buf = safe_malloc(bufsize);
        buf[0] = '\0';
        while (1) {
                if (Editor.cursor_y < Editor.row_offset)
                        Editor.row_offset = Editor.cursor_y;
                if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows)
                        Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
                struct Buffer ab = BUFFER_INIT;
                append(&ab, "\x1b[?25l\x1b[H", 9);
                draw_content(&ab);
                status(&ab);
                char pos[64];
                snprintf(pos, sizeof(pos), "\x1b[%d;1H\x1b[K:%s\x1b[%d;%dH\x1b[?25h", Editor.editor_rows + 2, buf, Editor.editor_rows + 2, (int) buflen + 2);
                append(&ab, pos, strlen(pos));
                if (write(STDOUT_FILENO, ab.b, ab.length) == -1) {
                        free(ab.b);
                        free(buf);
                        die("write");
                }
                free(ab.b);
                int c = input_read_key();
                if (c == '\r') {
                        if (buflen > 0)
                                handle_command(buf);
                        Editor.mode = 0;
                        free(buf);
                        refresh();
                        return;
                } else if (c == '\x1b') {
                        Editor.mode = 0;
                        free(buf);
                        refresh();
                        return;
                } else if (c == 127 || c == CTRL_KEY('h')) {
                        if (buflen > 0)
                                buf[--buflen] = '\0';
                } else if (c >= 32 && c < 127) {
                        if (buflen >= bufsize - 1) {
                                bufsize *= 2;
                                buf = safe_realloc(buf, bufsize);
                        }
                        buf[buflen++] = c;
                        buf[buflen] = '\0';
                }
        }
}

static void handle_spec(int c)
{
        int is_file_tree = Editor.file_tree;
        switch (c) {
        case KEY_QUIT:
                handle_quit();
                break;
        case KEY_TOGGLE_FILE_TREE:
                toggle_file_tree();
                return;
        case KEY_ESCAPE:
        case 'q':
                if (is_file_tree) {
                        toggle_file_tree();
                } else {
                        run_cleanup();
                        Editor.help_view = 0;
                        if (Editor.buffer_rows == 0)
                                append_row("", 0);
                }
                return;
        case KEY_COMMAND_MODE:
                Editor.mode = 2;
                command_mode();
                return;
        case KEY_ENTER:
                if (is_file_tree)
                        open_file_tree();
                return;
        case KEY_TOGGLE_HELP:
                if (!is_file_tree)
                        handle_help();
                return;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
        case KEY_MOVE_LEFT:
        case KEY_MOVE_DOWN:
        case KEY_MOVE_UP:
        case KEY_MOVE_RIGHT:
                cursor_move(c);
                break;
        }
        refresh();
}

static void yank_line(void)
{
        if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows)
                return;
        struct Row *row = &Editor.row[Editor.cursor_y];
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        char *encoded = safe_malloc(((row->size + 2) / 3) * 4 + 1);
        int j = 0;
        for (int i = 0; i < row->size; i += 3) {
                int n = row->chars[i] << 16;
                if (i + 1 < row->size)
                        n |= row->chars[i + 1] << 8;
                if (i + 2 < row->size)
                        n |= row->chars[i + 2];
                encoded[j++] = b64[(n >> 18) & 63];
                encoded[j++] = b64[(n >> 12) & 63];
                encoded[j++] = (i + 1 < row->size) ? b64[(n >> 6) & 63] : '=';
                encoded[j++] = (i + 2 < row->size) ? b64[n & 63] : '=';
        }
        encoded[j] = '\0';
        dprintf(STDOUT_FILENO, "\x1b]52;c;%s\x07", encoded);
        free(encoded);
        notify(1, "Yanked");
}

static void delete_line(void)
{
        if (Editor.buffer_rows > 0 && Editor.cursor_y >= 0 && Editor.cursor_y < Editor.buffer_rows) {
                delete_row(Editor.cursor_y);
                Editor.cursor_x = 0;
                notify(2, "Deleted");
        }
}

static void goto_line(void)
{
        char *command = prompt("Go to line: ");
        if (!command) {
                Editor.mode = 0;
                refresh();
                return;
        }
        char *trimmed = command;
        while (isspace((unsigned char) *trimmed))
                trimmed++;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && isspace((unsigned char) *end))
                *end-- = '\0';
        char *endptr;
        long line_number = strtol(trimmed, &endptr, 10);
        while (*endptr && isspace((unsigned char) *endptr))
                endptr++;
        int valid = (*endptr == '\0' && line_number > 0 && line_number <= Editor.buffer_rows);
        free(command);
        if (valid) {
                Editor.cursor_y = (int) line_number - 1;
                Editor.cursor_x = 0;
                if (Editor.cursor_y < Editor.row_offset)
                        Editor.row_offset = Editor.cursor_y;
                if (Editor.cursor_y >= Editor.row_offset + Editor.editor_rows)
                        Editor.row_offset = Editor.cursor_y - Editor.editor_rows + 1;
                notify(2, "Jumped");
        } else {
                notify(1, "Invalid");
        }
        Editor.mode = 0;
        refresh();
}

static void handle_normal_mode(int c)
{
        switch (c) {
        case KEY_ENTER:
                break;
        case KEY_INSERT_MODE:
                Editor.mode = 1;
                break;
        case KEY_COMMAND_MODE:
                Editor.mode = 2;
                command_mode();
                return;
        case KEY_OPEN_FILE:
                open_file();
                return;
        case KEY_SAVE_FILE:
                save_file();
                return;
        case KEY_TOGGLE_HELP:
                handle_help();
                return;
        case KEY_TOGGLE_FIND:
                toggle_workspace_find();
                return;
        case KEY_TOGGLE_TAGS:
                handle_tags();
                return;
        case KEY_TOGGLE_FILE_TREE:
                toggle_file_tree();
                return;
        case KEY_FIND_NEXT:
                workspace_find_next(1);
                return;
        case KEY_FIND_PREV:
                workspace_find_next(-1);
                return;
        case KEY_YANK_LINE:
                yank_line();
                break;
        case KEY_DELETE_LINE:
                delete_line();
                break;
        case KEY_GO_TO_LINE:
                goto_line();
                return;
        case KEY_MOVE_LEFT:
        case KEY_MOVE_DOWN:
        case KEY_MOVE_UP:
        case KEY_MOVE_RIGHT:
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
                cursor_move(c);
                break;
        }
}

static void auto_dedent(void)
{
        if (Editor.cursor_y < 0 || Editor.cursor_y >= Editor.buffer_rows)
                return;
        struct Row *row = &Editor.row[Editor.cursor_y];
        for (int i = 0; i < Editor.cursor_x - 1; i++)
                if (row->chars[i] != ' ' && row->chars[i] != '\t')
                        return;
        int leading = 0;
        for (int i = 0; i < row->size; i++) {
                if (row->chars[i] == ' ' || row->chars[i] == '\t')
                        leading++;
                else
                        break;
        }
        if (leading >= TAB_SIZE) {
                int spaces_to_remove = 0;
                for (int i = 0; i < row->size && i < TAB_SIZE; i++) {
                        if (row->chars[i] == ' ')
                                spaces_to_remove++;
                        else
                                break;
                }
                if (spaces_to_remove == 0)
                        return;
                memmove(row->chars, row->chars + spaces_to_remove, row->size - spaces_to_remove + 1);
                row->size -= spaces_to_remove;
                memmove(row->state, row->state + spaces_to_remove, row->size);
                resize_row(row, row->size);
                Editor.cursor_x -= spaces_to_remove;
                if (Editor.cursor_x < 0)
                        Editor.cursor_x = 0;
                Editor.modified = 1;
        }
}

static void handle_insert_mode(int c)
{
        switch (c) {
        case KEY_ENTER:
                Editor.modified = 1;
                insert_newline();
                break;
        case KEY_TAB:
                Editor.modified = 1;
                for (int i = 0; i < 4; i++)
                        insert_character(' ');
                break;
        case KEY_BACKSPACE_ASCII:
                delete_character();
                break;
        case KEY_BACKSPACE_ALT:
        case KEY_CTRL_BACKSPACE:
                delete_line();
                break;
        default:
                if (c >= 32 && c < 127) {
                        Editor.modified = 1;
                        insert_character((char) c);
                        if (c == '}' || c == ')' || c == ']')
                                auto_dedent();
                }
                break;
        }
}

void process_keypress(void)
{
        int c = input_read_key();
        if (Editor.file_tree || Editor.help_view) {
                handle_spec(c);
                return;
        }
        switch (c) {
        case KEY_QUIT:
                handle_quit();
                break;
        case KEY_TOGGLE_TAGS:
                if (Editor.mode == 0)
                        handle_tags();
                break;
        case KEY_TOGGLE_FILE_TREE:
                if (Editor.mode == 0)
                        toggle_file_tree();
                break;
        case KEY_BACKSPACE_ALT:
                if (Editor.mode == 1)
                        delete_line();
                else if (Editor.mode == 0)
                        handle_help();
                break;
        case KEY_CTRL_BACKSPACE:
                if (Editor.mode == 1)
                        delete_line();
                break;
        case KEY_ESCAPE:
                if (Editor.mode == 1) {
                        Editor.mode = 0;
                        if (Editor.cursor_x > 0)
                                Editor.cursor_x--;
                } else if (Editor.mode == 2) {
                        Editor.mode = 0;
                }
                break;
        case KEY_ARROW_UP:
        case KEY_ARROW_DOWN:
        case KEY_ARROW_LEFT:
        case KEY_ARROW_RIGHT:
        case KEY_CTRL_ARROW_UP:
        case KEY_CTRL_ARROW_DOWN:
        case KEY_CTRL_ARROW_LEFT:
        case KEY_CTRL_ARROW_RIGHT:
                cursor_move(c);
                break;
        default:
                if (Editor.mode == 0)
                        handle_normal_mode(c);
                else if (Editor.mode == 1)
                        handle_insert_mode(c);
                break;
        }
        refresh();
}
