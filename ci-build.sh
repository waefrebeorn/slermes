#!/bin/bash
# ci-build.sh — Direct C compilation for CI (bypasses Makefile pattern rule issues)
# Usage: bash ci-build.sh
set -euo pipefail

SRCDIR="$PWD"
echo "=== CI Build: Direct compilation ==="

# Collect all include paths once
INC_PATHS="-I include"
INC_PATHS="$INC_PATHS $(find lib -maxdepth 2 -name '*.h' -exec dirname {} \; | sort -u | sed 's/^/-I /')"
INC_PATHS="$INC_PATHS -I lib/libncurses/include"
INC_WHISPER="-I lib/whisper_cpp/include"

# Common CFLAGS
CFLAGS="-O2 -g -Wall -w $(echo $INC_PATHS)"

echo "[1/3] Compiling library sources..."
find lib -name '*.c' \
  ! -path '*/whisper_cpp/*' \
  ! -path '*/libncurses/*' \
  ! -path '*/lib/syslib/*' | sort | while read -r src; do
    obj="${src%.c}.o"
    if [ ! -f "$obj" ]; then
        gcc $CFLAGS -c "$src" -o "$obj" 2>&1 | tail -1
    fi
done

echo "[2/3] Compiling main sources..."
find src -name '*.c' ! -path '*/plugins/*' | sort | while read -r src; do
    obj="${src%.c}.o"
    if [ ! -f "$obj" ]; then
        gcc $CFLAGS -c "$src" -o "$obj" 2>&1 | tail -1
    fi
done

echo "[3/3] Linking..."
OBJS=$(find src lib -name '*.o' ! -path '*/plugins/*' ! -path '*/libncurses/*' | sort -u | tr '\n' ' ')
gcc -O2 -g -o slermes $OBJS \
    -lssl -lcrypto -ldl -lpthread -lz -lm -lstdc++ \
    /usr/lib/x86_64-linux-gnu/libncursesw.so.6 \
    /usr/lib/x86_64-linux-gnu/libpanelw.so.6 \
    /usr/lib/x86_64-linux-gnu/libtinfo.so.6 2>&1 | tail -5

echo "=== Build complete: $(ls -lh slermes | awk '{print $5}') ==="
file slermes
./slermes --version
