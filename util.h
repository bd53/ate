#ifndef UTIL_H
#define UTIL_H

#include "efunc.h"

void run_cleanup(void);
void die(const char *s);
void *safe_malloc(size_t size);
void *safe_realloc(void *ptr, size_t size);
int fetch(int *rows, int *cols);
void append(struct Buffer *ab, const char *s, int len);
int calculate(void);

#endif
