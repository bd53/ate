#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "keybinds.h"
#include "utils.h"

struct Setup E;

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
    write(STDOUT_FILENO, "\x1b[?1000l", 9);
    write(STDOUT_FILENO, "\x1b[?1002l", 9);
    write(STDOUT_FILENO, "\x1b[?1003l", 9);
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disable_raw_mode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
    write(STDOUT_FILENO, "\x1b[?1000l", 9);
    write(STDOUT_FILENO, "\x1b[?1002l", 9);
    write(STDOUT_FILENO, "\x1b[?1003l", 9);
}

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (rows == NULL || cols == NULL) {
        return -1;
    }
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
        return -1;
    }
    return 0;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (rows == NULL || cols == NULL) {
        return -1;
    }
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col != 0 && ws.ws_row != 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    } else {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return get_cursor_position(rows, cols);
    }
}

void abinit(struct buffer *ab) {
    if (ab == NULL) {
        return;
    }
    ab->b = NULL;
    ab->len = 0;
}

void abappend(struct buffer *ab, const char *s, int len) {
    if (ab == NULL || s == NULL || len < 0) {
        return;
    }
    if (len == 0) {
        return;
    }
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) {
        die("realloc");
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abfree(struct buffer *ab) {
    if (ab == NULL) {
        return;
    }
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
}

char *trim_whitespace(char *str) {
    if (str == NULL) {
        return NULL;
    }
    while (isspace((unsigned char)*str)) {
        str++;
    }
    if (*str == 0) {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}
