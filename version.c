#include <stdio.h>

#include "version.h"

void version() {
    printf("%s version %s\n", PROGRAM_NAME_LONG, VERSION);
}