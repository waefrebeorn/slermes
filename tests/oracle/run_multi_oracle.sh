#!/usr/bin/env bash
# Multi-arg deterministic oracle for selected port_*.c functions.
set -u
ROOT=/home/wubu/hermes-agent-dev/slermes
cd "$ROOT"
BUILD_DIR=tests/oracle/build
mkdir -p "$BUILD_DIR"
NAME=multi
HARNESS=tests/oracle/harness_multi.c
LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\\"//g')
LINKCMD=${LINKCMD// -o slermes / -o "$BUILD_DIR/tt_$NAME" }
LINKCMD=${LINKCMD//src\/main.o /$HARNESS }
LINKCMD=$(echo "$LINKCMD" | sed -E 's#-DHERMES_VERSION=[^ ]*#-DHERMES_VERSION=0#g; s#-DHERMES_RELEASE_DATE=[^ ]*#-DHERMES_RELEASE_DATE=0#g; s#-DATADIR=[^ ]*#-DATADIR="/share/slermes/docs"#g')
SRCINCS=$(find src -type d 2>/dev/null | sed 's#^#-I #' | tr '\n' ' ')
LIBINCS="-I lib $(grep -oE 'lib/lib[a-z0-9_]+' build/libs-config.mk 2>/dev/null | sed 's#^#-I #' | tr '\n' ' ')"
INC="-I include -I lib $LIBINCS $SRCINCS"
LINKCMD="$LINKCMD $INC -D_XOPEN_SOURCE=700 -D_GNU_SOURCE"
mkdir -p "$BUILD_DIR/obj/tests"
COMPILE="gcc -c $HARNESS -o $BUILD_DIR/obj/tests/t_$NAME.o $INC -D_XOPEN_SOURCE=700 -D_GNU_SOURCE"
COMPILE=$(echo "$COMPILE" | sed -E 's#-DHERMES_VERSION=[^ ]*#-DHERMES_VERSION=0#g; s#-DHERMES_RELEASE_DATE=[^ ]*#-DHERMES_RELEASE_DATE=0#g; s#-DATADIR=[^ ]*#-DATADIR="/share/slermes/docs"#g')
rm -f "$BUILD_DIR/tt_$NAME"
bash -c "$COMPILE" >/tmp/multi_compile.log 2>&1 || { echo "COMPILE FAILED"; tail -8 /tmp/multi_compile.log; exit 1; }
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >/tmp/multi_link.log 2>&1 || { echo "LINK FAILED"; grep -iE 'error|undefined' /tmp/multi_link.log | head; exit 1; }

# function | fixtures (comma-separated args, '|'-separated fixtures)
declare -A FIX
FIX[multiset_char_hit_ratio]='abc,abracadabra|hello,world|aa,aabbcc'
FIX[bigram_jaccard]='the cat,the cat sat|abc,abc|hello,world'
FIX[longest_subsequence_ratio]='abc,axbycz|hello,hxexlxlxox|aa,aabb'
FIX[score_field]='hello world,hello|foo bar baz,bar|abc,xyz'
FIX[env_line_defines_key]='FOO=1,FOO|BAR=2,FOO|PATH=/x,PATH'
FIX[web_windows_build_number]='1.2.3,windows|0.19.0,windows|1.0,linux'
FIX[allows_private_ip_resolution]='localhost,http|example.com,https|10.0.0.1,http'
FIX[match_host_against_rule]='api.example.com,*.example.com|evil.com,*.example.com|foo.com,foo.com'
FIX[compact_text]='  hello   world  |a b c|  spaced   out  '

FAIL=0
for fn in "${!FIX[@]}"; do
  IFS='|' read -ra ARR <<< "${FIX[$fn]}"
  for entry in "${ARR[@]}"; do
    IFS=',' read -ra A <<< "$entry"
    C_OUT=$("$BUILD_DIR/tt_$NAME" "$fn" "${A[@]}" 2>/dev/null)
    P_OUT=$(python3 tests/oracle/json_oracle_multi_py.py "$fn" "${A[@]}" 2>/dev/null)
    if [ "$C_OUT" == "$P_OUT" ]; then
      echo "MATCH  $fn [${A[*]}]"
    else
      echo "MISMATCH $fn [${A[*]}]"
      echo "  C: $C_OUT"
      echo "  P: $P_OUT"
      FAIL=1
    fi
  done
done
echo "=== MULTI ORACLE: $([ $FAIL -eq 0 ] && echo ALL MATCH || echo HAS MISMATCH) ==="
exit $FAIL
