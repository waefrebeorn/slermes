# Devil's-advocate worklist — waves 10–13 flow ports (recursive PoP-tag + stub audit)

**Snapshot:** 2026-11-19 13:03:33 EST · **Scope:** `src/**/*flow*.c`, `*flow2*.c`, `*revive*.c` (122 files, 2999 PoP claims) · **Quarry:** `C:/Users/eman5/hermes-agent` (read-only)

> **Moving-tree caveat:** ports were being landed by concurrent workers DURING this audit (86 target files modified <6h; claim count grew 2828→2999 across passes; one first-wave harness `t_port_cch_flow.c` appeared mid-audit and cleared 24 dead-credit findings; 21 corrupted tag fragments in `auth_flow.c` seen at 12:57 were repaired by the worker by 12:59). This document is a timestamped snapshot — re-run `scratch/wave13_audit.py` before acting on any line item.

## Headline

**2683/2999 claims (89.5%) are FAITHFUL**: real C bodies whose claimed Python function exists in the quarry AST and does matching work. Unlike the gaps-era stub clusters, waves 10–13 are **behaviorally honest — only 1 guilty stub**. The false-green risk in these waves is **metadata and plumbing**: wrong tag names, duplicate claims against superseded ports, orphan claims, and harness dispatch omissions.

## Verdict counts

| verdict | claims | meaning |
|---|---:|---|
| FAITHFUL | 2683 | real body; live quarry fn exists; behavior class matches (stub-vs-trivial-live allowed) |
| INFRA | 12 | `<seam dispatcher>`/`<truthiness>` plumbing tags — not live-fn claims |
| DUP | 86 | same live fn also claimed by another port (whole-tree claim map) |
| MISPLACED | 141 | tag not immediately before ITS definition: wrong/undefined C name, stacked tags |
| AMBIG | 9 | bare py name matches ≥2 class methods; scanner cannot tell which is credited |
| DEAD | 48 | claimed fn (and its adjacent def) absent from every driver harness dispatch |
| ORPHAN | 19 | claimed module/fn does not exist in quarry AST |
| GUILTY | 1 | stub-shaped C body while live quarry fn does real work |

Verdict = worst finding per claim; a claim can carry several findings (e.g. DUP + UNBUILT-INCL).

## Pattern 1 — GUILTY stub bodies (1)

| file:line | claim | C shape | live behavior |
|---|---|---|---|
| `src/gateway/platforms/platforms_base_flow2.c:1792` | `gateway/platforms/base.py:supports_exec_approval_buttons` | echo | real: 1 stmts, 0 calls |

`pb2_supports_exec_approval_buttons` returns `json_bool(override)` — the fixture supplies the answer. Live `BasePlatformAdapter.supports_exec_approval_buttons` computes `cls._send_exec_approval_prompt is not BasePlatformAdapter._send_exec_approval_prompt`. The port carries a comment documenting the modeling choice, and the oracle compares against the live computation — but the C function itself contributes zero logic (echo-param, not even a seam event). **Borderline GUILTY**: convert the hook-identity check to a named seam event, or strip the tag and let the oracle own it. Low priority (documented, differentially checked).

All other constant/empty-alloc returns in the waves were **proven innocent against the quarry AST**: e.g. `config_backend_flow.c` cb_file_* ports return NULL/true/empty arrays because the concrete `FileBackend` methods return `None`/`True`/`frozenset()` (the protocol `...` stubs and module-level self-delegators do not outrank the concrete impl); `bedrock_adapter_flow.c`, `photon_adapter_flow.c`, `hosted_room_driver_flow.c`, `web_git_flow.c`, `turn_progress_flow.c` single-return bodies are wrappers over real local helpers or seam models of single-call live fns.

## Pattern 2 — DUP claims (86)

Same live function claimed by a wave port AND another port. Counterpart histogram:

