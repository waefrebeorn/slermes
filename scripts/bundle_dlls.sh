#!/usr/bin/env bash
# Bundle the non-system DLLs the Windows binary needs beside it, then prove it starts.
#
# THE FAILURE THIS ADDRESSES: slermes.exe exits 127 and prints NOTHING when its DLLs are
# not findable. Not a crash, not an error -- silence plus 127, which every script and every
# gate reads as "not run". `objdump -p` names 7 direct deps and `ldd` finds 20 with their
# transitive closure; copying only the 7 still gives 127, which is why this resolves the
# full closure instead of a hand-written list.
set -u
cd "$(dirname "$0")/.." || exit 1

EXE=slermes.exe
MINGW=${MINGW:-/c/msys64/mingw64}

[ -f "$EXE" ] || { echo "bundle-dlls: no $EXE (build first)"; exit 2; }
[ -d "$MINGW/bin" ] || { echo "bundle-dlls: no $MINGW/bin"; exit 2; }

# Resolve the closure with a PATH that can see the toolchain, copy each dependency here.
PATH="$MINGW/bin:$PATH" ldd "$EXE" 2>/dev/null \
  | awk -F'=> ' '{print $2}' | awk '{print $1}' \
  | grep -i "$MINGW" | sort -u \
  | while read -r dll; do
      [ -f "$dll" ] && cp -u "$dll" . 2>/dev/null
    done

# Verify the point of the exercise: no PATH help, real output, rc 0.
out=$(env -u PATH PATH=/usr/bin:/bin ./"$EXE" --version 2>&1); rc=$?
case "$rc:$out" in
  0:*slermes*) echo "bundle-dlls: OK ($(ls ./*.dll 2>/dev/null | wc -l) DLLs beside the exe)" ;;
  *) echo "bundle-dlls: FAILED rc=$rc out=${out:-<empty>}"; exit 1 ;;
esac
