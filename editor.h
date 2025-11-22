#ifndef EDITOR_H
#define EDITOR_H

#include "common.h"

void editorFreeRows();
void editorCleanup();
void editorContentInsertRow(int at, char *s, size_t len);
void editorContentAppendRow(char *s, size_t len);
void editorDelRow(int at);
void editorContentInsertChar(char c);
void editorContentInsertNewline();
int editorOpen(char *filename);
void editorOpenDir(char *path);
void editorSave();
void editorOpenFile();
void editorOpenHelp();
char *editorPrompt(const char *prompt);
void editorScroll();
void editorFindNext(int direction);
void editorFind();
void editorViewDrawContent(struct buffer *ab);
void editorViewDrawStatusBar(struct buffer *ab);
void editorRefreshScreen();
void editorExecuteCommand(char *cmd);
void editorCommandMode();
void editorBuildFileTree(const char *root_path);
void editorFileTreeOpen();
void editorToggleFileTree();
void initEditor();

#endif
