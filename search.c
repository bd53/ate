#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "display.h"
#include "search.h"

void editorFindNext(int direction) {
    if (E.query == NULL || strlen(E.query) == 0) return;
    E.match_row = -1;
    E.match_col = -1;
    if (E.numrows == 0) return;
    int start_row = E.search_start_row;
    int start_col = E.search_start_col;
    if (direction == 1) {
        for (int i = 0; i < E.numrows; i++) {
            int r = (start_row + i) % E.numrows;
            if (r < 0 || r >= E.numrows) break;
            erow *row = &E.row[r];
            int search_offset = (r == start_row) ? start_col + 1 : 0;
            if (search_offset < 0) search_offset = 0;
            if (search_offset >= row->size) continue;
            char *match_ptr = strstr(row->chars + search_offset, E.query);
            if (match_ptr) {
                int match_col = match_ptr - row->chars;
                E.cy = r;
                E.cx = match_col;
                E.match_row = r;
                E.match_col = match_col;
                E.search_start_row = r;
                E.search_start_col = match_col;
                editorScroll();
                editorRefreshScreen();
                return;
            }
        }
    }
    else {
        for (int i = 0; i < E.numrows; i++) {
            int r = start_row - i;
            if (r < 0) r = E.numrows - 1;
            if (r < 0 || r >= E.numrows) break;
            erow *row = &E.row[r];
            if (row->chars == NULL) continue;
            int best_match_col = -1;
            char *p = row->chars;
            while (1) {
                char *match_ptr = strstr(p, E.query);
                if (match_ptr == NULL) break;
                int match_col = match_ptr - row->chars;
                if (r != start_row || match_col < start_col) {
                    best_match_col = match_col;
                }
                p = match_ptr + 1;
                if (p >= row->chars + row->size) break;
            }
            if (best_match_col != -1) {
                E.cy = r;
                E.cx = best_match_col;
                E.match_row = r;
                E.match_col = best_match_col;
                E.search_start_row = r;
                E.search_start_col = best_match_col;
                editorScroll();
                editorRefreshScreen();
                return;
            }
        }
    }
}

void editorFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    E.match_row = -1;
    E.match_col = -1;
    char *query = editorPrompt("Find text: %s (ESC to cancel)");
    if (query == NULL) {
        E.cx = saved_cx;
        E.cy = saved_cy;
        editorRefreshScreen();
        return;
    }
    if (E.query) free(E.query);
    E.query = query;
    E.search_start_row = saved_cy;
    E.search_start_col = saved_cx;
    editorFindNext(1);
    editorRefreshScreen();
}
