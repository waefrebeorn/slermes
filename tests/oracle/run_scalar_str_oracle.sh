#!/usr/bin/env bash
set -u
ROOT=/home/wubu/hermes-agent-dev/slermes
cd "$ROOT"
BUILD_DIR=tests/oracle/build
mkdir -p "$BUILD_DIR"
NAME=scalar_str
HARNESS=tests/oracle/harness_scalar_str.c
LINKCMD=$(make -B -n slermes 2>/dev/null | awk '/ -o slermes /{f=1} f{print} /libwhisper\.a/{exit}' | tr -d '\\\n' | sed 's/  */ /g; s/\"//g')
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
bash -c "$COMPILE" >/tmp/ss_compile.log 2>&1 || { echo "COMPILE FAILED"; tail -8 /tmp/ss_compile.log; exit 1; }
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >/tmp/ss_link.log 2>&1 || { echo "LINK FAILED"; grep -iE 'error|undefined' /tmp/ss_link.log | head; exit 1; }

declare -A FIX
FIX[is_openai_fast_model]='gpt-4o-mini|gpt-4o|claude-3|o1-mini|gpt-4.1-nano'
FIX[is_anthropic_fast_model]='claude-3-haiku|claude-3|gpt-4o|claude-sonnet-4'
FIX[model_supports_fast_mode]='gpt-4o-mini|claude-3-haiku|deepseek-chat|gpt-4'
FIX[is_github_models_base_url]='https://models.inference.ai.azure.com|https://api.openai.com|https://models.github.ai|http://example.com'
FIX[is_loopback_hostname]='localhost|127.0.0.1|0.0.0.0|example.com|::1'
FIX[is_control_interrupt_message]='[SYSTEM] interrupt|hello|/stop|interrupt now'
FIX[is_auto_continue_noise]='...|....|hello|... continued'
FIX[is_intentional_silence_response]='<silence/>|hello|<no response>|   '
FIX[is_partial_silence_marker]='[partial]|complete|...partial'
FIX[coerce_ssl_verify]='true|false|1|0|yes'
FIX[coerce_config_version]='1|2|3|0'

FAIL=0
for fn in "${!FIX[@]}"; do
  IFS='|' read -ra ARR <<< "${FIX[$fn]}"
  for entry in "${ARR[@]}"; do
    C_OUT=$("$BUILD_DIR/tt_$NAME" "$fn" "$entry" 2>/dev/null)
    P_OUT=$(python3 tests/oracle/json_oracle_scalar_str_py.py "$fn" "$entry" 2>/dev/null)
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
echo "=== SCALAR-STR ORACLE: $([ $FAIL -eq 0 ] && echo ALL MATCH || echo HAS MISMATCH) ==="
exit $FAIL
