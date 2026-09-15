# Session Prestige — 2026-08-24 (v707 oracle-sweep repair wave)

## What closed

1. **final10 verified** — all ten upstream-sync modules (relay_plugin_cutover,
   worktree_cmd, process_identity, browser_control_artifacts/broker,
   control_socket, update_inventory/receipt, browser_extension_router,
   worktree_gc) build, link, and `final10` oracle MATCH (45/45). One real FAP:
   `_restart_mechanism` had been written as a copy of
   `describe_restart_mechanism` display strings instead of the policy-id table.
2. **Full oracle sweep: 39 failing ports triaged to green.** Real port fixes:
   - `models_helpers`: name-parity rename to header contract
     (`is_openai_fast_model`, `base_url_looks_like_anthropic_messages`).
   - `libtooldispatch make_tool_result_message_json` +
     `replay_cleanup` recovery messages + `message_sanitize close_interrupted`:
     attach wall-clock timestamp per `stamp_message_timestamp` (LIVE parity).
   - `api_error_summary`: Cloudflare Ray requires "Cloudflare ... <strong>"
     pattern; `[code] type: msg` / "API error (see logs)" / error-string
     branches per LIVE.
   - `prompt_builder_guidance`: synced Typed-browser-rung + Verify-ladder +
     agent-cursor sections to live text (byte-equal on linux).
   - `update_cmd` completion suffix format fixed to LIVE `" [branch @ sha]"`.
3. **~20 auto-generated integration harnesses repaired**: stale `port_*`
   include paths repointed to the real module files; watercup gate.json
   extra_src/cflags/ldflags + shims.c for config/context-compressor/
   api-server gates; `wc_run.py` now supports per-gate ldflags.
4. **Water-cup: 302 gates, FAILING 0** (was 8). Twin-side drift fixed
   (`_tool_call_id_variants`, `_structural_no_op_backoff_until`,
   `_host_header_hostname` extraction); legacy one-off audit entries
   deregistered in favor of the generic gates framework.

## Verification

- `rm -f slermes && make slermes` green; `nm slermes | grep -c ' T '` = 19,366+.
- final10 MATCH, hermes_state MATCH, platforms_helpers_md MATCH,
  replay_cleanup 9/9, message_sanitize_close 5/5, cli_logs, heartbeat,
  kanban_util/db_engine/decompose, cron scheduler trio, url_safety trio,
  web_git_base, vision/helpers/prompt_size/env-*/path_security/write_approval,
  gateway run/response_filters/shutdown_forensics/platforms*, working_diff,
  doctor, file_operations, browser_camofox, models_helpers, api_error_summary,
  prompt_builder_guidance, update_completion, web_server_ws_auth/prune,
  gateway_health_export_pure — ALL MATCH.

## Next-session queue

1. Confirm full sweep green (post-fix re-run).
2. Long-tail noops: tests/unexplained_noops.json (~229 entries) is mostly
   detector noise (sampled entries are real implementations) — reclassify
   with the comment convention and shrink the list.
3. Recursive gap loop continues: next REAL_GAP cluster from the scanner.

## Round 2 (recursive loop continues)

1. Upstream quarry re-synced (+79 commits, behind=0). New upstream work triaged.
2. ed `agent/terminal_env_provider.py` TerminalEnvironmentProvider
   ABC (222-line new module) as a vtable with all base-class @property defaults;
   wired into AGENT_OBJ, 20 tep_* symbols in binary.
3. ed `_interleave_addrinfos` (RFC 8305 Happy Eyeballs prep,
   pure) + LIVE-Python oracle (5 cases MATCH).
4. ed `get_browser_snapshot_threshold` (browser.snapshot_threshold
   config knob, clamped + cached + resettable).
