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
  [side_question_request]="src/cli/port_side_question_request.o lib/libjson/json.o"
  [gateway_profile_routing]="src/cli/port_gateway_profile_routing.o lib/libjson/json.o"
  [session_activity]="src/cli/port_session_activity.o lib/libjson/json.o"
  [turn_liveness]="src/agent/port_turn_liveness.o lib/libjson/json.o"
  [hermes_auth]="src/cli/port_hermes_auth.o lib/libjson/json.o"
  [auth_error_copy]="src/cli/port_auth_error_copy.o lib/libjson/json.o"
  [partial_compress_keep]="FULL"
  [input_sanitize]="src/cli/port_input_sanitize.o lib/libjson/json.o"
  [agent_reasoning_timeouts]="src/agent/port_agent_reasoning_timeouts.o lib/libjson/json.o"
  [memory_status]="src/cli/port_memory_status.o lib/libjson/json.o"
  [gateway_signal_format]="FULL"
  [learning_graph_render_helpers]="FULL"
  [agent_display_helpers]="src/agent/port_agent_display_helpers.o lib/libjson/json.o"
  [agent_redact_helpers]="FULL"
  [repetition_guard]="src/cli/port_repetition_guard.o lib/libjson/json.o"
  [agent_retry_utils]="src/agent/port_agent_retry_utils.o lib/libjson/json.o"
  [agent_ssl_verify]="src/agent/port_agent_ssl_verify.o lib/libjson/json.o"
  [url_safety_helpers]="src/tools/port_url_safety_helpers.o lib/libjson/json.o"
  [agent_thinking_timeout_guidance]="src/agent/port_agent_thinking_timeout_guidance.o src/agent/port_agent_reasoning_timeouts.o lib/libjson/json.o"
  [gateway_platform_activation]="src/cli/port_gateway_platform_activation.o lib/libjson/json.o"
  [agent_intent_ack]="src/agent/port_agent_intent_ack.o lib/libjson/json.o"
  [gateway_platforms_helpers]="src/gateway/port_gateway_platforms_helpers.o lib/libjson/json.o"
  [turn_summary]="src/cli/port_turn_summary.o lib/libjson/json.o"
  [agent_tool_dispatch_helpers]="src/agent/port_agent_tool_dispatch_helpers.o lib/libjson/json.o"
  [agent_verify_hooks]="src/agent/port_agent_verify_hooks.o lib/libjson/json.o"
  [anthropic_credentials]="src/cli/port_anthropic_credentials.o lib/libjson/json.o"
  [agent_oneshot]="FULL"
  [message_sanitize_close]="FULL"
  [ccd]="src/cli/port_cli_config_load.o src/cli/port_fast_mode.o lib/libjson/json.o"
  [cronjob_prompt_scan]="src/cli/port_cronjob_prompt_scan.o lib/libregex/hermes_regex.o lib/libjson/json.o"
  [image_token_cost_lic]="src/cli/port_image_token_cost.o src/cli/port_anthropic_credentials.o lib/libjson/json.o"
  [relay_media]="src/cli/port_media_labels.o src/cli/port_aux_outcome_event.o lib/libcrypto/crypto.o lib/libjson/json.o"
  [learning_graph]="src/cli/port_learning_graph_helpers.o lib/libjson/json.o"
  [model_cost_guard]="src/cli/port_model_cost_guard.o lib/libjson/json.o"
  [agent_replay_cleanup]="src/agent/port_agent_replay_cleanup.o lib/libjson/json.o"
  [mcp_security]="src/cli/port_mcp_security.o lib/libjson/json.o"
  [agent_turn_context]="FULL"
  [default_soul]="src/cli/port_default_soul.o"
  [inventory]="src/cli/port_inventory.o lib/libjson/json.o"
  [browser_connect]="src/cli/port_browser_connect.o lib/libjson/json.o"
  [banner]="src/cli/port_banner.o lib/libjson/json.o"
  [nous_account]="src/cli/port_nous_account.o lib/libjson/json.o"
  [nous_subscription]="src/cli/port_nous_subscription.o lib/libjson/json.o"
  [secrets_cli]="src/cli/port_secrets_cli.o lib/libjson/json.o"
  [dashboard_register]="src/cli/port_dashboard_register.o lib/libjson/json.o"
  [codex_models]="src/cli/port_codex_models.o lib/libjson/json.o"
  [dingtalk_auth]="src/cli/port_dingtalk_auth.o lib/libjson/json.o"
  [pt_input_extras]="src/cli/port_pt_input_extras.o lib/libjson/json.o"
  [telegram_managed_bot]="src/cli/port_telegram_managed_bot.o lib/libjson/json.o"
  [clipboard]="src/cli/port_clipboard.o lib/libjson/json.o"
  [service_manager]="src/cli/port_service_manager.o lib/libjson/json.o"
  [cli_config_load]="src/cli/port_cli_config_load.o src/cli/port_fast_mode.o lib/libjson/json.o"
  [scheduler_tick]="src/cli/port_scheduler_tick.o lib/libjson/json.o"
  [file_operations_search]="src/tools/port_file_operations_search.o lib/libregex/hermes_regex.o lib/libjson/json.o"
  [hermes_cli_misc]="src/cli/port_hermes_cli_misc.o lib/libjson/json.o"
  [gateway_wake]="src/cli/port_gateway_wake.o lib/libjson/json.o"
  [hosted_room_execution_policy]="src/gateway/port_hosted_room_execution_policy.o lib/libcrypto/crypto.o lib/libjson/json.o"
  [vault_login_classifier]="src/cli/port_vault_login_classifier.o src/cli/vault_login_tokens.o lib/libjson/json.o"
  [api_store_error]="src/cli/port_api_store_error.o"
  [parked_notice]="src/cli/port_parked_notice.o lib/libjson/json.o"
  [producer_lifecycle]="src/cli/port_producer_lifecycle.o lib/libjson/json.o"
  [fast_mode]="src/cli/port_fast_mode.o"
  [bot_loop_guard]="src/cli/port_bot_loop_guard.o lib/libjson/json.o"
  [media_labels]="src/cli/port_media_labels.o src/cli/port_aux_outcome_event.o lib/libcrypto/crypto.o lib/libjson/json.o"
  [error_surface]="src/agent/port_error_surface.o lib/libjson/json.o"
  [config_pure]="lib/libjson/json.o"
  [acp_client]="src/cli/port_acp_client.o lib/libjson/json.o"
  [anthropic_message_convert]="src/cli/port_anthropic_message_convert.o lib/libjson/json.o"
  [status_helpers]="FULL"
  [micro_env]="src/cli/port_micro_env.o lib/libjson/json.o"
  [image_gen_path]="lib/libjson/json.o"
  [xai_expires]="FULL"
  [web_base64_img]="FULL"
  [image_token_cost]="src/cli/port_image_token_cost.o src/cli/port_anthropic_credentials.o lib/libjson/json.o"
  [comp_telemetry]="src/cli/port_comp_telemetry.o lib/libjson/json.o"
  [file_pagination_ops]="src/tools/file_pagination_ops.o lib/libjson/json.o"
  [audit]="src/cli/port_audit.o lib/libjson/json.o"
  [access_policy]="src/cli/port_access_policy.o lib/libjson/json.o"
  [aux_task_builder]="src/cli/port_aux_task_builder.o lib/libjson/json.o"
  [error_classifier_upstream]="FULL"
  [auth_providers]="src/cli/port_auth_providers.o lib/libjson/json.o"
  [blueprint_plugins]="src/cli/port_blueprint_plugins.o lib/libjson/json.o"
  [callbacks]="src/cli/port_callbacks.o lib/libjson/json.o"
  [aux_outcome_event]="src/cli/port_aux_outcome_event.o lib/libjson/json.o"
  [cli_process_dock]="src/cli/port_cli_process_dock.o lib/libjson/json.o"
  [display_shell]="FULL"
  [provider_auth]="lib/libproviderauth/provider_auth.o"
  [browser_redact]="src/tools/browser_redact.o lib/libregex/hermes_regex.o lib/libjson/json.o"
)

