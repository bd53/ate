#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>

#include "content.h"
#include "display.h"
#include "file.h"
#include "init.h"
#include "keybinds.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    if (E.numrows == 0) {
        editorContentAppendRow("", 0);
    }
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
