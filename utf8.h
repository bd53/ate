#ifndef UTF8_H
#define UTF8_H

typedef unsigned int unicode_t;

unsigned utf8_to_unicode(const char *line, unsigned index, unsigned len, unicode_t *res);
unsigned unicode_to_utf8(unsigned int c, char *utf8);
int utf8_char_len(const char *s, int max_len);
int utf8_decode(const char *s, int max_len, int *codepoint);
int utf8_encode(int codepoint, char *out);
int utf8_is_valid(int codepoint);
int utf8_is_char_boundary(const char *s, int byte_offset);
int utf8_prev_char_boundary(const char *s, int byte_offset);
int utf8_next_char_boundary(const char *s, int byte_offset, int max_len);

#endif
