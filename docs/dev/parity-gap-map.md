# Parity gap map (measured, not estimated)

Source: `live_parity_scan.json` produced by `scripts/parity_truth.py`
(scanner: `tests/slermes_parity_battleground.py`). Every number below is
a count of live quarry functions with no verified C port — the scanner
walks the real upstream tree at `C:/Users/eman5/hermes-agent` and matches
PoP annotations against actual definitions. Annotation-only claims do not
count as ports (PoP annotation != parity).

**Totals at honest-universe scan (2026-11-19, DA-6d):** 15,496 ported /
49,699 live functions (**31.2%**); **34,154 real gaps**; **2,987 modules**.

## DA-6: the universe was selection-biased (recursive audit finding)

The prior 27.4% figure (10,219 / 37,307) was computed on a *biased*
denominator. `scan_all` hardcoded per-package walks for only the home
junction set (agent/tools/gateway/cron/cli/hermes_cli); every package
outside that set was invisible **unless already PoP-claimed**. The fix
(quarry-sibling walks for plugins/tui_gateway/providers/session_rules/pm/
hermes_agent/apps/persistence/acp_adapter/hermes_platform) revealed
**+12,392 live functions** and **+7,111 gaps** the old scan silently omitted:
`plugins/` 4,398 gaps (platform adapters dominate), `tui_gateway/` 1,810,
`pm/` 723, `session_rules/` 223, `providers/` 100. Coverage rose to 31.2%
only because today's waves added real ports; the denominator grew faster.
The honest gap is larger than previously reported — the point of the audit.

**Devil's-advocate caveat (see `docs/dev/devils-worklist.md`):** the scanner
counts source-level claims. The audit found 653 constant-return/empty stub
bodies under PoP tags, 628 tagged files in no link list (nm-verified absent
from the binary), and 3,199 orphan claims. The binary-honest number is lower;
revival agents are re-porting the worst stub clusters with differential
oracles, and `scripts/watercup_truth.py` separates VERIFIED/MISMATCH/BROKEN
from the false-green PORTED classifier.

## Largest gap modules (top 25, honest universe)

| gap fns | ported/live | module |
|---|---|---|
| 304 | 39/343 | `plugins/platforms/telegram/adapter.py` |
| 270 | 29/299 | `plugins/platforms/slack/adapter.py` |
| 266 | 25/291 | `plugins/platforms/discord/adapter.py` |
| 230 | 25/255 | `plugins/platforms/feishu/adapter.py` |
| 190 | 57/247 | `hermes_cli/gateway.py` |
| 145 | 20/165 | `plugins/platforms/matrix/adapter.py` |
| 139 | 66/211 | `gateway/platforms/api_server.py` |
| 122 | 61/183 | `agent/chat_completion_helpers.py` |
| 110 | 144/254 | `agent/context_compressor.py` |
| 108 | 1/109 | `plugins/kanban/dashboard/plugin_api.py` |
| 100 | 6/106 | `tui_gateway/server.py` |
| 96 | 22/118 | `plugins/platforms/buzz/adapter.py` |
| 96 | 18/114 | `hermes_cli/gateway_agent.py` |
| 93 | 6/99 | `persistence/sqlite/conversation/sessions.py` |
| 93 | 283/376 | `agent/auxiliary_client.py` |
| 92 | 14/106 | `persistence/conversation/sessions.py` |
| 92 | 22/114 | `hermes_cli/cli_tui_mixin.py` |
| 91 | 24/115 | `plugins/platforms/photon/adapter.py` |
| 87 | 1/88 | `pm/packages.py` |
| 84 | 5/89 | `persistence/conversation/messages.py` |
| 81 | 12/93 | `plugins/platforms/whatsapp/adapter.py` |
| 76 | 37/113 | `tui_gateway/methods_tools.py` |
| 76 | 1/77 | `gateway/session_policy.py` |
| 74 | 6/80 | `tui_gateway/hosted_room_driver.py` |
| 74 | 23/97 | `plugins/platforms/google_chat/adapter.py` |

## Verified flow oracles (central orchestrator re-runs, 0 mismatches)

gateway_flow 88, context_flow 104, cch_flow 94, terminal_lifecycle_flow 123,
stream_consumer_flow 130, config_load_save_flow 138, credential_pool_flow 145,
base_flow 415, gateway_status_flow 176, models_catalog_flow 163,
process_registry_flow 118, tui_dispatch_flow 132, gateway_windows_flow 53,
browser_launcher_flow 134, setup_revive 118, openai_routes_flow 189,
tail_revive3 361, ms_providers_flow 154, redact_flow 134,
platforms_helpers_flow 180, bedrock_adapter_flow 173, run_busy_flow 147,
model_switch_flow 128, hosted_rooms_flow 152, run_adapters_flow 138.
In verification battery: ws_revive1-4, rest_revive1/3, sessions_cmd,
turn_progress, run_notifications, docker_env, runtime_provider, kanban_tools,
gw_session_store, session_controls, prompt_cache_clock.

## Recursive audit tooling (level 2)

- `scripts/fuzz_flow_oracle.py` — mutation-based differential fuzzing beyond
  hand-written fixtures (WFDB-x/DIFUZZI grounding). First run on redact_flow
  found 14 divergences (C swallows None where live raises AttributeError).
- `scripts/raise_parity_accounting.py` — measures how many green-oracle passes
  are both-sides-raise agreements (DIFUZZI: "both failing" counts as agreement);
  flags error-path-heavy oracles with untested happy paths.
- `docs/dev/debug-queue.md` — measured states + triage recipes for open targets.

## Watercup fleet truth (scripts/watercup_truth.py)

Gate classification PORTED = harness exists, NOT cup equivalence. Honest
verdicts from results.jsonl: VERIFIED (ok:true, 0 mismatches), MISMATCH
(real divergence — the port-fix queue), BROKEN (a side errored). The
MISMATCH queue concentrates in cli_py, agent_display_py, learning_graph*,
hosted_room_driver, gateway_run_py — next debugger waves target these.
