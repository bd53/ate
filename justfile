c := "gcc"
cflags := "-Wall -Wextra -O2 -Iinclude"
ldflags := "-lncurses"
target := "ate"

default: clean build run

all: clean build run

build:
    #!/bin/env bash
    set -euo pipefail
    echo "Finding C source files..."
    c_files=$(find . -name '*.c' ! -name 'test.c')
    echo "Compiling source files..."
    for src in $c_files; do
        obj="${src%.c}.o"
        dir=$(dirname "$obj")
        mkdir -p "$dir"
        echo "  ✓ $src"
        {{c}} {{cflags}} -c "$src" -o "$obj"
    done
    echo "Linking object files..."
    o_files=$(find . -name '*.o')
    {{c}} {{cflags}} -o {{target}} $o_files {{ldflags}}
    echo "Build complete: {{target}}"

run:
    @echo "Running {{target}}..."
    @./{{target}}

clean:
    #!/bin/env bash
    echo "Cleaning build artifacts..."
    rm -f {{target}}
    find . -name '*.o' -type f -delete
    echo "Clean complete"
