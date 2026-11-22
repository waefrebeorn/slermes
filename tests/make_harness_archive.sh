#!/usr/bin/env bash
# make_harness_archive.sh — pack the registered project objects into one
# static archive so a FULL harness link pulls only the objects it actually
# references, instead of being forced to load all ~2241 of them.
#
# Measured on this box (gateway_revive, MSYS2/gcc 16):
#   explicit objects:  ~9 s link, 19 MB image
#   archive, no --whole-archive: 2.5 s link, 217 KB image
# The archive is rebuilt only when a registered object is newer than it.
set -e
cd "$(dirname "$0")/.."

ARCHIVE=".testbin/libslermes_all.a"
RSP=".testbin/harness_full_objs.rsp"
mkdir -p .testbin
command -v ar >/dev/null 2>&1 || export PATH=/mingw64/bin:$PATH

{
  grep -hoE '[A-Za-z0-9_./\\-]+\.o' build/*.mk \
    | tr '\\' '/' | sed 's,^\./,,' | sort -u \
    | while read -r obj; do
        case "$obj" in
          */main.o|*/hash.o|*/hermes_regex.o|*/auxiliary_client_runtime.o) continue ;;
        esac
        if [ -f "$obj" ]; then printf '%s\n' "$obj"; fi
      done
  [ -f .testbin/hash_O0.o ] && echo .testbin/hash_O0.o
  [ -f .testbin/hermes_regex_O0.o ] && echo .testbin/hermes_regex_O0.o
} > "$RSP"

# Never scan `tests`: a fixture submodule there holds ~19k .o files and a
# full-tree find costs 4.5 s per invocation. Only src/lib objects can enter
# the archive; the two SAC-safe -O0 objects live in .testbin and are checked
# explicitly below.
if [ -f "$ARCHIVE" ] \
   && [ -z "$(find src lib -name '*.o' -newer "$ARCHIVE" -print -quit 2>/dev/null)" ] \
   && { [ ! -f .testbin/hash_O0.o ] || [ ! .testbin/hash_O0.o -nt "$ARCHIVE" ]; } \
   && { [ ! -f .testbin/hermes_regex_O0.o ] || [ ! .testbin/hermes_regex_O0.o -nt "$ARCHIVE" ]; }; then
  echo "$ARCHIVE"
  exit 0
fi

TMP="$ARCHIVE.tmp.$$"
rm -f "$TMP"
ar rcs "$TMP" @"$RSP"
mv -f "$TMP" "$ARCHIVE"
echo "$ARCHIVE"