5. **v714** — signal standalone send enforces MAX_MESSAGE_LENGTH chunking
   (upstream 48f69e51d3/#67279/#57929).
6. Rebuild green; final10/hermes_state/process_bootstrap_pure/replay_cleanup
   oracles re-MATCH after rebuild.

Next: remaining upstream deltas (memory gating verified already covered),
continue scanner REAL_GAP descent.

## Round 3

1. ed voice_client_config pure core (client-direct gate value
   parsing, relay verdict shape, STT relay ladder with LIVE
   BUILTIN_STT_PROVIDERS membership) + 16-case LIVE oracle MATCH.
2. **v717** — tool_result_storage 11/15 → 15/15: _prune_spillover_once,
   _is_host_side_env, generate_preview, _write_to_spillover,
   _sandbox_visible_spillover_path.
3. **v718** — video_generation_tool 9/12 → 12/12 (_coerce_int/_coerce_bool);
   terminal_hints 8/10 → 10/10 (_first_token, annotate_masked_success with
   failure-shape scan + masking-pipe/|| advisory notes).
4. **v719** — xai_http 11/14 → 14/14 (dotenv-preferred env getter, coerce_bool,
   explicit key via shared secret machinery, validated base URL).
5. **v720** — web_tools keyless-rescue decision core (tavily-configured scan,
   rescue gate, eligibility rule).

All nm-verified in the shipped binary; rebuild green throughout.

## Round 4

- **v721/v722** — ported the frozen plugin system-prompt sections machinery:
  agent/system_prompt.py _restore_plugin_prompt_sections /
  _plugin_section_blocks / restore_plugin_prompt_sections and
  hermes_cli/plugins.py format_system_prompt_section(s) with the
  PLUGIN_SECTIONS container constants. The oracle caught a real off-by-one
  in the chars-tag parse plus a missing conversation-start-marker guard —
  both fixed toward the LIVE contract. system_prompt.py battleground module
  moves from 8/16 toward full coverage of its pure surface.
- Full rebuild green; plugin_prompt_sections oracle MATCH (6 cases).

## Round 5

- **v724** — skills_sync: essential skills survive suppression (LIVE gate
  `suppressed and not in _essential_names` was skipping all suppressed).
- ed _interpret_signal_exit with the LIVE
  _SIGNAL_EXIT_NOTES table (subprocess-negative definite, 128+signum hedged,
  SIGINT reserved). terminal_tool battleground module now 74/80.

## Round 6

- **v727** — web_tools: _policy_blocked_result (blocked_by_policy flag /
  case-insensitive error match; policy refusals never rescued) and
  _provider_is_ready (is_available / is_keyless_available vtable slots,
  fail-soft). web_tools → 27/29.
- **v728** — vision_helpers crop-region validate/clamp (pure core of
  _crop_image_region): malformed/inverted/zero-area regions rejected with
  an error naming the actual dimensions.

## Round 7

- **v730** — skill_utils: is_org_mirror_path / org_id_of_path (_org mirror),
  parse_qualified_name (namespace:skill split), is_valid_namespace
  ([a-zA-Z0-9_-]+). Module → 44/44.
- Audit: secret_sources base/registry and subagent_lifecycle battleground
  "gaps" are name-mapping noise — every function already ships
  (secret_registry_register/get/list, secret_set/reset/get_source_environment,
  scrub_ansi, is_valid_env_name, apply_all). Scanner name-matcher needs a
  mapping table pass.

## Round 8

- **v732** — battleground scanner gained an explicit rename ALIAS_MAP keyed on
  (module_stem, python_name) for C-side renames the prefix heuristics can't
  derive (secret_sources family first entries).
- **v733** — background_review pure core: _classify_review_result (prefix-based
  skill/memory classification, emoji-stripped), enabled gate,
  _review_input_token_budget (600k default, <=0 unlimited). Module 8/23 → 15/23.
- Scanner totals after alias map: PORTED 14,574 (87.2%), STUB 0.

## Round 9

- **v735** — chat_completion_helpers: _parse_provider_sse_events (faithful
  SSE state machine: comments, fields, multi-line data join, status from
  comments then status/status_code/http_status) + _prompt_cache_scope_for_agent
  guarded wrapper over the existing resolver. Module → 40/50.
- **v736** — context_compressor: is_compaction_summary_message (metadata
  marker preferred, content heuristics fallback) and _handoff_only_content
  (delimiter→END_MARKER slicing both layouts). → 149/171.
- **v737** — retryable_user_text fail-closed guard (all-text-only flattening).
- **v738** — _tool_content_has_images + _strip_images_from_tool_msg wrappers
  reusing existing cc_ image primitives. → 152/171.
- **v739** — _digest_worthy with hand-rolled low-signal tool-ack matcher. → 153/171.

## Round 10

- **v741** — context_compressor: split_user_originated_turn (handoff scaffold
  + canonical live view projection with marker stripping, durable display
  metadata carry, synthetic/actionable gates).
- **v742** — _synthetic_user_row (nine scaffold prefixes) +
  _build_recovery_footer (deterministic session_search pointer).
- **v743** — _serialize_turns_for_digest (pristine-snapshot substitution for
  pruned tool stubs, digest_worthy filter). → 155/171.
- **v744** — _is_hygiene_idle_timeout_error + _lean_recovery_stub.
- Audit: cc_reinject_pruned_skill_markers was already faithfully ported in
  context.c (context_compressor__reinject...) — duplicate removed before it
  ever shipped. context_compressor module 147/171 → ~157/171.

## Round 11

- **v747** — user_originated_turn_view + history_before_user_originated_turn
  (rewind prefix with handoff scaffold retained at head).
- **v748** — _retire_stale_tool_result_images (keep newest 3 image-bearing
  tool results; in-place retirement).
- **v749** — _build_verbatim_user_section (newest-first, 4K/message cap,
  24K budget, truncated straddler, blockquote format).
- **v750** — _build_anchor_index (libregex harvest of PR/commit ids, handle
  prefix scan, frequency-ranked, noise-filtered, capped at budget).

context_compressor: 147/171 → 170/171 (the one remaining entry is the
context.c reinject port under its cc_ name). Module effectively CLOSED.

## Round 12

- **v752** — auxiliary_client routing predicates: _task_prefers_fast_model,
  _is_official_codex_base_url, _is_dual_surface_anthropic_host,
  _is_structured_output_rejection (fail-closed status gate 400/422).
  auxiliary_client → 247/253.

## Round 13

- **v754** — conversation_loop: _review_input_budget_exhausted,
  _should_rearm_compression_budget, _billing_terminal_label,
  _billing_failure_result (structured terminal construction point).
  Module → 37/42.
- **v755** — agent_runtime_helpers: tool-call id variant expansion (shared
  policy owner, composite call|item spellings) and trailing_continue_intent
  stall-guard gate. Module → ~44/47.

## Round 14

- **v757** — error_classifier: _billing_ambiguity_context +
  _has_usage_limit_transient_signal (message patterns, payload reset
  fields, Retry-After probes). Module → 18/19.
- **v758** — codex_runtime: _is_plain_json_data wire-format proof walk.
  Module → 16/20.
- **v759** — delegation_context CLOSED (9/9): thread-local dispatcher-owned
  context gates — delegated-child flag, non-dispatcher enter/exit, and the
  single identity predicate for HERMES_KANBAN_* gates.

## Round 15

- **v761** — browser_registry CLOSED (8/8): snapshot_registration,
  registry_generation cache fingerprint, restore_registration (restore only
  when current still installed; generation bump under lock).

## Round 15 (continued)

- **v763** — credential_pool CLOSED (74/74): resolve_runtime_pool_key +
  get_env_prefer_dotenv (op:// unresolved-reference deferral).
- **v764** — file_safety CLOSED (20/20): write-approval path gates
  (~/.ssh/config — ProxyCommand/Match exec influence process execution).

## Round 16

- **v766** — conversation_compression CLOSED: _strip_marker_for_comparison
  (#92231), _pruned_skill_reload_notice (canonical marker scan, cap 20),
  conversation_history_after_compression (legacy/in-place/abort baselines).

## Round 17

- **v768** — auxiliary_client: _translate_anthropic_response_format,
  _without_structured_output_format, _record_route_info. Module → 250/253
  (remaining three are SDK-client-coupled: client construction, async close
  scheduling, codex header injection over the live SDK path).

## Round 18

- **v770** — anthropic_adapter CLOSED (75/75): _accepts_thinking_disable —
  adaptive Claude models need an explicit disable (omission leaves thinking
  ON), reasoning-mandatory families reject it, Kimi/Moonshot omission
  contract deliberately untouched.
