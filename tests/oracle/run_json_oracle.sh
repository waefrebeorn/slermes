#!/usr/bin/env bash
# Run JSON oracle for gateway/shutdown_forensics json_node_t ports.
set -u
ROOT=/home/wubu/hermes-agent-dev/slermes
cd "$ROOT"
BUILD_DIR=tests/oracle/build
mkdir -p "$BUILD_DIR"
NAME=json_shutdown_forensics
HARNESS=tests/oracle/harness_json_shutdown_forensics.c
TMPH="$BUILD_DIR/home_$NAME"
# Build link recipe (same as run_oracle.sh)
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
bash -c "$COMPILE" >/tmp/json_oracle_compile.log 2>&1 || { echo "COMPILE FAILED"; tail -5 /tmp/json_oracle_compile.log; exit 1; }
bash -c "$LINKCMD -Wl,--allow-multiple-definition" >/tmp/json_oracle_link.log 2>&1 || { echo "LINK FAILED"; grep -iE 'error|undefined' /tmp/json_oracle_link.log | head; exit 1; }

FUNCS=(context_as_json format_context_for_log normalize_reference_images missing_provider_error get_group_info)
# Fixtures per func
declare -A FIX
FIX[context_as_json]='{}|{"signal":"SIGTERM","under_systemd":true,"parent":{"cmdline":"/bin/x","name":"x","pid":7},"loadavg_1m":1.5}|{"signal":"SIGKILL","under_systemd":false,"takeover_marker":1,"takeover_marker_for_self":true,"planned_stop_marker":2,"tracer_pid":99}|{"signal":"?","under_systemd":false}'
FIX[format_context_for_log]='{}|{"signal":"SIGTERM","under_systemd":true,"parent":{"cmdline":"/bin/x","name":"x","pid":7},"loadavg_1m":1.5}|{"signal":"SIGKILL","under_systemd":false,"takeover_marker":1,"takeover_marker_for_self":true,"planned_stop_marker":2,"tracer_pid":99}|{"signal":"?","under_systemd":false}'
FIX[normalize_reference_images]='"hello"|["a","b "," c "]|""|null|["","x"]|42'
FIX[missing_provider_error]='|fal-provider'
FIX[get_group_info]='|grp-123'
FAIL=0
for fn in "${FUNCS[@]}"; do
  IFS='|' read -ra ARR <<< "${FIX[$fn]}"
  for i in "${!ARR[@]}"; do
    IN="${ARR[$i]}"
    C_OUT=$("$BUILD_DIR/tt_$NAME" "$fn" "$IN" 2>/dev/null)
    P_OUT=$(python3 tests/oracle/json_oracle_py.py "$fn" "$IN" 2>/dev/null)
    if [ "$fn" != "format_context_for_log" ]; then
      C_OUT=$(printf '%s' "$C_OUT" | python3 -c "import sys,json; print(json.dumps(json.loads(sys.stdin.read()),sort_keys=True))" 2>/dev/null)
      P_OUT=$(printf '%s' "$P_OUT" | python3 -c "import sys,json; print(json.dumps(json.loads(sys.stdin.read()),sort_keys=True))" 2>/dev/null)
    fi
    if [ "$C_OUT" == "$P_OUT" ]; then
      echo "MATCH  $fn fixture#$i"
    else
      echo "MISMATCH $fn fixture#$i"
      echo "  C: $C_OUT"
      echo "  P: $P_OUT"
      FAIL=1
    fi
  done
done
echo "=== JSON ORACLE: $([ $FAIL -eq 0 ] && echo ALL MATCH || echo HAS MISMATCH) ==="
exit $FAIL
