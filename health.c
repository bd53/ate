#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "health.h"
#include "keybinds.h"
#include "utils.h"

static void append_health_line(int is_ok, const char *format, ...) {
    char message[500];
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    const char *icon = is_ok ? "✅" : "⚠️";
    snprintf(line, sizeof(line), "- %s %s", icon, message);
    append_row(line, strlen(line));
}

static void append_advice(const char *format, ...) {
    char message[500];
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    snprintf(line, sizeof(line), "  - ADVICE:");
    append_row(line, strlen(line));
    snprintf(line, sizeof(line), "    - %s", message);
    append_row(line, strlen(line));
}

static void append_section_header(const char *name) {
    char header[256];
    append_row("", 0);
    snprintf(header, sizeof(header), "%s:", name);
    append_row(header, strlen(header));
    append_row("", 0);
}

static int check_command(const char *cmd, char *output_buf, size_t buf_size, const char *version_flag) {
    char command[512];
    FILE *fp = NULL;
    snprintf(command, sizeof(command), "command -v %s > /dev/null 2>&1", cmd);
    if (system(command) != 0) return 0;
    if (version_flag && output_buf) {
        snprintf(command, sizeof(command), "%s %s 2>&1 | head -n 1", cmd, version_flag);
        fp = popen(command, "r");
        if (fp) {
            if (fgets(output_buf, buf_size, fp) != NULL) {
                output_buf[strcspn(output_buf, "\n")] = 0;
            } else {
                strncpy(output_buf, "unknown", buf_size - 1);
                output_buf[buf_size - 1] = 0;
            }
            pclose(fp);
        } else {
            strncpy(output_buf, "unknown", buf_size - 1);
            output_buf[buf_size - 1] = 0;
        }
    }
    return 1;
}

static void check_core_utils() {
    char version[256];
    append_row("==============================================================================", 78);
    append_section_header("Core Utility");
    const char *tools[][2] = {
        {"unzip", ""},
        {"wget", "--version"},
        {"curl", "--version"},
        {"gzip", "--version"},
        {"tar", "--version"},
        {"bash", "--version"},
        {"sh", "--version"}
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        const char *cmd = tools[i][0];
        const char *flag = tools[i][1];
        if (check_command(cmd, version, sizeof(version), flag)) {
            append_health_line(1, "OK %s: %s", cmd, version);
        } else {
            append_health_line(0, "%s: not found", cmd);
        }
    }
}

static void check_language_support() {
    char version[256];
    append_row("", 0);
    append_row("==============================================================================", 78);
    append_section_header("Languages");
    const char *languages[][3] = {
        {"luarocks", "lua", ""},
        {"Ruby", "ruby", "--version"},
        {"Composer", "composer", "--version"},
        {"PHP", "php", "--version"},
        {"javac", "javac", "-version"},
        {"java", "java", "-version"},
        {"julia", "julia", "--version"},
        {"Go", "go", "version"},
        {"node", "node", "--version"},
        {"python", "python3", "--version"},
        {"cargo", "cargo", "--version"},
        {"npm", "npm", "--version"},
        {"pip", "pip3", "--version"}
    };
    for (size_t i = 0; i < sizeof(languages) / sizeof(languages[0]); i++) {
        const char *name = languages[i][0];
        const char *cmd = languages[i][1];
        const char *flag = languages[i][2];
        if (check_command(cmd, version, sizeof(version), flag)) {
            append_health_line(1, "OK %s: %s", name, version);
        } else {
            append_health_line(0, "WARNING %s: not available", name);
            append_advice("spawn: %s failed with exit code - and signal -. Could not find executable \"%s\" in PATH.", cmd, cmd);
            if (i < sizeof(languages) / sizeof(languages[0]) - 1) append_row("", 0);
        }
    }
}

static void check_os_info() {
    struct utsname sys_info;
    char line[256];
    append_row("", 0);
    append_row("==============================================================================", 78);
    append_section_header("OS Info");
    if (uname(&sys_info) == 0) {
        append_row("{", 1);
        snprintf(line, sizeof(line), "  machine = \"%s\",", sys_info.machine);
        append_row(line, strlen(line));
        snprintf(line, sizeof(line), "  release = \"%s\",", sys_info.release);
        append_row(line, strlen(line));
        snprintf(line, sizeof(line), "  sysname = \"%s\",", sys_info.sysname);
        append_row(line, strlen(line));
        snprintf(line, sizeof(line), "  version = \"%s\"", sys_info.version);
        append_row(line, strlen(line));
        append_row("}", 1);
    }
}

static void check_external_tools() {
    char version[256];
    char path[512];
    char command[512];
    FILE *fp;
    append_row("", 0);
    append_row("==============================================================================", 78);
    append_section_header("External Tools");
    const char *tools[][3] = {
        {"ripgrep", "rg", "--version"},
        {"fd", "fd", "--version"},
        {"fzf", "fzf", "--version"},
        {"git", "git", "--version"},
        {"make", "make", "--version"},
        {"cmake", "cmake", "--version"},
        {"gcc", "gcc", "--version"},
        {"clang", "clang", "--version"}
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        const char *name = tools[i][0];
        const char *cmd = tools[i][1];
        const char *flag = tools[i][2];
        if (check_command(cmd, NULL, 0, NULL)) {
            snprintf(command, sizeof(command), "command -v %s 2>/dev/null", cmd);
            fp = popen(command, "r");
            if (fp) {
                if (fgets(path, sizeof(path), fp)) {
                    path[strcspn(path, "\n")] = 0;
                } else {
                    strncpy(path, "unknown", sizeof(path) - 1);
                    path[sizeof(path) - 1] = 0;
                }
                pclose(fp);
            } else {
                strncpy(path, "unknown", sizeof(path) - 1);
                path[sizeof(path) - 1] = 0;
            }
            check_command(cmd, version, sizeof(version), flag);
            append_health_line(1, "OK %s: %s (%s)", name, version, path);
        } else {
            append_health_line(0, "WARNING %s: not found", name);
        }
    }
}

static void check_clipboard() {
    const char *clipboard_tools[] = {"xclip", "xsel", "wl-copy", "pbcopy"};
    int found = 0;
    append_row("", 0);
    append_row("==============================================================================", 78);
    append_section_header("Clipboard (optional)");
    for (size_t i = 0; i < sizeof(clipboard_tools) / sizeof(clipboard_tools[0]); i++) {
        if (check_command(clipboard_tools[i], NULL, 0, NULL)) {
            append_health_line(1, "OK Clipboard tool found: %s", clipboard_tools[i]);
            found = 1;
            break;
        }
    }
    if (!found) {
        append_health_line(0, "WARNING No clipboard tool found");
        append_advice("Install xclip, xsel, wl-clipboard, or similar.");
    }
}

void check_health() {
    run_cleanup();
    check_core_utils();
    check_language_support();
    check_os_info();
    check_external_tools();
    check_clipboard();
    Editor.help_view = 1;
    Editor.cursor_x = 0;
    Editor.cursor_y = 0;
    Editor.row_offset = 0;
    refresh_screen();
}