#include <stdlib.h>
#include <locale.h>
#include "content.h"
#include "display.h"
#include "file.h"
#include "init.h"
#include "keybinds.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    enable_raw_mode();
    editor_init();
    if (argc >= 2) {
        open_editor(argv[1]);
    }
    if (E.numrows == 0) {
        append_row("", 0);
    }
    while (1) {
        refresh_screen();
        process_keypress();
    }
    return 0;
}