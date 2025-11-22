#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"
#include "editor.h"
#include "keybinds.h"
#include "utils.h"

static int readEscapeSequence() {
    char seq[5];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[' && seq[1] == '1') {
        if (read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == ';' && read(STDIN_FILENO, &seq[3], 1) == 1 && seq[3] == '5' && read(STDIN_FILENO, &seq[4], 1) == 1) {
            switch (seq[4]) {
                case 'A': return CTRL_ARROW_UP;
                case 'B': return CTRL_ARROW_DOWN;
                case 'C': return CTRL_ARROW_RIGHT;
                case 'D': return CTRL_ARROW_LEFT;
            }
        }
    }
    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
        }
    }
    return '\x1b';
}

int inputReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return (c == '\x1b') ? readEscapeSequence() : c;
}

void editorCursorMove(int key) {
    erow *row = (E.cy >= 0 && E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    E.match_row = -1;
    E.match_col = -1;
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                if (E.cy >= 0 && E.cy < E.numrows) {
                    E.cx = E.row[E.cy].size;
                }
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy > 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows - 1) E.cy++;
            break;
        case CTRL_ARROW_LEFT:
            E.cx = 0;
            break;
        case CTRL_ARROW_RIGHT:
            if (row) E.cx = row->size;
            break;
        case CTRL_ARROW_UP:
        case CTRL_ARROW_DOWN: {
            int jump = (E.screenrows / 7 < 1) ? 1 : E.screenrows / 7;
            E.cy += (key == CTRL_ARROW_UP) ? -jump : jump;
            if (E.cy < 0) E.cy = 0;
            if (E.cy >= E.numrows) E.cy = E.numrows - 1;
            break;
        }
    }
    row = (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) ? &E.row[E.cy] : NULL;
    int len = row ? row->size : 0;
    if (E.cx > len) E.cx = len;
}

static int translateKey(int key) {
    switch (key) {
        case 'h': return ARROW_LEFT;
        case 'j': return ARROW_DOWN;
        case 'k': return ARROW_UP;
        case 'l': return ARROW_RIGHT;
        default: return key;
    }
}

static void handleQuit() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    editorCleanup();
    exit(0);
}

static void handleFileTreeKeys(int c) {
    switch (c) {
        case CTRL_KEY('q'):
            handleQuit();
            break;
        case CTRL_KEY('p'):
            editorToggleFileTree();
            return;
        case 'q':
        case '\x1b':
            editorToggleFileTree();
            return;
        case ':':
            E.mode = MODE_COMMAND;
            editorCommandMode();
            return;
        case '\r':
            editorFileTreeOpen();
            return;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case CTRL_ARROW_UP:
        case CTRL_ARROW_DOWN:
        case CTRL_ARROW_LEFT:
        case CTRL_ARROW_RIGHT:
            editorCursorMove(c);
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            editorCursorMove(translateKey(c));
            break;
    }
    editorRefreshScreen();
}

static void handleHelpViewKeys(int c) {
    switch (c) {
        case CTRL_KEY('q'):
            handleQuit();
            break;
        case 'q':
        case '\x1b':
            editorCleanup();
            E.is_help_view = 0;
            if (E.numrows == 0) {
                editorContentAppendRow("", 0);
            }
            break;
        case ':':
            E.mode = MODE_COMMAND;
            editorCommandMode();
            return;
        case CTRL_KEY('h'):
            editorOpenHelp();
            return;
        case CTRL_KEY('p'):
            editorToggleFileTree();
            return;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case CTRL_ARROW_UP:
        case CTRL_ARROW_DOWN:
        case CTRL_ARROW_LEFT:
        case CTRL_ARROW_RIGHT:
            editorCursorMove(c);
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            editorCursorMove(translateKey(c));
            break;
    }
    editorRefreshScreen();
}

