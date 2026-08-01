#!/usr/bin/env bash
set -u
ROOT=/home/wubu/hermes-agent-dev/slermes
cd "$ROOT"
BUILD_DIR=tests/oracle/build
mkdir -p "$BUILD_DIR"
NAME=json_in
HARNESS=tests/oracle/harness_json_in.c
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
bash -c "$COMPILE" >/tmp/json_in_compile.log 2>&1 || { echo "COMPILE FAILED"; tail -8 /tmp/json_in_compile.log; exit 1; }
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >/tmp/json_in_link.log 2>&1 || { echo "LINK FAILED"; grep -iE 'error|undefined' /tmp/json_in_link.log | head; exit 1; }

# func | fixtures (JSON strings, space-separated within; '|'-separated fixtures)
declare -A FIX
FIX[is_command_provider_config]='{"provider":"command"}|{"provider":"openai"}|{"type":"cmd"}|{}'
FIX[get_command_tts_timeout]='{"provider":"command","timeout":30}|{"timeout":45}|{}|{"provider":"command"}'
FIX[openrouter_model_is_free]='{"prompt":0,"completion":0}|{"prompt":1,"completion":0}|{"prompt":0,"completion":2}|{"foo":1}'
FIX[is_signal_rate_limit_error]='{"code":429,"message":"rate limited"}|"[429] too many requests"|{"code":500}|"RateLimitException boo"|{"message":"Retry after 30 seconds"}'
FIX[scale_to_zero_enabled]='{"SLERM_SCALE_TO_ZERO":"1"}|{}|{"SLERM_SCALE_TO_ZERO":"0"}|{"PATH":"/x"}'
FIX[messaging_is_relay_only_or_absent]='[]|["relay"]|["discord","telegram"]|["relay","slack"]'

FAIL=0
for fn in "${!FIX[@]}"; do
  IFS='|' read -ra ARR <<< "${FIX[$fn]}"
  for entry in "${ARR[@]}"; do
    C_OUT=$("$BUILD_DIR/tt_$NAME" "$fn" "$entry" 2>/dev/null)
    P_OUT=$(python3 tests/oracle/json_oracle_json_in_py.py "$fn" "$entry" 2>/dev/null)
    if [ "$C_OUT" == "$P_OUT" ]; then
      echo "MATCH  $fn [$entry]"
    else
      echo "MISMATCH $fn [$entry]"
      echo "  C: $C_OUT"
      echo "  P: $P_OUT"
      FAIL=1
    fi
  done
done
echo "=== JSON-IN ORACLE: $([ $FAIL -eq 0 ] && echo ALL MATCH || echo HAS MISMATCH) ==="
exit $FAIL
