#!/bin/bash
# gate_fuzzy_match_helpers.sh — drive the stdin-consuming harness, pipe to oracle.
set -e
cd "$(dirname "$0")/.."
mkdir -p .testbin/fmh_tmp
PYBIN=$(for cand in python python3 \
  /c/Users/*/AppData/Local/hermes/tools/python-*/python.exe; do
  [ -x "$cand" ] && "$cand" -c "import hashlib;hashlib.sha256(b'x').hexdigest()" >/dev/null 2>&1 && { echo "$cand"; break; }
done)
echo "PYBIN=$PYBIN"
# build the harness if stale/missing (this harness consumes its own stdin, so
# it cannot hang off run_one_oracle.sh's no-stdin pipe — standalone gate)
if [ ! -x ./.testbin/tt_fmh ]; then
  gcc -w -O2 -std=gnu11 -D_GNU_SOURCE -I include -I src -I lib/libjson \
    tests/t_port_fuzzy_match_helpers.c src/cli/port_fuzzy_match_helpers.o \
    lib/libjson/json.o -lstdc++ -lm -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 \
    -luser32 -lshell32 -lole32 -luuid -lz -lssl -lcrypto -lSDL2main -lSDL2 \
    -lSDL2_ttf -Wl,--allow-multiple-definition -o ./.testbin/tt_fmh
fi
# feed input cases (one op-value line per case)
cat > .testbin/fmh_in.txt <<'IN'
norm plain no maps here
norm "smart“quote” em—dash ellipsis… nbsp  x
first hello world
first 

second line here
first 
   
   indented third
lead hello
lead 	indent
IN
# run harness per line, collect JSONL
: > .testbin/fmh_cases.jsonl
while IFS='' read -r ln; do
  [ -z "$ln" ] && continue
  printf '%s\n' "$ln" | HOME="$PWD/.testbin/fmh_tmp" ./.testbin/tt_fmh >> .testbin/fmh_cases.jsonl 2>/dev/null
done < .testbin/fmh_in.txt
echo "=== cases (count: $(wc -l < .testbin/fmh_cases.jsonl)) ==="
cat .testbin/fmh_cases.jsonl
echo "=== oracle ==="
"$PYBIN" tests/oracle_fuzzy_match_helpers.py < .testbin/fmh_cases.jsonl
echo "GATE_EXIT=$?"