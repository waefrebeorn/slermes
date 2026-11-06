#!/usr/bin/env bash
# run_all_oracles.sh - verify EVERY committed oracle in one command.
#
# Differential-testing regression gate (the "land it green" step for a port):
# compiles+links each tests/t_port_<name>.c, pipes it into tests/oracle_<name>.py,
# and requires ``ORACLE <name>: N cases, 0 mismatches`` + exit 0 for every pair.
# Any drift (a regression, a Smart-App-Control refusal, a missing object) fails
# the run and names the port.
#
# Dependency default: src/cli/port_<name>.o + lib/libjson/json.o + (regex if the
# harness includes hermes_regex.h). Override per port with the ORACLE_OBJS_<NAME>
# env var or an entry below under OVERRIDES (function is a module-not-name-safe
# hook for the handful of oracles whose harness needs extra/custom objects).
#
# Usage:
#   bash tests/run_all_oracles.sh                 # run all
#   bash tests/run_all_oracles.sh name1 name2     # run a subset
#   ORACLE_OBJS_IDLE_WATCH="src/cli/port_idle_watch.o lib/libregex/hermes_regex.o lib/libjson/json.o" \
#     bash tests/run_all_oracles.sh idle_watch

set -u
cd "$(dirname "$0")/.." || exit 1
mkdir -p .testbin/ao

# legacy: discovery name -> harness basename (when the oracle filename and the
# t_port_<name>.c differ for a pre-existing port).
declare -A HARNESS_ALIAS=(
  [audit_bounded]="audit"
  [callbacks_skip]="callbacks"
)

# special deps: name -> FULL MINIMAL_OBJS (ports whose own code pulls extra lib
# objects the default auto-set misses). Override any on the CLI with
# ORACLE_OBJS_<NAME>.
declare -A DEPS=(
  # aliased harness names: the object follows the HARNESS name, not the discovery
  # oracle name.
  [audit_bounded]="src/cli/port_audit.o lib/libjson/json.o"
  [callbacks_skip]="src/cli/port_callbacks.o lib/libjson/json.o"
  # backfilled: harness/object name differs from the oracle discovery name.
  [whatsapp_identity]="src/cli/port_gateway_whatsapp_identity.o lib/libjson/json.o"
  [pty_clamp]="src/cli/port_pty_clamp_helpers.o lib/libjson/json.o"
  [session_server_requests]="src/cli/port_session_server_requests.o lib/libjson/json.o"
  [ccd]="src/cli/port_cli_config_load.o src/cli/port_fast_mode.o lib/libjson/json.o"
  [cronjob_prompt_scan]="src/cli/port_cronjob_prompt_scan.o lib/libregex/hermes_regex.o lib/libjson/json.o"
  [image_token_cost_lic]="src/cli/port_image_token_cost.o src/cli/port_anthropic_credentials.o lib/libjson/json.o"
  [relay_media]="src/cli/port_media_labels.o src/cli/port_aux_outcome_event.o lib/libcrypto/crypto.o lib/libjson/json.o"
)

fail=0
count=0
names=("$@")

# If no subset given, discover all committed oracles.
if [ ${#names[@]} -eq 0 ]; then
  for o in tests/oracle_*.py; do
    names+=("$(basename "$o" | sed -E 's/^oracle_//; s/\.py$//')")
  done
fi

for name in "${names[@]}"; do
  oracle="tests/oracle_$name.py"
  harness="${HARNESS_ALIAS[$name]:-$name}"
  harness_c="tests/t_port_${harness}.c"
  [ -f "$oracle" ] || { echo "MISS oracle_$name.py"; fail=1; continue; }
  [ -f "$harness_c" ] || { echo "MISS t_port_${harness}.c (harness for $name)"; fail=1; continue; }

  # default dependency set; honor a per-port env override then a DEPS entry
  deps="src/cli/port_$name.o lib/libjson/json.o"
  # the regex object is needed when the PORT .c (not just the harness, which
  # includes only the port header) pulls hermes_regex.h — grep both.
  if grep -q 'hermes_regex.h' "$harness_c" \
     || grep -q 'hermes_regex.h' "src/cli/port_$name.c" 2>/dev/null \
     || grep -q 'hermes_regex.h' "include/port_$name.h" 2>/dev/null; then
    deps="$deps lib/libregex/hermes_regex.o"
  fi
  varname="ORACLE_OBJS_$(echo "$name" | tr '[:lower:]' '[:upper:]' | tr '/' '_')"
  if [ -n "${!varname:-}" ]; then deps="${!varname}"; fi
  if [ -n "${DEPS[$name]:-}" ]; then deps="${DEPS[$name]}"; fi
  export MINIMAL_OBJS="$deps"

  log=".testbin/ao/$name.log"
  # run_one_oracle compiles t_port_<HARNESS>.c from its first arg; pass the
  # harness basename, the oracle basename second.
  out=$(bash tests/run_one_oracle.sh "$harness" "$(basename "$oracle")" 2>&1)
  rc=$?
  # accept ORACLE <any>: N cases, 0 mismatches — a port's oracle may announce a
  # slightly different identity than the discovery name (e.g. kanban_schema).
  ok=$(printf '%s\n' "$out" | grep -cE "ORACLE [A-Za-z0-9_]+: [0-9]+ cases, 0 mismatches")
  if [ $rc -eq 0 ] && [ "$ok" -ge 1 ]; then
    echo "PASS $name  ($(printf '%s' "$out" | grep -oE '[0-9]+ cases, 0 mismatches' | head -1))"
  else
    echo "FAIL $name  rc=$rc  $(printf '%s' "$out" | grep -oE 'ORACLE .*' | head -1)"
    fail=1
  fi
  count=$((count+1))
done

echo "----"
echo "oracles run: $count  (exit 0 = all green, no drift)"
exit "$fail"
