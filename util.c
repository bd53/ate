#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "ebind.h"
#include "util.h"

void run_cleanup(void)
{
        if (Editor.query) {
                free(Editor.query);
                Editor.query = NULL;
        }
        free_rows();
        free_file_entries();
        free_workspace_search();
}

void die(const char *s)
{
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        perror(s);
        exit(1);
}

int fetch(int *rows, int *cols)
{
        if (rows == NULL || cols == NULL)
                return -1;
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0 && ws.ws_row != 0) {
                *cols = ws.ws_col;
                *rows = ws.ws_row;
                return 0;
        }
        char buf[32];
        unsigned int i = 0;
        if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
                return -1;
        while (i < sizeof(buf) - 1) {
                if (read(STDIN_FILENO, &buf[i], 1) != 1)
                        break;
                if (buf[i] == 'R')
                        break;
                i++;
        }
        buf[i] = '\0';
        if (buf[0] != '\x1b' || buf[1] != '[')
                return -1;
        if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
                return -1;
        return 0;
}

void append(struct Buffer *ab, const char *s, int len)
{
        if (ab == NULL || s == NULL || len < 0)
                return;
        if (len == 0)
                return;
        char *new = realloc(ab->b, ab->length + len);
        if (new == NULL)
                die("realloc");
        memcpy(&new[ab->length], s, len);
        ab->b = new;
        ab->length += len;
}

int calculate(void)
{
        if (Editor.file_tree)
                return 0;
        int max = Editor.buffer_rows > Editor.editor_rows ? Editor.buffer_rows : Editor.editor_rows;
        return (max < 9 ? 1 : max < 99 ? 2 : max < 999 ? 3 : 4) + 1;
}
