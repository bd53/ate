#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ebind.h"
#include "efunc.h"
#include "util.h"
#include "version.h"

static void usage(void)
{
        printf("%s --version | output version information\n", PROGRAM_NAME);
        printf("%s --help    | display this help or view more information in the editor using :help\n", PROGRAM_NAME);
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
        ttopen(true);
        if (argc >= 2)
                display_editor(argv[1]);
        if (fetch(&Editor.editor_rows, &Editor.editor_cols) == -1)
                die("fetch");
        Editor.editor_rows -= 2;
        if (Editor.buffer_rows == 0)
                append_row("", 0);
        while (1) {
                refresh_screen();
                process_keypress();
        }
        return 0;
}
