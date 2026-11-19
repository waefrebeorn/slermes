# Parity gap map (measured, not estimated)

Source: `live_parity_scan.json` produced by `scripts/parity_truth.py`
(scanner: `tests/slermes_parity_battleground.py`). Every number below is
a count of live quarry functions with no verified C port — the scanner
walks the real upstream tree at `C:/Users/eman5/hermes-agent` and matches
PoP annotations against actual definitions. Annotation-only claims do not
count as ports (PoP annotation != parity).

**Totals at last scan:** 8,418 ported / 37,307 live functions (22.6%); **28,855 real gaps**.

Package split of the gap: hermes_cli 11,670, gateway 5,897, agent 5,843, tools 4,856, cron 589.

## Largest gap modules (top 30)

| gap fns | module | coverage status |
|---|---|---|
| 243 | `gateway/platforms/base.py` | wave-3 agent (adapter lifecycle flow) |
| 218 | `hermes_cli/gateway.py` | unclaimed — next wave candidate |
| 208 | `agent/auxiliary_client.py` | wave-3 agent (retry/fallback/budget flow) |
| 198 | `gateway/platforms/api_server.py` | wave-3 agent (request pipeline flow) |
| 148 | `agent/chat_completion_helpers.py` | wave-1 flow ported (cch_flow_*), oracle debugging |
| 144 | `agent/context_compressor.py` | wave-1 flow ported (cc_flow_*), oracle debugging |
| 143 | `agent/conversation_compression.py` | wave-3 agent (compression pipeline flow) |
| 112 | `hermes_cli/observability/relay_shared_metrics.py` | unclaimed — next wave candidate |
| 110 | `hermes_cli/cli_tui_mixin.py` | unclaimed — next wave candidate |
| 109 | `hermes_cli/gateway_agent.py` | unclaimed — next wave candidate |
| 106 | `gateway/run.py` | wave-1 flow ported (grun_flow_*), oracle debugging |
| 105 | `hermes_cli/gateway_windows.py` | unclaimed — next wave candidate |
| 95 | `cron/scheduler.py` | wave-3 agent (tick flow) |
| 95 | `agent/credential_pool.py` | wave-3 agent (rotation flow) |
| 93 | `gateway/relay/adapter.py` | unclaimed — next wave candidate |
| 84 | `hermes_cli/main.py` | unclaimed — next wave candidate |
| 84 | `gateway/platforms/helpers.py` | unclaimed — next wave candidate |
| 84 | `agent/agent_runtime_helpers.py` | unclaimed — next wave candidate |
| 80 | `hermes_cli/update_lock.py` | unclaimed — next wave candidate |
| 79 | `tools/browser_launcher.py` | unclaimed — next wave candidate |
| 77 | `hermes_cli/kanban_db.py` | unclaimed — next wave candidate |
| 76 | `gateway/session_policy.py` | unclaimed — next wave candidate |
| 75 | `hermes_cli/observability/shared_metrics_contract.py` | unclaimed — next wave candidate |
| 75 | `gateway/run_shutdown.py` | unclaimed — next wave candidate |
| 74 | `hermes_cli/models.py` | unclaimed — next wave candidate |
| 72 | `hermes_cli/left_core_migration.py` | unclaimed — next wave candidate |
| 72 | `hermes_cli/config_backend.py` | unclaimed — next wave candidate |
| 72 | `gateway/run_startup.py` | unclaimed — next wave candidate |
| 72 | `gateway/platforms/api_server_openai_routes.py` | unclaimed — next wave candidate |
| 71 | `hermes_cli/model_switch.py` | unclaimed — next wave candidate |

## How gaps get closed (doctrine)

1. **Flow first.** Orchestration functions are ported as decision sequences:
   call order, branch conditions, state transitions, error paths. All I/O
   (network, disk, clock, subprocess) is injected through documented seams.
2. **Sequence oracles.** `tests/oracle_<name>.py` AST-extracts the live defs,
   drives them under the SAME scripted seam results as the C harness, and
   compares ordered traces + returns + mutated state. Fail-closed on 0 cases.
3. **Central verification.** Every claimed green is re-run by the orchestrator
   via `tests/run_one_oracle.sh` (SAC-safe FULL link) before commit.
4. **Honest residuals.** Functions deleted upstream stay orphaned; low-consensus
   attribution repairs are dropped, not guessed (`scripts/repair_orphan_claims.py`).

## Scanner hygiene

- Module keys are POSIX (`as_posix()` in all five census loops). Before this
  fix, `str(Path)` on Windows produced mixed-separator keys that split one
  module's stats across two entries (e.g. `gateway/platforms\base.py`).
- `build/objects.mk` is deduped; same-basename objects are nm-checked for
  symbol overlap before linking (complementary split ports link both).
- Re-measure after every wave: `python scripts/parity_truth.py`.
