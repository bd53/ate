#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "search.h"
#include "tree.h"
#include "utils.h"

int utf8_char_len(const char *s, int max_len) {
    if (s == NULL || max_len <= 0) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return 1;
    if (max_len < 2) return 0;
    if ((c & 0xE0) == 0xC0) return 2;
    if (max_len < 3) return 0;
    if ((c & 0xF0) == 0xE0) return 3;
    if (max_len < 4) return 0;
    if ((c & 0xF8) == 0xF0) return 4;
    return 0;
}

int utf8_decode(const char *s, int max_len, int *codepoint) {
    if (s == NULL || max_len <= 0) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) {
        *codepoint = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        if (max_len < 2 || (s[1] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);
        if (*codepoint < 0x80) return -1;
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (max_len < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (*codepoint < 0x800 || (*codepoint >= 0xD800 && *codepoint <= 0xDFFF)) return -1;
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (max_len < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return -1;
        *codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (*codepoint < 0x10000 || *codepoint > 0x10FFFF) return -1;
        return 4;
    }
    return -1;
}

int utf8_is_valid(int codepoint) {
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
        return 0;
    }
    if (codepoint >= 0x00 && codepoint <= 0x10FFFF) {
        return 1;
    }
    return 0;
}

int utf8_is_char_boundary(const char *s, int byte_offset) {
    if (s == NULL || byte_offset < 0) return 1;
    if (s[byte_offset] == '\0') return 1;
    unsigned char c = (unsigned char)s[byte_offset];
    return (c & 0xC0) != 0x80;
}

int utf8_prev_char_boundary(const char *s, int byte_offset) {
    if (byte_offset <= 0) return 0;
    byte_offset--;
    while (byte_offset > 0 && !utf8_is_char_boundary(s, byte_offset)) {
        byte_offset--;
    }
    return byte_offset;
}

int utf8_next_char_boundary(const char *s, int byte_offset, int max_len) {
    if (byte_offset >= max_len) return max_len;
    int remaining_len = max_len - byte_offset;
    int len = utf8_char_len(&s[byte_offset], remaining_len);
    if (len == 0) {
        if (remaining_len > 0) return byte_offset + 1;
        return byte_offset;
    }
    byte_offset += len;
    if (byte_offset > max_len) return max_len;
    return byte_offset;
}

void free_rows() {
    for (int j = 0; j < E.numrows; j++) {
        free(E.row[j].chars);
        free(E.row[j].hl);
    }
    free(E.row);
    E.row = NULL;
    E.numrows = 0;
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
}

void run_cleanup() {
    if (E.query) {
        free(E.query);
        E.query = NULL;
    }
    free_rows();
    free_file_entries();
    free_workspace_search();
}

void insert_row(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at));
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void append_row(char *s, size_t len) {
    if (len > 0 && s[len - 1] == '\n') len--;
    erow *new_rows = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (new_rows == NULL) die("realloc");
    E.row = new_rows;
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    int hl_size = len > 0 ? len : 1;
    E.row[at].hl = malloc(hl_size);
    if (E.row[at].hl == NULL) {
        free(E.row[at].chars);
        die("malloc");
    }
    memset(E.row[at].hl, HL_NORMAL, hl_size);
    E.row[at].hl_open_comment = 0;
    E.numrows++;
}

void delete_row(int at) {
    if (at < 0 || at >= E.numrows) return;
    erow *row = &E.row[at];
    free(row->chars);
    free(row->hl);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    if (E.numrows == 0) {
        free(E.row);
        E.row = NULL;
        E.cy = 0;
    } else {
        erow *new_rows = realloc(E.row, sizeof(erow) * E.numrows);
        if (new_rows != NULL) {
            E.row = new_rows;
        } else {
            die("realloc (delete_row shrink)");
        }
        if (E.cy >= E.numrows) {
            E.cy = E.numrows - 1;
        }
    }
    if (E.cy < 0) {
        E.cy = 0;
    }
    E.dirty = 1;
}

void insert_character(char c) {
    if (E.cy == E.numrows) {
        append_row("", 0);
    }
    erow *row = &E.row[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > row->size) E.cx = row->size;
    if (!utf8_is_char_boundary(row->chars, E.cx)) {
        E.cx = utf8_prev_char_boundary(row->chars, E.cx);
    }
    char *new = realloc(row->chars, row->size + 2);
    if (new == NULL) die("realloc");
    row->chars = new;
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;
    row->size++;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->hl, new_hl_size);
    if (new_hl == NULL) die("realloc");
    row->hl = new_hl;
    memmove(&row->hl[E.cx + 1], &row->hl[E.cx], row->size - E.cx - 1);
    row->hl[E.cx] = HL_NORMAL;
    E.cx++;
    E.dirty = 1;
}

void insert_new_line() {
    if (E.cy == E.numrows) {
        append_row("", 0);
    } else {
        erow *row = &E.row[E.cy];
        int split_at = E.cx;
        if (split_at < 0) split_at = 0;
        if (split_at > row->size) split_at = row->size;
        if (!utf8_is_char_boundary(row->chars, split_at)) {
            split_at = utf8_prev_char_boundary(row->chars, split_at);
        }
        int second_half_len = row->size - split_at;
        char *second_half = malloc(second_half_len + 1);
        if (second_half == NULL) die("malloc");
        memcpy(second_half, &row->chars[split_at], second_half_len);
        second_half[second_half_len] = '\0';
        row->size = split_at;
        char *new_chars = realloc(row->chars, row->size + 1);
        if (new_chars == NULL) {
            free(second_half);
            die("realloc");
        }
        row->chars = new_chars;
        row->chars[row->size] = '\0';
        int hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->hl, hl_size);
        if (new_hl == NULL) {
            free(second_half);
            die("realloc");
        }
        row->hl = new_hl;
        insert_row(E.cy + 1, second_half, second_half_len);
        free(second_half);
    }
    E.cy++;
    E.cx = 0;
    E.dirty = 1;
}