static void handleNormalMode(int c) {
    switch (c) {
        case '\r':
            break;
        case ':':
            E.mode = MODE_COMMAND;
            editorCommandMode();
            return;
        case CTRL_KEY('h'):
            editorOpenHelp();
            return;
        case CTRL_KEY('p'):
            editorToggleFileTree();
            return;
        case CTRL_KEY('o'):
            editorOpenFile();
            return;
        case CTRL_KEY('f'):
            editorFind();
            return;
        case CTRL_KEY('s'):
            editorSave();
            return;
        case 'n':
            editorFindNext(1);
            return;
        case 'N':
            editorFindNext(-1);
            return;
        case 'i':
            E.mode = MODE_INSERT;
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            editorCursorMove(translateKey(c));
            break;
        case 'd':
            if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
                editorDelRow(E.cy);
                if (E.numrows == 0) {
                    E.cy = 0;
                } else if (E.cy >= E.numrows) {
                    E.cy = E.numrows - 1;
                }
                if (E.cy < 0) E.cy = 0;
                E.cx = 0;
            }
            break;
    }
}

static void handleBackspace() {
    if (E.cy < 0 || E.cy >= E.numrows) return;
    E.dirty = 1;
    if (E.cx > 0) {
        erow *row = &E.row[E.cy];
        E.cx--;
        memmove(&row->chars[E.cx], &row->chars[E.cx + 1], row->size - E.cx);
        row->size--;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars == NULL) die("realloc");
        row->chars = new_chars;
        row->chars[row->size] = '\0';
    } else if (E.cy > 0) {
        erow *prev_row = &E.row[E.cy - 1];
        erow *curr_row = &E.row[E.cy];
        E.cx = prev_row->size;
        char *new_chars = realloc(prev_row->chars, prev_row->size + curr_row->size + 1);
        if (new_chars == NULL) die("realloc");
        prev_row->chars = new_chars;
        memcpy(&prev_row->chars[prev_row->size], curr_row->chars, curr_row->size);
        prev_row->size += curr_row->size;
        prev_row->chars[prev_row->size] = '\0';
        free(curr_row->chars);
        curr_row->chars = NULL;
        free(curr_row->hl);
        curr_row->hl = NULL;
        memmove(&E.row[E.cy], &E.row[E.cy + 1], sizeof(erow) * (E.numrows - E.cy - 1));
        E.numrows--;
        E.cy--;
    }
}

static void handleInsertMode(int c) {
    switch (c) {
        case '\r':
            E.dirty = 1;
            editorContentInsertNewline();
            break;
        case '\t':
            E.dirty = 1;
            for (int i = 0; i < 4; i++) {
                editorContentInsertChar(' ');
            }
            break;
        case 127:
            handleBackspace();
            break;
        default:
            if (c >= 32 && c < 127) {
                E.dirty = 1;
                editorContentInsertChar(c);
            }
            break;
    }
}

void editorProcessKeypress() {
    int c = inputReadKey();
    if (E.is_file_tree) {
        handleFileTreeKeys(c);
        return;
    }
    if (E.is_help_view) {
        handleHelpViewKeys(c);
        return;
    }
    switch (c) {
        case CTRL_KEY('q'):
            handleQuit();
            break;
        case CTRL_KEY('p'):
            editorToggleFileTree();
            break;
        case '\x1b':
            if (E.mode == MODE_INSERT) {
                E.mode = MODE_NORMAL;
                if (E.cx > 0) E.cx--;
            } else if (E.mode == MODE_COMMAND) {
                E.mode = MODE_NORMAL;
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case CTRL_ARROW_UP:
        case CTRL_ARROW_DOWN:
        case CTRL_ARROW_LEFT:
        case CTRL_ARROW_RIGHT:
            editorCursorMove(c);
            break;
        default:
            if (E.mode == MODE_NORMAL) {
                handleNormalMode(c);
            } else if (E.mode == MODE_INSERT) {
                handleInsertMode(c);
            }
            break;
    }
    editorRefreshScreen();
}