| counterpart file | duplicate claims |
|---|---:|
| `src/agent/credential_pool_ports.c` | 16 |
| `src/agent/context_compressor_pure.c` | 10 |
| `src/gateway/platforms/platforms_helpers_flow.c` | 10 |
| `src/agent/auxiliary_client_gaps.c` | 8 |
| `src/agent/context_compressor_wrappers.c` | 4 |
| `src/agent/conversation_compression_gaps.c` | 4 |
| `src/agent/context_compressor_gaps.c` | 3 |
| `src/agent/conversation_compression_gaps2.c` | 3 |
| `src/agent/message_content_ports.c` | 2 |
| `src/agent/context_compressor_ports.c` | 2 |
| `src/cli/cli_py_flow.c` | 2 |
| `src/cli/config_backend_flow.c` | 2 |
| `src/cli/setup_flow2.c` | 2 |
| `src/cron/scheduler_gaps.c` | 2 |
| `src/gateway/platforms/api_server_flow.c` | 2 |
| `src/gateway/platforms/base_gaps.c` | 2 |
| `src/hermes_cli/pm_packages_flow.c` | 2 |
| `src/hermes_cli/web_models.c` | 2 |
| `src/tools/terminal_tool.c` | 2 |
| `src/agent/reasoning_summaries.c` | 1 |
| `src/agent/chat_completion_helpers_ports.c` | 1 |
| `src/agent/chat_completion_helpers.c` | 1 |
| `src/hermes_cli/gateway_windows_gaps.c` | 1 |
| `src/hermes_cli/commands.c` | 1 |
| `src/gateway/run_gaps.c` | 1 |
| `src/hermes_cli/config_gaps.c` | 1 |
| `src/hermes_cli/config.c` | 1 |
| `src/hermes_cli/web_server_cron_dash.c` | 1 |
| `src/tools/file.c` | 1 |

**Verified example:** `auxiliary_client_gaps.c:125` still carries `/* PoP: message_field @ agent/auxiliary_client.py:_message_field */` over a genuine stub (`(void)message; (void)field; return strdup("");`) while `auxiliary_client_flow.c:244` ports the real behavior. The gaps-era tag was never stripped when the flow port landed — the scanner double-credits, and the stub stays in the link. Same shape for `credential_pool_ports.c` (16), `context_compressor_pure.c` (10), `context_compressor_wrappers.c` (4), `conversation_compression_gaps*.c` (7), `scheduler_gaps.c` (2), `base_gaps.c` (2), etc.

**Wave-internal DUP:** `platforms_helpers_flow.c` shares 10 claims with other wave ports (e.g. `base_flow.c`/`platforms_base_flow2.c` family) — decide the owning port per fn.

Fix: per stub-purge doctrine, delete the superseded stub + its PoP tag (keep the flow port); where both bodies are real, keep ONE claim and make the other a wrapper without a tag.

## Pattern 3 — ORPHAN claims (19)

| group | file:line | claim | quarry reality |
|---|---|---|---|
| state plumbing (10) | `context_compressor_flow.c:135–184` | `cc_state_*` → `ContextCompressor.__getattribute__` / `__setattr__` | neither dunder exists in `agent/context_compressor.py`; these are seam-state accessors, not live fns |
| dataclass dunders (5) | `prompt_caching_flow.c:774–828` | `pc_plan_*` → `PromptCachePlan.__init__/__eq__/__setattr__/__delattr__/__hash__` | `PromptCachePlan` is `@dataclass(frozen=True)` — dunders are runtime-generated, absent from AST |
| test-only helper (1) | `dashboard_auth/native_flow.c:15` | `…_u_b64url_no_pad` → `hermes_cli/dashboard_auth/native_flow.py:_b64url_no_pad` | fn exists ONLY in `tests/hermes_cli/test_dashboard_auth_native_flow.py:45`; production module never defines it |
| string-template helpers (3) | `code_execution_flow.c:683,706,716` | `ce_json_parse/ce_shell_quote/ce_retry` → `tools/code_execution_tool.py:json_parse/shell_quote/retry` | names exist only inside the `_COMMON_HELPERS` string literal (generated `hermes_tools` stub source), not as AST defs |

**Aggravating factor for native_flow:** the file is registered in `build/objects.mk` and `nm src/hermes_cli/dashboard_auth/native_flow.o` shows the orphan-claimed symbol **ships in the binary**, yet the file has NO harness and NO oracle — credited, linked, never gated, claim orphaned.

Fix: retag the three plumbing groups as `<state plumbing>` / `<dataclass-generated>` / `<generated-helper (template _COMMON_HELPERS)>` infra annotations (or point at the real defining module); for native_flow add harness+oracle or strip the tag.

