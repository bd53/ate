#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"
#include "content.h"
#include "display.h"
#include "health.h"
#include "keybinds.h"
#include "utils.h"

static const char* get_status_icon(enum HealthStatus status) {
    switch (status) {
        case HEALTH_INFO: return "✅ ";
        case HEALTH_WARNING: return "⚠️ ";
        default: return "? ";
    }
}

static void append_health_line(enum HealthStatus status, const char *format, ...) {
    char message[500];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    char line[512];
    snprintf(line, sizeof(line), "  %s %s", get_status_icon(status), message);
    append_row(line, strlen(line));
}

static int command_exists(const char *cmd) {
    char path_cmd[512];
    snprintf(path_cmd, sizeof(path_cmd), "command -v %s > /dev/null 2>&1", cmd);
    return system(path_cmd) == 0;
}

static char* get_command_version(const char *cmd, const char *version_flag) {
    static char version[256];
    char command[512];
    snprintf(command, sizeof(command), "%s %s 2>&1 | head -n 1", cmd, version_flag);
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        strcpy(version, "unknown");
        return version;
    }
    if (fgets(version, sizeof(version), fp) != NULL) {
        version[strcspn(version, "\n")] = 0;
    } else {
        strcpy(version, "unknown");
    }
    pclose(fp);
    return version;
}

static const char* get_env_var(const char *name) {
    const char *val = getenv(name);
    return val ? val : "not set";
}

static void check_tool(const ToolCheck *tool) {
    if (command_exists(tool->cmd)) {
        char *version = get_command_version(tool->cmd, tool->version_flag);
        append_health_line(tool->found_status, "%s: %s", tool->name, version);
    } else {
        append_health_line(tool->missing_status, "%s: not found", tool->name);
    }
}

void check_health() {
    run_cleanup();
    append_row("==============================================================================", 70);
    append_row("default:", 8);
    append_row(" ", 0);
    append_health_line(HEALTH_INFO, "Shell: %s", get_env_var("SHELL"));
    append_health_line(HEALTH_INFO, "Terminal: %s", get_env_var("TERM"));
    append_row(" ", 0);
    append_row("==============================================================================", 70);
    append_row("utility:", 8);
    append_row(" ", 0);
    ToolCheck tools[] = {
        {"CMake", "cmake", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"GCC", "gcc", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"Java", "java", "-version", HEALTH_INFO, HEALTH_WARNING},
        {"Lua", "lua", "-v", HEALTH_INFO, HEALTH_WARNING},
        {"Make", "make", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"Node.js", "node", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"PHP", "php", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"Python 3", "python3", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"Ruby", "ruby", "--version", HEALTH_INFO, HEALTH_WARNING},
        {"Rust", "rustc", "--version", HEALTH_INFO, HEALTH_WARNING},
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
        check_tool(&tools[i]);
    }
    append_row("", 0);
    E.is_help_view = 1;
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    refresh_screen();
}
