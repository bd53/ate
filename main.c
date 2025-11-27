#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ebind.h"
#include "efunc.h"
#include "estruct.h"
#include "util.h"
#include "version.h"
#include "search.h"

static void usage(void)
{
        printf("%s --version | output version information\n", PROGRAM_NAME);
        printf("%s --help    | display this help\n", PROGRAM_NAME);
        exit(0);
}

int main(int argc, char *argv[])
{
        setlocale(LC_ALL, "");
        if (argc >= 2) {
                if (strcmp(argv[1], "--version") == 0) {
                        version();
                        exit(0);
                }
                if (strcmp(argv[1], "--help") == 0) {
                        usage();
                }
        }
        ttopen();
        editor.lines = malloc(sizeof(char *));
        editor.lines[0] = strdup("");
        editor.line_numbers = 1;
        editor.cursor_x = 0;
        editor.cursor_y = 0;
        editor.offset_y = 0;
        editor.filename = NULL;
        editor.modified = 0;
        if (argc >= 2) {
                browse_mode = 0;
                load_file(argv[1]);
        } else {
                browse_mode = 1;
                init_filetree(".");
        }
        while (1) {
                if (help_mode) {
                        render_help();
                } else if (search_mode) {
                        render_search_interface();
                } else if (browse_mode) {
                        render_filetree();
                } else {
                        refresh_screen();
                }
                char c;
                if (read(STDIN_FILENO, &c, 1) != 1)
                        continue;
                if (!process_keypress(c)) {
                        break;
                }
        }
        if (search_mode) {
                free_search();
        }
        if (browse_mode) {
                free_filetree();
        }
        cleanup_help();
        for (int i = 0; i < editor.line_numbers; i++) {
                free(editor.lines[i]);
        }
        free(editor.lines);
        free(editor.filename);
        ttclose();
        return 0;
}
