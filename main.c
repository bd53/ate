#include <locale.h>
#include <stdlib.h>

#define GLOBALS

#include "common.h"
#include "content.h"
#include "display.h"
#include "file.h"
#include "keybinds.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    enable_raw_mode();
    if (get_window_size(&Editor.editor_rows, &Editor.editor_cols) == -1) die("get_window_size");
    Editor.editor_rows -= 2;
    if (argc >= 2) open_editor(argv[1]);
    if (Editor.buffer_rows == 0) append_row("", 0);
    while (1) {
        refresh_screen();
        process_keypress();
    }
    return 0;
}