void delete_character() {
    if (E.cx == 0 && E.cy == 0) return;
    if (E.cy >= E.numrows) return;
    if (E.cx == 0) {
        if (E.cy == 0) return;
        E.cy--;
        erow *row = &E.row[E.cy];
        erow *next_row = &E.row[E.cy + 1];
        E.cx = row->size;
        char *new_chars = realloc(row->chars, row->size + next_row->size + 1);
        if (new_chars == NULL) die("realloc");
        row->chars = new_chars;
        memcpy(&row->chars[row->size], next_row->chars, next_row->size);
        row->size += next_row->size;
        row->chars[row->size] = '\0';
        unsigned char *new_hl = realloc(row->hl, row->size > 0 ? row->size : 1);
        if (new_hl == NULL) die("realloc");
        row->hl = new_hl;
        memcpy(&row->hl[E.cx], next_row->hl, next_row->size);
        delete_row(E.cy + 1);
        E.dirty = 1;
    } else {
        erow *row = &E.row[E.cy];
        int prev_pos = utf8_prev_char_boundary(row->chars, E.cx);
        int char_len = E.cx - prev_pos;
        memmove(&row->chars[prev_pos], &row->chars[E.cx], row->size - E.cx + 1);
        row->size -= char_len;
        memmove(&row->hl[prev_pos], &row->hl[E.cx], row->size - prev_pos);
        int new_hl_size = row->size > 0 ? row->size : 1;
        unsigned char *new_hl = realloc(row->hl, new_hl_size);
        if (new_hl == NULL) die("realloc");
        row->hl = new_hl;
        E.cx = prev_pos;
        E.dirty = 1;
    }
}

void insert_utf8_character(const char *utf8_char, int char_len) {
    if (E.cy == E.numrows) {
        append_row("", 0);
    }
    erow *row = &E.row[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > row->size) E.cx = row->size;
    if (!utf8_is_char_boundary(row->chars, E.cx)) {
        E.cx = utf8_prev_char_boundary(row->chars, E.cx);
    }
    int codepoint;
    int decoded_len = utf8_decode(utf8_char, char_len, &codepoint);
    if (decoded_len != char_len || !utf8_is_valid(codepoint)) {
        return;
    }
    char *new = realloc(row->chars, row->size + char_len + 1);
    if (new == NULL) die("realloc");
    row->chars = new;
    memmove(&row->chars[E.cx + char_len], &row->chars[E.cx], row->size - E.cx + 1);
    memcpy(&row->chars[E.cx], utf8_char, char_len);
    row->size += char_len;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->hl, new_hl_size);
    if (new_hl == NULL) die("realloc");
    row->hl = new_hl;
    memmove(&row->hl[E.cx + char_len], &row->hl[E.cx], row->size - E.cx - char_len);
    for (int i = 0; i < char_len; i++) {
        row->hl[E.cx + i] = HL_NORMAL;
    }
    E.cx += char_len;
    E.dirty = 1;
}

void yank_line() {
    if (E.cy < 0 || E.cy >= E.numrows) return;
    erow *row = &E.row[E.cy];
    char header[] = "\x1b]52;c;";
    char trailer[] = "\x07";
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = row->size;
    char *encoded = malloc(((len + 2) / 3) * 4 + 1);
    if (!encoded) return;
    int j = 0;
    for (int i = 0; i < len; i += 3) {
        int n = row->chars[i] << 16;
        if (i + 1 < len) n |= row->chars[i + 1] << 8;
        if (i + 2 < len) n |= row->chars[i + 2];
        encoded[j++] = b64[(n >> 18) & 63];
        encoded[j++] = b64[(n >> 12) & 63];
        encoded[j++] = (i + 1 < len) ? b64[(n >> 6) & 63] : '=';
        encoded[j++] = (i + 2 < len) ? b64[n & 63] : '=';
    }
    encoded[j] = '\0';
    write(STDOUT_FILENO, header, strlen(header));
    write(STDOUT_FILENO, encoded, strlen(encoded));
    write(STDOUT_FILENO, trailer, strlen(trailer));
    free(encoded);
    char msg[512];
    int display_len = (row->size > 50) ? 50 : row->size;
    snprintf(msg, sizeof(msg), "Yanked: '%.*s%s'", display_len, row->chars, (row->size > 50) ? "..." : "");
    display_message(1, msg);
}

void delete_line() {
    if (E.numrows > 0 && E.cy >= 0 && E.cy < E.numrows) {
        erow *row = &E.row[E.cy];
        char saved_line[512];
        int display_len = (row->size > 50) ? 50 : row->size;
        snprintf(saved_line, sizeof(saved_line), "%.*s%s", display_len, row->chars, (row->size > 50) ? "..." : "");
        delete_row(E.cy);
        if (E.numrows == 0) {
            E.cy = 0;
        } else if (E.cy >= E.numrows) {
            E.cy = E.numrows - 1;
        }
        if (E.cy < 0) E.cy = 0;
        E.cx = 0;
        E.dirty = 1;
        char msg[600];
        snprintf(msg, sizeof(msg), "Deleted: '%s'", saved_line);
        display_message(2, msg);
    }
}