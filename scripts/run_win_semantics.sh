#!/usr/bin/env bash
# run_win_semantics.sh — build and run tests/win_semantics/probe.c against the port.
#
# The probe links the real port archive (util_rename_replace and friends), so it
# measures the code that ships rather than a reimplementation of it. Compile with
# the project's three compat flags, exactly like a water-cup harness.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="/c/msys64/mingw64/bin:$PATH"
cd "$REPO" || exit 2

ARCHIVE="tests/watercup/build/_all_objects.a"
OUT="tests/win_semantics/probe.exe"

# The archive is older than any freshly built object unless the gate runner rebuilt
# it; do it here so the probe sees the current port.
NEWEST=$(ls -t src/*/*.o src/*/*/*.o lib/*/*.o 2>/dev/null | head -1)
if [ ! -f "$ARCHIVE" ] || { [ -n "$NEWEST" ] && [ "$NEWEST" -nt "$ARCHIVE" ]; }; then
  echo "-- rebuilding object archive"
  { find src lib -name '*.o' ! -name 'main.o' | tr '\n' '\0' | rm -f "$ARCHIVE"; xargs -0 -n 400 ar rcsP "$ARCHIVE"; } 2>/dev/null \
    || find src lib -name '*.o' ! -name 'main.o' > /tmp/_ws_members.txt
fi

echo "-- compiling probe"
# Same toolchain as the main build: winlibs gcc. MSYS2's gcc carries its own
# string.h, and hermes_win32.h's `static strndup` then collides with it.
CC="${CC:-C:/winlibs/mingw64/bin/gcc.exe}"
"$CC" -I include -I lib -I lib/libdatetime -I lib/libjson -I src -I include/compat -I C:/winlibs/extra-inc -include hermes_win32.h -D_GNU_SOURCE \
    -DHERMES_VERSION='"0.0.0-slermes"' -o "$OUT" tests/win_semantics/probe.c \
    "$ARCHIVE" -Wl,--allow-multiple-definition -Wl,--start-group -lm -lws2_32 \
    -L C:/winlibs/extra-libs -lssl -lcrypto -lz -lpcre2-8 -ltre -lstdc++ \
    -Wl,--end-group 2>&1 | head -20
[ "${PIPESTATUS[0]}" -eq 0 ] || exit 3

echo "-- running"
"$OUT"
rc=$?
echo "-- exit $rc"
exit $rc
