#ifndef UTIL_H
#define UTIL_H

#include "efunc.h"

void run_cleanup(void);
void die(const char *s);
int fetch(int *rows, int *cols);
void append(struct Buffer *ab, const char *s, int len);
int calculate(void);

#endif
