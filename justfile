program := "ate"
cc := "cc"
warnings := "-Wall -Wstrict-prototypes"
cflags := "-O2 " + warnings + " -g"
libs := "-lncurses"

default: clean build run

all: clean build run

build:
    #!/bin/env bash
    set -euo pipefail
    echo "Finding source files..."
    c_files=$(find . -name '*.c' ! -name 'test.c')
    echo "Compiling..."
    for src in $c_files; do
        obj="${src%.c}.o"
        echo " $src"
        {{cc}} {{cflags}} -D_GNU_SOURCE -c "$src" -o "$obj"
    done
    echo "Linking object files..."
    o_files=$(find . -name '*.o')
    {{cc}} -o {{program}} $o_files {{libs}}
    echo "Build complete: {{program}}"

run:
    @echo "Running {{program}}..."
    @./{{program}}

clean:
    #!/bin/env bash
    echo "Cleaning build artifacts..."
    rm -f {{program}}
    find . -name '*.o' -type f -delete
    echo "Clean complete"