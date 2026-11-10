#!/bin/bash
# gate_file_type.sh — drive the stdin-consuming file_type harness (it reads op
#    lines from stdin, so it cannot hang off run_one_oracle.sh's no-stdin pipe),
#    feeding a curated fixture set and piping the JSONL into the oracle.
set -e
cd "$(dirname "$0")/.."
mkdir -p .testbin
PYBIN=$(for cand in python python3 \
  /c/Users/*/AppData/Local/hermes/tools/python-*/python.exe; do
  [ -x "$cand" ] && "$cand" -c "import hashlib;hashlib.sha256(b'x').hexdigest()" >/dev/null 2>&1 && { echo "$cand"; break; }
done)
if [ -z "$PYBIN" ]; then echo "no python with working hashlib" >&2; exit 1; fi
EXE=.testbin/tt_file_type
if [ ! -x "$EXE" ]; then
  # the port .o drags sibling fns (is_write_denied, hermes_log,
  # file_text_ops_detect_line_ending), so give the linker their objects.
  gcc -w -O2 -std=gnu11 -D_GNU_SOURCE -I include/compat -I include -I src -I lib/libjson \
    tests/t_port_file_type.c src/tools/file_fs_ops.o src/agent/file_safety.o \
    src/tools/file_text_ops.o src/agent/logger.o lib/libjson/json.o \
    -lstdc++ -lm -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 \
    -lole32 -luuid -lz -lssl -lcrypto -lSDL2main -lSDL2 -lSDL2_ttf -ltre \
    -Wl,--allow-multiple-definition -o "$EXE"
fi
# curated op-line set: extension tokens (no real files -> both sides reduce to
# extension-set membership), plus edge cases for temp paths.
cat > ".testbin/ft_fixtures.txt" <<'EOF'
image /tmp/x.PNG
image /tmp/x.png
image report.pdf
image /a/b/c/d.webp
image notes.txt
image README.MD
image "spaces name.jpeg"
image /tmp/x
binary /tmp/x.png
binary /tmp/x.exe
binary archive.tar.gz
binary data.bin
binary text.txt
binary /tmp/.jpg
binary script.py
binary image.jpeg
binary "x  y.gif"
binary noext
binary ""
EOF
SLERMES_HOME="$(pwd)/.testbin/ft_tmp" HOME="$(pwd)/.testbin/ft_tmp" \
  "$EXE" < ".testbin/ft_fixtures.txt" > ".testbin/ft_lines.txt" 2>&1
"$PYBIN" "tests/oracle_file_type.py" < ".testbin/ft_lines.txt"
RC=$?
rm -rf .testbin/ft_tmp
exit $RC
