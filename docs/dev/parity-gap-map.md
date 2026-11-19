# Parity gap map (measured, not estimated)

Source: `live_parity_scan.json` produced by `scripts/parity_truth.py`
(scanner: `tests/slermes_parity_battleground.py`). Every number below is
a count of live quarry functions with no verified C port — the scanner
walks the real upstream tree at `C:/Users/eman5/hermes-agent` and matches
PoP annotations against actual definitions. Annotation-only claims do not
count as ports (PoP annotation != parity).

**Totals at last scan (2026-11-19 refresh):** 10,219 ported / 37,307 live
functions (27.4%); **27,043 real gaps**. Module keys are POSIX.

**Devil's-advocate caveat (see `docs/dev/devils-worklist.md`):** the scanner
counts source-level claims. The audit found 653 constant-return/empty stub
bodies under PoP tags, 628 tagged files in no link list (nm-verified absent
from the binary), and 3,199 orphan claims. The binary-honest number is lower;
a DA-5 built-link criterion is queued. Revival agents are re-porting the
worst stub clusters with differential oracles now.

## Largest gap modules (top 30, fresh scan)

| gap fns | module |
|---|---|
| 194 | `hermes_cli/gateway.py` |
| 163 | `gateway/platforms/api_server.py` |
| 131 | `agent/context_compressor.py` |
| 125 | `agent/auxiliary_client.py` |
| 122 | `agent/chat_completion_helpers.py` |
| 110 | `hermes_cli/cli_tui_mixin.py` |
| 109 | `hermes_cli/gateway_agent.py` |
| 104 | `hermes_cli/gateway_windows.py` |
| 90 | `gateway/relay/adapter.py` |
| 84 | `hermes_cli/main.py` |
| 84 | `agent/agent_runtime_helpers.py` |
| 80 | `hermes_cli/update_lock.py` |
| 79 | `tools/browser_launcher.py` |
| 79 | `gateway/run.py` |
| 76 | `gateway/session_policy.py` |
| 74 | `hermes_cli/observability/relay_shared_metrics.py` |
| 72 | `hermes_cli/left_core_migration.py` |
| 72 | `hermes_cli/config_backend.py` |
| 72 | `gateway/run_shutdown.py` |
| 72 | `gateway/platforms/base.py` |
| 71 | `gateway/session_authority.py` |
| 71 | `gateway/run_inbound.py` |
| 71 | `gateway/platforms/api_server_openai_routes.py` |
| 70 | `hermes_cli/model_switch.py` |
| 70 | `hermes_cli/main_desktop.py` |
| 70 | `hermes_cli/gateway_migrate.py` |
| 69 | `hermes_cli/models.py` |
| 69 | `agent/conversation_compression.py` |
| 68 | `tools/mcp_oauth.py` |
| 68 | `gateway/platforms/api_server_runs.py` |

## Verified flow oracles (central orchestrator re-runs, 0 mismatches)

gateway_flow 88, context_flow 104, cch_flow 94, api_server_flow 148,
scheduler_flow 141, conversation_compression_flow 322,
terminal_lifecycle_flow 123, stream_consumer_flow 130,
gw_service_lifecycle 169, config_load_save_flow 138, kanban_db_flow 130,
session_policy_flow 73, run_startup_flow 32, run_shutdown_flow 32,
relay_adapter_flow 115. In triage: tui_dispatch, gateway_windows,
relay_shared_metrics, agent_runtime_helpers, gateway_agent, update_lock.

## Watercup fleet truth (scripts/watercup_truth.py)

Gate classification PORTED = harness exists, NOT cup equivalence. Honest
verdicts from results.jsonl: VERIFIED (ok:true, 0 mismatches), MISMATCH
(real divergence — the port-fix queue), BROKEN (a side errored). The
MISMATCH queue concentrates in cli_py, agent_display_py, learning_graph*,
hosted_room_driver, gateway_run_py — next debugger waves target these.
