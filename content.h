#ifndef CONTENT_H
#define CONTENT_H

#include <stddef.h>

void editorContentInsertRow(int at, char *s, size_t len);
void editorContentAppendRow(char *s, size_t len);
void editorDelRow(int at);
void editorContentInsertChar(char c);
void editorContentInsertNewline();
void editorFreeRows();
void editorFreeFileEntries();
void editorCleanup();

#endif