## Pattern 4 — MISPLACED tags (141)

Nearly all are **tag-name defects**: the PoP tag claims a C function name that is not the definition it precedes (and usually exists nowhere). The implementation under the tag is real and mostly dispatched — the credit metadata is wrong, so symbol-level tooling (nm sweeps, scanner c_fn matching, collision audits) cannot see it.

| file | affected claims | defect |
|---|---:|---|
| `src/gateway/platforms/photon_adapter_flow.c` | 36 | tags claim `photon__X`; defs are static thunks `ph__X`; harness dispatches by PYTHON name |
| `src/agent/chat_completion_helpers_flow.c` | 24 | first-wave tags claim `cch_flow_X`; defs are `flow_X` (renamed, tags stale) |
| `src/tools/file_operations_flow.c` | 24 | tag names ≠ def names (fresh landing) |
| `src/cli/port_ms_providers_flow.c` | 18 | tag names ≠ def names |
| `src/cli/port_gateway_migrate_flow.c` | 13 | tag names ≠ def names + 1 stacked pair |
| `src/gateway/platforms/buzz_adapter_flow.c` | 21 | tag names ≠ def names |
| `src/agent/conversation_compression_flow2.c` | 3 | stray tag name |
| `src/gateway/platforms/feishu_adapter_flow.c` | 3 | stray tag name |
| `src/agent/display_flow.c` | 1 | stray tag name |
| `src/gateway/hosted_room_driver_flow.c` | 1 | stray tag name |
| `src/gateway/platforms/slack_adapter_flow.c` | 1 | stray tag name |
| `src/gateway/tui_methods_flow.c` | 1 | stray tag name |

(counts = affected claims; 146 total. Each emits NAME-MISMATCH and, when the claimed name is defined nowhere in the file, UNDEFINED.)

**Stacked pair:** `port_gateway_migrate_flow.c:1116` — `/* PoP: gmf_wait_for_live_gateway @ …:_wait_for_live_gateway */` immediately followed by `/* PoP: gmf_poll_until_loop @ agent/deadline.py:poll_until */` before the single def `gmf_wait_for_live_gateway`. The `poll_until` claim sits on the wrong definition.

**Transient (already fixed, do not queue):** 21 corrupted tag fragments in `auth_flow.c` (partial `…l_profile */` remnants between tag and def) present at 12:57, repaired by the landing worker by 12:59. Classic append-corruption shape — worth a post-landing `grep -c PoP` check.

## Pattern 5 — DEAD credit (48)

Claimed fn AND its adjacent definition appear in no driver harness dispatch (checked: strcmp chains, `{"name", fn}` tables in harness AND included port, `DISPATCH(nm)` macro expansions, direct calls).

