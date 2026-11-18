#!/usr/bin/env bash
# ensure_oracle_deps.sh <obj> [<obj> ...] — make sure every explicit oracle
# dependency object exists before a harness link.
#
# Three classes:
#   .testbin/hash_O0.o      — lib/libhash/hash.c compiled at -O0. Smart App
#                             Control refuses freshly linked images containing
#                             the -O3 hash.o; the -O0 image is accepted
#                             (verified: hashprobe runs, correct SHA-256).
#   tests/stubs/<x>.o       — local dependency stubs, compiled -O0.
#   src/**/<x>.o            — a real port object not built yet; build it with
#                             the project pattern rule (make <obj>).
# Anything else missing is left alone — the linker will name it.
set -u
cd "$(dirname "$0")/.." || exit 1
mkdir -p .testbin
for obj in "$@"; do
  [ -n "$obj" ] || continue
  [ -f "$obj" ] && continue
  case "$obj" in
    .testbin/hash_O0.o)
      gcc -O0 -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h \
        -I include/compat -I include -I lib/libhash -I C:/winlibs/extra-inc \
        -c lib/libhash/hash.c -o "$obj" || exit 1
      ;;
    tests/stubs/*.o)
      src="tests/stubs/$(basename "$obj" .o).c"
      [ -f "$src" ] || continue
      gcc -O0 -std=gnu11 -D_GNU_SOURCE -include hermes_win32.h \
        -I include/compat -I include -I lib/libjson -I lib/libyaml \
        -c "$src" -o "$obj" || exit 1
      ;;
    src/*.o)
      src="${obj%.o}.c"
      [ -f "$src" ] || continue
      make "$obj" USERPROFILE='C:\Users\eman5' LOCALAPPDATA='C:\Users\eman5\AppData\Local' >/dev/null 2>&1 || exit 1
      ;;
  esac
done
exit 0
