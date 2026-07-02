#!/bin/bash
# ci-build.sh — Direct C compilation for CI
# Usage: bash ci-build.sh
# Bypasses Makefile to avoid pattern-rule issues on GitHub Actions Ubuntu 24.04
set -euo pipefail

echo "=== CI Build: Direct C compilation ==="
ROOT="$PWD"

# Collect all include directories that have .h files
INCS="-I $ROOT/include"
while IFS= read -r dir; do
    INCS="$INCS -I $dir"
done < <(find "$ROOT/lib" -maxdepth 2 -name '*.h' -exec dirname {} \; | sort -u)

CFLAGS="-O2 -g -Wall $INCS"

echo "[1/3] Compiling library sources..."
LIB_SRCS=$(find "$ROOT/lib" -name '*.c' \
    ! -path '*/whisper_cpp/*' \
    ! -path '*/libncurses/*' \
    ! -path '*/lib/syslib/*' 2>/dev/null | sort || true)
for src in $LIB_SRCS; do
    obj="${src%.c}.o"
    if [ ! -f "$obj" ]; then
        echo "  CC $(basename $src)"
        gcc $CFLAGS -c "$src" -o "$obj" 2>&1
    fi
done

echo "[2/3] Compiling application sources..."
APP_SRCS=$(find "$ROOT/src" -name '*.c' ! -path '*/plugins/*' 2>/dev/null | sort || true)
for src in $APP_SRCS; do
    obj="${src%.c}.o"
    if [ ! -f "$obj" ]; then
        echo "  CC $(basename $src)"
        gcc $CFLAGS -c "$src" -o "$obj" 2>&1
    fi
done

echo "[3/3] Linking..."
OBJS=$(find "$ROOT/src" "$ROOT/lib" -name '*.o' \
    ! -path '*/plugins/*' \
    ! -path '*/libncurses/*' \
    ! -path '*/lib/syslib/*' 2>/dev/null | sort -u | tr '\n' ' ')

gcc -O2 -g -o "$ROOT/slermes" $OBJS \
    -lssl -lcrypto -ldl -lpthread -lz -lm -lstdc++ 2>&1

echo "=== Build complete: $(ls -lh "$ROOT/slermes" | awk '{print $5}') ==="
file "$ROOT/slermes"
"$ROOT/slermes" --version
