#!/usr/bin/env bash
# run_oracle_web_server_console_theme.sh — console frame protocol + theme
# cluster oracle. Builds a real dashboard-themes sandbox with user YAMLs.
set -uo pipefail
cd "$(dirname "$0")/../../.."

FIX_GLOB=tests/oracle/fixtures/web_server_console_theme/cases_*.in
HARNESS=/tmp/tt_wsct
ORACLE=tests/sta_oracle_web_server_console_theme.py

SBX=$(mktemp -d /tmp/wsct_sbx.XXXXXX)
mkdir -p "$SBX/dashboard-themes" "$SBX/empty"
cat > "$SBX/dashboard-themes/strike.yaml" <<'EOF'
name: strike
label: Strike Freedom
description: Gundam-inspired reskin
palette:
  background: "#0b0e1a"
  midground:
    hex: "#e8e4d8"
    alpha: 0.95
typography:
  fontSans: "Rajdhani, sans-serif"
  baseSize: "16px"
layout:
  radius: "0.25rem"
  density: compact
layoutVariant: cockpit
customCSS: ".hud{border:1px solid gold}"
EOF
cat > "$SBX/dashboard-themes/broken.yaml" <<'EOF'
: this is [not: valid yaml
  - {{{{
EOF
cat > "$SBX/dashboard-themes/noname.yaml" <<'EOF'
label: Nameless
palette:
  background: "#111111"
EOF
cat > "$SBX/dashboard-themes/default.yaml" <<'EOF'
name: default
label: Shadow Default
palette:
  background: "#000000"
EOF

LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o $HARNESS }
LINKCMD=${LINKCMD//src\/main.o /tests\/t_port_web_server_console_theme.c -Ilib }
eval "$LINKCMD" 2>/tmp/wsct_link.log || { echo "LINK FAILED"; tail -5 /tmp/wsct_link.log; rm -rf "$SBX"; exit 1; }
[ -x "$HARNESS" ] || { echo "MISSING harness"; rm -rf "$SBX"; exit 1; }

i=0 fails=0
while IFS= read -r line; do
  [ -z "$line" ] && continue
  i=$((i+1))
  printf '%s' "${line//@SBX@/$SBX}" > /tmp/wsct_case.json
  "$HARNESS" /tmp/wsct_case.json > /tmp/wsct_c.out 2>/dev/null
  python3 "$ORACLE" /tmp/wsct_case.json > /tmp/wsct_py.out 2>/dev/null
  # Semantic JSON comparison: parse both and compare with Python == (numeric
  # equality across int/float — the C emitter can't know 1.0 came from a
  # Python float; 0.90000000000000002 and 0.9 are the SAME double).
  if python3 -c '
import json, sys
try:
    c = json.load(open("/tmp/wsct_c.out"))
    p = json.load(open("/tmp/wsct_py.out"))
except Exception:
    sys.exit(1)
sys.exit(0 if c == p else 1)
'; then
    echo "PASS case $i"
  else
    fails=$((fails+1))
    echo "FAIL case $i: fixture=$line"
    echo "  C : $(tr -d ' \n' < /tmp/wsct_c.out)"
    echo "  Py: $(tr -d ' \n' < /tmp/wsct_py.out)"
  fi
done < <(cat $FIX_GLOB)
rm -rf "$SBX"
echo "=== web_server_console_theme oracle: $i cases, $fails failures ==="
[ "$fails" -eq 0 ]