fail=0
count=0

# --changed / ORACLE_ONLY=changed must be read BEFORE names=("$@"), otherwise
# the flag itself lands in the port list and is treated as an oracle name.
WANT_CHANGED=0
for a in "$@"; do
  [ "$a" = "--changed" ] && WANT_CHANGED=1
done
names=()
for a in "$@"; do
  [ "$a" = "--changed" ] && continue
  names+=("$a")
done

# If no subset given, discover all committed oracles.
# fuzzy_match_helpers is EXCLUDED: its harness consumes its OWN stdin (op-value
# lines), so the no-stdin pipe here can't drive it — the standalone
# tests/gate_fuzzy_match_helpers.sh is the authority for that port.
SUITE_SKIP="fuzzy_match_helpers file_type github_provider"
if [ ${#names[@]} -eq 0 ]; then
  for o in tests/oracle_*.py; do
    n="$(basename "$o" | sed -E 's/^oracle_//; s/\.py$//')"
    case " $SUITE_SKIP " in *" $n "*) continue;; esac
    names+=("$n")
  done
fi

# --changed / ORACLE_ONLY=changed: gate only the ports the current work touches.
#
# The full 95-oracle gate costs ~450s and is I/O/spawn-bound (measured: 4-way
# 466s, 8-way 444s, sequential 43s for 7 oracles), so parallelism does not help.
# A commit usually touches one or two ports, so gating just those is the
# difference between a 10s check and an 8-minute one.
#
# Maps changed paths back to oracle names: src/**/port_<name>.c and
# include/port_<name>.h both mean oracle <name>. Falls back to the full set
# when nothing maps (a change to a shared header, a lib, or the build files can
# affect any port) - say so rather than silently passing.
if [ "${1:-}" = "--changed" ] || [ "${ORACLE_ONLY:-}" = "changed" ]; then
  shift 2>/dev/null || true
  # git is NOT on the login shell's PATH (it is /mingw64/bin/git here), so
  # resolve it explicitly and fall back to the full gate rather than silently
  # reporting "nothing changed".
  GIT="$(command -v git 2>/dev/null || true)"
  if [ -z "$GIT" ]; then
    for c in /mingw64/bin/git /mingw64/bin/git.exe /usr/bin/git /usr/bin/git.exe \
             "/c/Program Files/Git/cmd/git.exe" "/c/Program Files/Git/bin/git.exe"; do
      [ -x "$c" ] && { GIT="$c"; break; }
    done
  fi
  if [ -n "$GIT" ]; then
    changed=$(
      { "$GIT" diff --name-only HEAD 2>/dev/null
        "$GIT" diff --name-only --staged 2>/dev/null
      } | sort -u
    )
  else
    echo "could not locate git; running the FULL gate" >&2
    changed=""
  fi
  mapped=()
  while IFS= read -r f; do
    case "$f" in
      */port_*.c|port_*.c)
        n="$(basename "$f" | sed -E 's/^port_//; s/\.c$//')"
        [ -f "tests/oracle_$n.py" ] && mapped+=("$n")
        ;;
      include/port_*.h)
        n="$(basename "$f" | sed -E 's/^port_//; s/\.h$//')"
        [ -f "tests/oracle_$n.py" ] && mapped+=("$n")
        ;;
    esac
  done <<< "$changed"
  if [ ${#mapped[@]} -gt 0 ]; then
    echo "changed ports: ${mapped[*]}"
    names=("${mapped[@]}")
  else
    echo "no port files changed; running the FULL gate" >&2
  fi
fi

# Resolve each name to "<name>\t<harness>\t<deps>" in the PARENT shell (bash
# cannot export the HARNESS_ALIAS / DEPS associative arrays to the subshells
# xargs spawns), then fan the lines out to tests/_oracle_one.sh workers.
# The borrowed third-party lib dir is needed to LINK each harness, and it must
# travel as an ARGUMENT rather than an environment variable: the ccache wrapper
# strips LIBRARY_PATH from the compiler it execs, and this box's bash drops
# exported vars in nested children. build/winlibs-env.sh sets it when the
# caller sourced it; otherwise fall back to the repo-local default.
if [ -z "${ORACLE_LIBDIR:-}" ]; then
  if [ -f build/winlibs-env.sh ]; then
    # shellcheck disable=SC1091
    . ./build/winlibs-env.sh >/dev/null 2>&1 || true
  fi
fi
if [ -z "${ORACLE_LIBDIR:-}" ]; then
  for c in /c/winlibs/extra-libs "C:/winlibs/extra-libs"; do
    [ -d "$c" ] && { ORACLE_LIBDIR="$c"; break; }
  done
fi

resolve_one() {
  local name="$1"
  local harness="${HARNESS_ALIAS[$name]:-$name}"
  local deps="src/cli/port_$name.o lib/libjson/json.o"
  if grep -q 'hermes_regex.h' "tests/t_port_${harness}.c" 2>/dev/null \
     || grep -q 'hermes_regex.h' "src/cli/port_$name.c" 2>/dev/null \
     || grep -q 'hermes_regex.h' "include/port_$name.h" 2>/dev/null; then
    deps="$deps lib/libregex/hermes_regex.o"
  fi
  local varname="ORACLE_OBJS_$(echo "$name" | tr '[:lower:]' '[:upper:]' | tr '/' '_')"
  if [ -n "${!varname:-}" ]; then deps="${!varname}"; fi
  if [ -n "${DEPS[$name]:-}" ]; then deps="${DEPS[$name]}"; fi
  printf '%s\t%s\t%s\t%s\n' "$name" "$harness" "$deps" "${ORACLE_LIBDIR:-}"
}

# Parallelism: ORACLE_JOBS (default 4). ORACLE_JOBS=1 gives the sequential
# behaviour. 12-way died with MSYS2 fork exhaustion ("Resource temporarily
# unavailable" / 0xC0000142) - but that turned out to be CAUSED by the shared
# .testbin/tt_$NAME / oracle_tmp_$NAME paths, not by the parallelism itself.
# With those made per-invocation ($$), 4-way is clean. Workers touch only their
# own compile, scratch dir and log, so ccache stays safe.
JOBS="${ORACLE_JOBS:-4}"

if [ "$JOBS" -le 1 ]; then
  for name in "${names[@]}"; do
    bash tests/_oracle_one.sh "$(resolve_one "$name")" || fail=1
    count=$((count+1))
  done
else
  results=$(
    for name in "${names[@]}"; do resolve_one "$name"; done |
      xargs -P "$JOBS" -d '\n' -I{} bash tests/_oracle_one.sh {}
  )
  echo "$results"
  pass_n=$(printf '%s\n' "$results" | grep -c '^PASS ')
  miss_n=$(printf '%s\n' "$results" | grep -c '^MISS ')
  fail_n=$(printf '%s\n' "$results" | grep -c '^FAIL ')
  count=$((pass_n + fail_n + miss_n))
  if [ "$fail_n" -gt 0 ] || [ "$miss_n" -gt 0 ]; then fail=1; fi
fi

echo "----"
echo "oracles run: $count  (exit 0 = all green, no drift)"
exit "$fail"
