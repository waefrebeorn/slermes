#!/usr/bin/env bash
# slerm-Lua (slv_*) test gate.
#
# The interpreter was ported from the WWW repository (~WuBu~/src/lv) into
# src/lv/ with an slv_ prefix. It is self-contained: standard C headers only,
# no engine dependency, so it builds with a plain gcc line.
#
#   tests/t_slv_test.c         core interpreter   (arena/value/lex/parse/eval)
#   tests/t_slv_script_test.c  gamemode contract  (host cfns + a real script)
#
# The script harness reads games/gamemode_demo.lua, so run it from the repo
# root (the same way run_all_oracles.sh is invoked).
set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO" || exit 2

# shellcheck disable=SC1091
export PATH="/mingw64/bin:/usr/bin:$PATH"

CC="${CC:-gcc}"
CFLAGS="-O2 -g -Wall -Werror=implicit-function-declaration -I src/lv"
OUT="${TMPDIR:-/tmp}/slv_tests.$$"
mkdir -p "$OUT"

fail=0

for t in t_slv_test t_slv_script_test; do
    if ! $CC $CFLAGS src/lv/slv_*.c "tests/$t.c" -o "$OUT/$t.exe" -lm 2> "$OUT/$t.err"; then
        echo "FAIL $t (build)"
        sed -n '1,10p' "$OUT/$t.err"
        fail=1
        continue
    fi
    if "$OUT/$t.exe" > "$OUT/$t.out" 2>&1; then
        echo "PASS $t  $(tr -d '\r' < "$OUT/$t.out" | grep -oE '[0-9]+ pass, [0-9]+ fail' | tail -1)"
    else
        echo "FAIL $t  $(tr -d '\r' < "$OUT/$t.out" | tail -2 | tr '\n' ' ')"
        fail=1
    fi
done

rm -rf "$OUT"
exit $fail