| file | dead claims | situation |
|---|---:|---|
| `src/cli/port_web_git_flow.c` | 18 | NO harness, NO .o on disk, not in any build/*.mk — fully dead credit |
| `src/cli/port_gateway_migrate_flow.c` | 10 | harness dispatches a subset; these 10 names absent (incl. gmf_host_gateway_owner, gmf_live_gateway_pid) |
| `src/hermes_cli/dashboard_auth/native_flow.c` | 7 | NO harness/oracle but registered+linked (see Pattern 3) |
| `src/gateway/relay_adapter_flow2.c` | 6 | ra2_chat_platform, ra2_text_metadata, ra2_draft_frame, ra2_task_card_metadata, ra2_stamp_slack_unfurl, ra2_mint_and_send_prompt not dispatched |
| `src/gateway/platforms/platforms_helpers_flow.c` | 4 | 4 claims not dispatched |
| `src/tools/skills_tool_flow.c` | 3 | 3 claims not dispatched |

Cleared during audit (do not queue): cch first-wave 24 + cch2 macro-dispatch 30 — the former by the mid-audit landing of `t_port_cch_flow.c`, the latter were an auditor regex bug (see Recursive notes). INFRA plumbing tags are exempt from dispatch checks by design.

## Additional measured findings

- **AMBIG bare-name claims (9):** `config_backend_flow.c` (4: honors_managed_config, config_path, write_changes, deployment_env_names), `pm_packages_flow.c` (3: stage, _probe_env, _asset_names), `port_gateway_migrate_flow.c:497` (to_dict), `chat_completion_helpers_flow.c:1671` (run). Each bare name matches ≥2 class methods; qualify as `Class.fn`. (The 6-way `latest_versions` collision in pm_packages was resolved by class-hint matching — Uv/Python/Nodejs/Npm/Pnpm/Git are distinct methods.)
- **UNBUILT-INCL (72 files):** code-only waves gated via harness `#include` of the port .c; .o absent from build/*.mk. By design for seam-isolated waves, but the credit does not ship in slermes.exe — decide per wave whether to wire into the binary or keep as verified-library code.
- **UNBUILT, no include (2):** `port_runtime_provider_flow.c` (stale .o on disk, in no .mk) and `port_web_git_flow.c` (no .o, no harness).
- **UNTAGGED files (6):** `prompt_cache_clock_flow.c`, `port_run_adapters_flow.c`, `api_server_flow2.c`, `run_shutdown_flow.c`, `run_startup_flow.c`, `session_policy_flow.c` — zero PoP claims; invisible to the scanner (some are registered). Annotate or document as infra TUs.
- **Syntax sweep:** 122/122 files pass winlibs `gcc -fsyntax-only` with build-equivalent flags (`-include hermes_win32.h -D_GNU_SOURCE`, full -I set). 3 initial `asprintf` implicit-declaration failures were the sweep missing `-D_GNU_SOURCE` (build defines it, config.mk:116) — not port defects.
- **nm spot-checks:** `run_flow.o` defines 15 `grun_flow_*` symbols (registered ✓); `native_flow.o` defines the orphan-claimed symbol (ships ungated ✗); `port_web_git_flow.o` absent ✗.

## Fix queue (priority order)

**P1 — false-green at the source**
1. `native_flow.c`: orphan claim + linked + zero gates → add harness+oracle (and rehome/infra-tag the `_b64url_no_pad` claim) or strip tag + unregister.
2. `port_web_git_flow.c`: 18 claims, no harness, no object → wire harness+oracle+objects.mk or strip all tags.
3. `port_gateway_migrate_flow.c:1116`: un-stack the `poll_until` tag (wrong def); dispatch or strip the 10 dead gmf claims.

**P2 — tag integrity (scanner credit correctness)**
4. Repair 146 wrong-name tags: photon (36), cch first-wave (24), file_operations (24), buzz (21), ms_providers (18), gmf (13), stragglers (10) — set tag cfn to the actual def name (keep the py claim).
5. Strip 86 superseded DUP claims: auxiliary_client_gaps (8, stubs verified), credential_pool_ports (16), context_compressor_pure (10)/wrappers (4)/gaps (3)/ports (2), conversation_compression_gaps+gaps2 (7), platforms_helpers_flow wave-internal (10), remainder per histogram. Delete gaps-era stub+tag where the flow port supersedes.
6. Retag 19 orphans: cc_state_* → `<state plumbing>`; PromptCachePlan dunders → `<dataclass-generated>`; code_execution helpers → `<generated-helper>`; native_flow per P1.
7. Qualify 9 AMBIG bare names with their class.

**P3 — coverage & registration**
8. Dispatch or strip: relay_adapter_flow2 (6), platforms_helpers_flow (4), skills_tool (3).
9. `port_runtime_provider_flow.c`: register in objects.mk or strip tags.
10. Annotate the 6 untagged files or document them as infra.
11. pb2_supports_exec_approval_buttons: seam-event the hook-identity check (borderline, documented).
12. Decide ship-vs-library for the 72 include-harness waves (UNBUILT-INCL list in JSON).

## Per-file verdicts

worst = highest-priority verdict among the file's claims (GUILTY>ORPHAN>DUP>AMBIG>MISPLACED>DEAD>INFRA>FAITHFUL). gate: incl-harness = harness #includes the .c; linked = .o in build/*.mk; UNBUILT = neither.

| file | claims | worst | bad | gate | findings |
|---|---:|---|---:|---|---|
| `src/agent/agent_runtime_helpers_flow2.c` | 21 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/auxiliary_client_flow.c` | 78 | DUP | 8 | linked | DUP:8 |
| `src/agent/auxiliary_client_flow2.c` | 40 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/bedrock_adapter_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/chat_completion_helpers_flow.c` | 68 | DUP | 26 | incl-harness | DUP:5, AMBIG:1, MISPLACED:20 |
| `src/agent/context_compressor_flow.c` | 32 | ORPHAN | 20 | linked | ORPHAN:10, DUP:10 |
| `src/agent/context_compressor_flow2.c` | 24 | DUP | 6 | incl-harness | DUP:6 |
| `src/agent/conversation_compression_flow2.c` | 20 | MISPLACED | 3 | incl-harness | MISPLACED:3 |
| `src/agent/conversation_loop_flow.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/display_flow.c` | 23 | MISPLACED | 1 | incl-harness | MISPLACED:1 |
| `src/agent/learning_graph_flow.c` | 4 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/moa_loop_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/port_conversation_compression_flow.c` | 74 | DUP | 7 | linked | DUP:7 |
| `src/agent/port_credential_pool_flow.c` | 42 | DUP | 16 | incl-harness | DUP:16 |
| `src/agent/prompt_cache_clock_flow.c` | 0 | UNTAGGED | 0 | incl-harness | clean |
| `src/agent/prompt_caching_flow.c` | 21 | ORPHAN | 5 | incl-harness | ORPHAN:5 |
| `src/agent/redact_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/agent/tool_executor_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/auth_flow.c` | 38 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/cli_py_flow.c` | 23 | DUP | 2 | incl-harness | DUP:2 |
| `src/cli/cli_tui_mixin_flow.c` | 44 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/commands_flow.c` | 22 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/config_backend_flow.c` | 46 | DUP | 6 | incl-harness | DUP:2, AMBIG:4 |
| `src/cli/config_flow2.c` | 31 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/gateway_agent_cli_flow.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/gateway_cli_flow.c` | 25 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/gateway_migrate_flow2.c` | 26 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/gateway_windows_flow2.c` | 27 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/left_core_migration_flow.c` | 38 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/main_cli_flow2.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/models_flow2.c` | 27 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_agent_runtime_helpers_flow.c` | 10 | FAITHFUL | 0 | linked | clean |
| `src/cli/port_gateway_agent_flow.c` | 11 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_gateway_migrate_flow.c` | 24 | AMBIG | 24 | incl-harness | AMBIG:1, MISPLACED:13, DEAD:10 |
| `src/cli/port_gateway_status_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_gateway_windows_flow.c` | 12 | DUP | 1 | incl-harness | DUP:1 |
| `src/cli/port_gw_session_store_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_main_cli_flow.c` | 18 | FAITHFUL | 0 | linked | clean |
| `src/cli/port_main_desktop_flow.c` | 18 | FAITHFUL | 0 | linked | clean |
| `src/cli/port_model_switch_flow.c` | 18 | DUP | 1 | incl-harness | DUP:1 |
| `src/cli/port_models_catalog_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_ms_providers_flow.c` | 18 | MISPLACED | 18 | incl-harness | MISPLACED:18 |
| `src/cli/port_run_adapters_flow.c` | 0 | UNTAGGED | 0 | UNBUILT NO-HARNESS | clean |
| `src/cli/port_runtime_provider_flow.c` | 18 | FAITHFUL | 0 | UNBUILT | clean |
| `src/cli/port_sessions_cmd_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_stream_consumer_flow.c` | 16 | FAITHFUL | 0 | linked | clean |
| `src/cli/port_tui_dispatch_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/port_update_lock_flow.c` | 10 | FAITHFUL | 0 | linked | clean |
| `src/cli/port_web_git_flow.c` | 18 | DEAD | 18 | UNBUILT NO-HARNESS | DEAD:18 |
| `src/cli/profiles_flow.c` | 36 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/relay_shared_metrics_flow2.c` | 29 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/setup_flow2.c` | 19 | DUP | 2 | incl-harness | DUP:2 |
| `src/cli/update_lock_flow2.c` | 30 | FAITHFUL | 0 | incl-harness | clean |
| `src/cli/web_git_flow.c` | 38 | FAITHFUL | 0 | incl-harness | clean |
| `src/cron/cron_jobs_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/cron/port_scheduler_flow.c` | 41 | DUP | 2 | incl-harness | DUP:2 |
| `src/gateway/gateway_inbound_flow.c` | 20 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/gateway_run_flow2.c` | 28 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/gateway_session_flow.c` | 22 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/hosted_room_driver_flow.c` | 42 | MISPLACED | 1 | incl-harness | MISPLACED:1 |
| `src/gateway/hosted_room_driver_flow2.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/hosted_rooms_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/api_server_flow.c` | 35 | DUP | 2 | linked | DUP:2 |
| `src/gateway/platforms/api_server_flow2.c` | 0 | UNTAGGED | 0 | incl-harness | clean |
| `src/gateway/platforms/api_server_runs_flow.c` | 19 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/base_flow.c` | 117 | DUP | 2 | incl-harness | DUP:2 |
| `src/gateway/platforms/buzz_adapter_flow.c` | 43 | MISPLACED | 21 | incl-harness | MISPLACED:21 |
| `src/gateway/platforms/discord_adapter_flow.c` | 33 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/feishu_adapter_flow.c` | 44 | MISPLACED | 3 | incl-harness | MISPLACED:3 |
| `src/gateway/platforms/google_chat_adapter_flow.c` | 34 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/matrix_adapter_flow.c` | 36 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/openai_routes_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/photon_adapter_flow.c` | 36 | MISPLACED | 36 | incl-harness | MISPLACED:36 |
| `src/gateway/platforms/platforms_base_flow2.c` | 27 | GUILTY | 1 | incl-harness | GUILTY:1 |
| `src/gateway/platforms/platforms_helpers_flow.c` | 27 | DUP | 14 | incl-harness | DUP:10, DEAD:4 |
| `src/gateway/platforms/slack_adapter_flow.c` | 28 | MISPLACED | 1 | incl-harness | MISPLACED:1 |
| `src/gateway/platforms/telegram_adapter_flow.c` | 45 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/platforms/whatsapp_adapter_flow.c` | 30 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/relay/relay_adapter_flow.c` | 14 | FAITHFUL | 0 | linked | clean |
| `src/gateway/relay_adapter_flow2.c` | 20 | DEAD | 6 | incl-harness | DEAD:6 |
| `src/gateway/run_adapters_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/run_busy_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/run_flow.c` | 15 | DUP | 1 | linked | DUP:1 |
| `src/gateway/run_inbound_flow.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/run_notifications_flow.c` | 16 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/run_shutdown_flow.c` | 0 | UNTAGGED | 0 | incl-harness | clean |
| `src/gateway/run_shutdown_flow2.c` | 26 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/run_startup_flow.c` | 0 | UNTAGGED | 0 | incl-harness | clean |
| `src/gateway/scale_to_zero_flow.c` | 6 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/session_authority_flow.c` | 29 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/session_controls_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/session_policy_flow.c` | 0 | UNTAGGED | 0 | incl-harness | clean |
| `src/gateway/tui_methods_flow.c` | 24 | MISPLACED | 1 | incl-harness | MISPLACED:1 |
| `src/gateway/tui_methods_tools_flow.c` | 26 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/tui_server_flow.c` | 26 | FAITHFUL | 0 | incl-harness | clean |
| `src/gateway/turn_progress_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/hermes_cli/config_load_save_flow.c` | 18 | DUP | 2 | incl-harness | DUP:2 |
| `src/hermes_cli/config_migration_flow.c` | 20 | FAITHFUL | 0 | incl-harness | clean |
| `src/hermes_cli/dashboard_auth/native_flow.c` | 8 | ORPHAN | 8 | linked NO-HARNESS | ORPHAN:1, DEAD:7 |
| `src/hermes_cli/kanban_dashboard_flow.c` | 30 | FAITHFUL | 0 | incl-harness | clean |
| `src/hermes_cli/observability/relay_shared_metrics_flow.c` | 10 | FAITHFUL | 0 | linked | clean |
| `src/hermes_cli/pm_packages_flow.c` | 48 | DUP | 5 | incl-harness | DUP:2, AMBIG:3 |
| `src/hermes_cli/port_kanban_db_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/hermes_cli/ws_revive1.c` | 17 | FAITHFUL | 0 | incl-harness | clean |
| `src/hermes_cli/ws_revive2.c` | 19 | DUP | 1 | incl-harness | DUP:1 |
| `src/hermes_cli/ws_revive3.c` | 27 | DUP | 1 | incl-harness | DUP:1 |
| `src/hermes_cli/ws_revive4.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/persistence/conversation_messages_flow.c` | 26 | FAITHFUL | 0 | incl-harness | clean |
| `src/persistence/conversation_sessions_flow.c` | 23 | FAITHFUL | 0 | incl-harness | clean |
| `src/persistence/sqlite_sessions_flow.c` | 24 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/browser_launcher_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/browser_launcher_flow2.c` | 33 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/code_execution_flow.c` | 20 | ORPHAN | 3 | incl-harness | ORPHAN:3 |
| `src/tools/environments/docker_env_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/environments/local_env_flow.c` | 17 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/file_operations_flow.c` | 24 | DUP | 24 | incl-harness | DUP:1, MISPLACED:23 |
| `src/tools/kanban_tools_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/mcp_oauth_flow.c` | 18 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/process_registry_flow.c` | 18 | FAITHFUL | 0 | linked | clean |
| `src/tools/skills_tool_flow.c` | 18 | DEAD | 3 | incl-harness | DEAD:3 |
| `src/tools/terminal_tool_flow2.c` | 20 | FAITHFUL | 0 | incl-harness | clean |
| `src/tools/terminal_tool_lifecycle_flow.c` | 17 | DUP | 2 | linked | DUP:2 |

## Recursive notes — auditing the auditor (false positives killed this pass)

First-pass numbers were WRONG until each instrument was validated against source; the corrections are the recursive layer's findings:

1. **Protocol-vs-concrete shadowing:** `config_backend.py` defines each method 3× (Protocol `...`, FileBackend trivial, module-level delegator). First match won → 12 false GUILTY. Fix: rank behaviors, concrete outranks protocol; self-delegators don't outrank trivial concrete methods.
2. **Nested closures:** 5 `conversation_compression_flow2` + 3 `setup_flow2` + 1 cch 'orphans' are real closures nested in functions — index must ast.walk, not just top-level.
3. **Multi-line macros:** backslash-continued `#define GC_TRY_RAISE…` corrupted brace depth → 13 false DANGLING tags in gateway_cli_flow/run_adapters_flow. Blank directives including continuations.
4. **Dispatch tables live in the PORT:** hrd_table (hosted_room_driver), ss_dispatch, dp_dispatch → 66 false DEAD. Extract table rows + port-side strcmp dispatchers + macro expansions from included ports.
5. **Macro token-pasting:** `cch2_##nm` expansion dropped the underscore (`cch2candidate…`) → 30 false DEAD. Prefix must include the trailing `_`.
6. **Seam vs helper:** name-based `_call$` seam detection misflagged real impl helpers (`tm_hermes_home_scope_call`) → false GUILTY. Seams are signature-identified: 3 params, middle `name`.
7. **Live-behavior subtleties:** identity returns (`return content`), module-constant returns (`return _DISCORD_…_SECONDS` = 4.5), function-local imports, and `contextlib.suppress` + call + None fallback are trivial/delegate, not 'real work' → 6 more false GUILTY cleared.
8. **Bare-name class collisions:** 6 `latest_versions` ports (Uv/Python/Nodejs/…) read as 30 DUPs; class-hint resolution split them; 9 genuinely unresolvable bare claims remain as AMBIG.
9. **String-template 'defs':** grep finds `def json_parse` in code_execution_tool.py but it is inside the `_COMMON_HELPERS` literal — AST is authoritative; those 3 claims are true orphans.
10. **Moving tree:** concurrent landings changed counts between passes (auth_flow corruption appeared and healed; t_port_cch_flow.c landed). Snapshot discipline + re-run before fixing.

## Artifacts

- Machine-readable audit: `C:/Users/eman5/AppData/Local/hermes/cache/scratch/wave13_audit.json` (per-claim findings, live AST behavior, shapes, drivers, registration).
- Aggregated report data: `scratch/wave13_final_agg.json`; syntax sweep: `scratch/wave13_syntax.json`.
- Auditor: `scratch/wave13_audit.py` (pure Python, read-only; re-run to refresh the snapshot).
- No port source, build/, registry.json, libjson, or watercup files were modified. No make/link/oracle-gate/git run.
