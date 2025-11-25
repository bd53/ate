#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf8.h"
#include "util.h"

// https://github.com/torvalds/uemacs/blob/master/utf8.c#L16
unsigned utf8_to_unicode(const char *line, unsigned index, unsigned len, unicode_t *res) {
    unsigned value;
    unsigned char c;
    unsigned bytes, mask, i;
    if (line == NULL || res == NULL || index >= len) {
        if (res) *res = 0;
        return 0;
    }
    c = line[index];
    *res = c;
    line += index;
    len -= index;
    if (c < 0x80) return 1;
    if (c < 0xc0) return 1;
    mask = 0x20;
    bytes = 2;
    while (c & mask) {
        bytes++;
        mask >>= 1;
    }
    if (bytes > 4 || bytes > len) return 1;
    value = c & (mask - 1);
    for (i = 1; i < bytes; i++) {
        c = line[i];
        if ((c & 0xc0) != 0x80) return 1;
        value = (value << 6) | (c & 0x3f);
    }
    if ((bytes == 2 && value < 0x80) || (bytes == 3 && value < 0x800) || (bytes == 4 && value < 0x10000)) return 1;
    if (value >= 0xD800 && value <= 0xDFFF) return 1;
    if (value > 0x10FFFF) return 1;
    *res = value;
    return bytes;
}

// https://github.com/torvalds/uemacs/blob/master/utf8.c#L60
static void reverse_string(char *begin, char *end) {
    while (begin < end) {
        char temp = *begin;
        *begin = *end;
        *end = temp;
        begin++;
        end--;
    }
}

// https://github.com/torvalds/uemacs/blob/master/utf8.c#L80
unsigned unicode_to_utf8(unsigned int c, char *utf8) {
    if (utf8 == NULL) return 0;
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) return 0;
    if (c <= 0x7F) {
        *utf8 = (char)c;
        return 1;
    }
    int bytes = 1;
    int prefix = 0x40;
    char *p = utf8;
    do {
        *p++ = 0x80 | (c & 0x3f);
        bytes++;
        prefix >>= 1;
        c >>= 6;
    } while (c > prefix);
    *p = c | (~(prefix * 2 - 1) & 0xFF);
    reverse_string(utf8, p);
    return bytes;
}

int utf8_char_len(const char *s, int max_len) {
    if (s == NULL || max_len <= 0) return 0;
    unsigned char c = (unsigned char)*s;
    if (c < 0x80) return 1;
    if (c < 0xC0) return 0;
    if ((c & 0xE0) == 0xC0) return (max_len >= 2) ? 2 : 0;
    if ((c & 0xF0) == 0xE0) return (max_len >= 3) ? 3 : 0;
    if ((c & 0xF8) == 0xF0) return (max_len >= 4) ? 4 : 0;
    return 0;
}

int utf8_decode(const char *s, int max_len, int *codepoint) {
    if (s == NULL || codepoint == NULL || max_len <= 0) return -1;
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

int utf8_encode(int codepoint, char *out) {
    if (out == NULL) return 0;
    return unicode_to_utf8((unsigned int)codepoint, out);
}

int utf8_is_valid(int codepoint) {
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return 0;
    if (codepoint >= 0x00 && codepoint <= 0x10FFFF) return 1;
    return 0;
}

int utf8_is_char_boundary(const char *s, int byte_offset) {
    if (s == NULL || byte_offset < 0) return 1;
    if (s[byte_offset] == '\0') return 1;
    unsigned char c = (unsigned char)s[byte_offset];
    return (c & 0xC0) != 0x80;
}

int utf8_prev_char_boundary(const char *s, int byte_offset) {
    if (s == NULL || byte_offset <= 0) return 0;
    byte_offset--;
    while (byte_offset > 0 && !utf8_is_char_boundary(s, byte_offset)) byte_offset--;
    return byte_offset;
}

int utf8_next_char_boundary(const char *s, int byte_offset, int max_len) {
    if (s == NULL || byte_offset < 0) return 0;
    if (byte_offset >= max_len) return max_len;
    if (!utf8_is_char_boundary(s, byte_offset)) {
        byte_offset++;
        while (byte_offset < max_len && !utf8_is_char_boundary(s, byte_offset)) byte_offset++;
        return byte_offset;
    }
    int remaining_len = max_len - byte_offset;
    int len = utf8_char_len(&s[byte_offset], remaining_len);
    if (len == 0) return (byte_offset + 1 > max_len) ? max_len : byte_offset + 1;
    byte_offset += len;
    return (byte_offset > max_len) ? max_len : byte_offset;
}

void insert_utf8_character(const char *utf8_char, int char_len) {
    if (Editor.cursor_y == Editor.buffer_rows) append_row("", 0);
    struct Row *row = &Editor.row[Editor.cursor_y];
    if (Editor.cursor_x < 0) Editor.cursor_x = 0;
    if (Editor.cursor_x > row->size) Editor.cursor_x = row->size;
    if (!utf8_is_char_boundary(row->chars, Editor.cursor_x)) Editor.cursor_x = utf8_prev_char_boundary(row->chars, Editor.cursor_x);
    int codepoint;
    int decoded_len = utf8_decode(utf8_char, char_len, &codepoint);
    if (decoded_len != char_len || !utf8_is_valid(codepoint)) return;
    char *new_chars = realloc(row->chars, row->size + char_len + 1);
    if (!new_chars) die("realloc");
    row->chars = new_chars;
    memmove(&row->chars[Editor.cursor_x + char_len], &row->chars[Editor.cursor_x], row->size - Editor.cursor_x + 1);
    memcpy(&row->chars[Editor.cursor_x], utf8_char, char_len);
    row->size += char_len;
    row->chars[row->size] = '\0';
    int new_hl_size = row->size > 0 ? row->size : 1;
    unsigned char *new_hl = realloc(row->state, new_hl_size);
    if (!new_hl) die("realloc");
    row->state = new_hl;
    memmove(&row->state[Editor.cursor_x + char_len], &row->state[Editor.cursor_x], row->size - Editor.cursor_x - char_len);
    for (int i = 0; i < char_len; i++) row->state[Editor.cursor_x + i] = 0;
    Editor.cursor_x += char_len;
    Editor.modified = 1;
}
