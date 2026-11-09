# Monolith split plan (post-TUI-drain)

Organized gap assessment for the structure mandate: split monoliths, reuse
functions, opaque structs + minimal includes + C11 only, no god headers,
self-contained modules.

## Method

For every `src/**/*.c` > 1200 lines: count file-scope statics (scalars +
`static const T NAME[]` tables), static fns, and leaf fns (static fns that
call no other static fn in the file). A **clean island** = a fn/cluster with
zero file-scope-static refs and only external (libc / hermes-header / vtable)
deps. Clean islands move to a new `.c` with a bounded header interface.

## Tier A — clean islands (0 file-scope statics, 0 tables): split now

| File | Lines | Static fns | Leaf fns | Island clusters |
|---|---|---|---|---|
| `src/agent/llm_client.c` | 2548 | 11 | 9 | stream-callback cluster (6 fns: on_provider_stream_chunk, on_stream_chunk, finalize_stream_toolcalls, populate_stream_diag_headers, extract_api_error_context, finalize_stream_diag); compress cluster (compress_content_hash, compress_summarize_tool); _update_env_value |
| `src/cron/port_cron_jobs.c` | 2012 | 20 | 18 | cronjobs secure-dir/file/lock/unlock; iso/time (iso_to_ts, iso_utc_offset, iso_wall_clock); is_cron_field; atomic_write_epoch, epoch_file_age; compute_provider_model_snapshots, normalized_inference_axes; find_job_index; rmtree, prune_job_output, cron_output_keep; cmp_str_desc |
| `src/tools/skills.c` | 2599 | 21 | 12 | fs helpers (skills_dir, ensure_dir, write_file, file_mtime, dir_exists, file_exists); cache (cache_move_to_front, cache_evict_lru); parse (is_valid_version, text_match, parse_deps, is_in_ordered) |
| `src/tools/memory_storage.c` | 1251 | 40 | 31 | vtable backends: inmem (10 fns), sqlite (11), file (5) — each a self-contained backend |
| `src/tools/curator_backup.c` | 1270 | 15 | 8 | tar/backup: utc_timestamp, is_valid_snapshot_name, is_excluded_top_level, tar_write_header, tar_write_file_data, extract_tar_gz, copy_file, count_json_jobs |
| `src/agent/provider_anthropic.c` | 3334 | 6 | 1 | normalize_model_key (+ already split provider_endpoint_detect) |
| `src/cli/config.c` | 1814 | 2 | 2 | get_slermes_home, parse_env_file |
| `src/tools/terminal.c` | 2110 | 5 | 5 | ssh_sync_upload/download, _inject_sudo_failure, looks_like_env_assignment, _looks_like_help_or_version_command |

## Tier B — table-coupled (file-scope tables): extract state first, then split

| File | Lines | Tables | Note |
|---|---|---|---|
| `src/cli/display_core.c` | 3123 | `SPINNER_CHARS` | diff subsystem `g_ansi_diff_*` coupled to `g_display_skin`/`skin_get`. |
| `src/cli/tui_fullscreen.c` | 3018 | `slash_registry` | overlay layer drained; core engine cohesive. |
| `src/agent/provider_metadata.c` | 2912 | `MODELS`, `PROVIDERS` | capability cluster reads the tables. |
| `src/cli/config_setup.c` | 2709 | `SETUP_KEY_INFO` | setup wizard key table. |
| `src/skills_hub.c` | 2115 | `g_wellknown_skills` | well-known skills table. |
| `src/agent/auxiliary_client.c` | 1671 | 5 tables | AUX_MODEL_FALLBACK, CHAIN_LABELS, PROVIDER_ALIASES, TASK_LABELS, VISION_MODELS. |
| `src/agent/prompt_builder.c` | 1586 | `THREAT_PATTERNS` | threat-pattern table. |
| `src/agent/coding_context.c` | 1335 | 3 tables | EDIT_FORMAT_FAMILIES, JS_LOCKFILES, PY_LOCKFILES. |

## Tier C — cohesive engines (high static-fn + shared state): leave, or extract only pure leaves

`src/web_dashboard.c` (1754, 55 static fns), `src/api_server.c` (1776, 35),
`src/gateway/status.c` (1361, 19), `src/gateway/platforms/*` (per-platform
adapters, already modular), `src/tools/computer_use.c` (2216, 19),
`src/tools/mcp_tool.c` (1931, 7), `src/tools/registry.c` (1309, 5).

## Execution order

1. `llm_client.c` stream-callback cluster -> `llm_stream.c` (Tier A, biggest clean win).
2. `port_cron_jobs.c` utility islands -> `cron_jobs_util.c` (Tier A).
3. `skills.c` fs/cache/parse islands -> `skills_fs.c` (Tier A).
4. `memory_storage.c` vtable backends -> `memory_vtable_inmem.c` / `_sqlite.c` / `_file.c` (Tier A).
5. `curator_backup.c` tar helpers -> `curator_tar.c` (Tier A).
6. Tier B: extract state tables into module-internal headers first, then split.

Every split gated: full `make all` Phase 5 RC=0 + oracle suite 95/95 + tree clean.

## Progress log (newest first)

| Commit | Split | Result |
|---|---|---|
| `924b06c` | `config_setup.c` setup wizard -> `config_setup_wizard.c` (Tier B) | 2709 -> 1323 |
| `5e0efa8` | `terminal.c` -> `terminal_cmd.c` (+ dead-dup removal) | 2110 -> 1979 |
| `dcf3a3b` | `kanban_workers.c` -> `kanban_workers_db.c` | 2407 -> 2164 |
| `62c9bcf` | `config.c` env-file helpers -> `config_env_file.c` | 1814 -> 1705 |
| `b82cac8` | `port_tools_session_search_tool.c` -> `session_search_tool.c` | 1305 -> 808 |
| `c615153` | `port_gateway_run.c` -> `gateway_run_match.c` | 1393 -> 1261 |
| `7535925` | `curator_backup.c` -> `curator_tar.c` | 1270 -> 1047 |
| `904f0b8` | `provider_anthropic.c` -> `provider_model_caps.c` | 3334 -> 3283 |
| `96e3373` | `port_cron_jobs.c` -> `cron_jobs_time.c` | 2012 -> 1965 |
| `fa403d0` | dedup: `str_tolower_inplace`/`str_strip_dup` -> `hermes_strutil.h` | — |
| `fcb6bfb` | `llm_client.c` -> `llm_stream.c` | 2548 -> 2109 |
| `e938d17`..`0de1a5c` | TUI overlay splits (7 commits) | 5169 -> 3018 |

Deferred (with reasons): `skills.c` (scan cluster calls `read_file` /
`extract_frontmatter`, defined outside the file and not in an includable
header); `memory_storage.c` (vtable dispatch, needs the vtable struct shared
first); `mcp_tool.c` (6 file-scope statics + 2 local typedefs — moving it
needs a god-header); `provider_metadata.c` (the HTTP/OAuth cluster calls ~17
exported-but-undeclared fns, so the interface would be huge); Tier B
table-coupled files not yet attempted (`display_core.c`, `skills_hub.c`, `auxiliary_client.c`, `prompt_builder.c`,
`coding_context.c`).

Name-collision backlog (rename candidates, not yet done): `setup_slack` /
`setup_telegram` exist as different functions in `config_setup.c` (wizard,
static) and `gw_setup.c` (gateway, exported).

## Parity gaps found while splitting (not splits)

### weixin file upload does not encrypt (real gap)
`src/gateway/platforms/weixin.c` exports five AES helpers with PoP
annotations - `weixin_aes128_ecb_encrypt`, `weixin_aes128_ecb_decrypt`,
`weixin_parse_aes_key`, `weixin_pkcs7_pad`, `weixin_aes_padded_size` - but
**none has a caller anywhere in the repo** (verified by grep over `src/`,
`include/`, `tests/`).

The Python reference does use them: `plugins/platforms/weixin/adapter.py:1152`
calls `_aes128_ecb_encrypt(plaintext, aes_key)` inside `_send_file`, which
then uploads the ciphertext to the CDN.

The C `send_file()` (weixin.c) does something different: it posts a
`msg_type: 6` message whose `content` is the file URL. It never reads the
file, never encrypts, never uploads. So the C upload path is missing the
encrypt-then-upload step the reference performs. Wiring it is a feature, not
a split, and is left undone here - flagged so it is not mistaken for parity.

### weixin markdown cluster is dead
`weixin_split_text_for_weixin_delivery`, `weixin_normalize_markdown_blocks`,
`weixin_guess_chat_type` and `weixin_wrap_copy_friendly_lines_for_weixin` are
never called from outside their own cluster; only
`weixin_truncate_message` and `weixin_split_markdown_blocks` have internal
callers. Splitting the cluster would move dead code, so it was skipped.

### tui_json_rpc.c is not a split target
Its 89 "clean" functions are the JSON-RPC handlers plus the `s_methods[]`
registration table they are dispatched through. Moving handlers without the
table (or vice versa) breaks the dispatch; moving both is a rename, not a
split.

## tui_fullscreen.c: render core reached, plus a dead exported API

After nine splits (tui_state.h types + 8 modules) tui_fullscreen.c is 1963
lines, down from 5169. What remains is:

- the genuinely live render core - tui_history_add (12 refs),
  tui_redraw_history (10), tui_redraw_status (5), tui_fullscreen_run (4),
  tui_render_markdown (3), tui_redraw_input (3), tui_process_input (3).
  These share the `tui` global and the curses windows, so splitting them
  would ship that state to a shared header (soul.md forbids a god header).
- an exported-but-DEAD api. These are declared in tui_fullscreen.h and
  defined here, but a repo-wide grep finds no caller anywhere:
  tui_fullscreen_session_create, tui_fullscreen_session_resume,
  tui_fullscreen_session_close, tui_fullscreen_session_finalize,
  tui_fullscreen_start_agent_build, tui_fullscreen_wait_agent,
  tui_fullscreen_resolve_model, tui_fullscreen_switch_model,
  tui_gateway_connect, tui_gateway_process_message, tui_gateway_send.
  They look live (exported, declared, non-static) but nothing calls them.
  Splitting them would move dead code, so they were left alone and are
  recorded here instead.

## acp/server.c: the handlers cannot move without publishing the struct

The 20 `handle_*` functions (27 KB) look like a clean handler cluster and
`dispatch_request` is their only caller, but they dereference
`srv->sessions[...]`, `srv->pending_approval`, `srv->permission_pending` etc.
DIRECTLY. `acp_server_t` is deliberately opaque - `include/acp/server.h` has
only `typedef struct acp_server_t acp_server_t;` and the struct is defined
in server.c.

Moving the handlers would require publishing that struct in a shared header,
which is exactly the god-header / lost-opacity outcome soul.md forbids. An
attempt was made and reverted. The file is now 232 lines after the
`acp_protocol.c` split; the handlers stay.

Same pattern as `computer_use.c`'s backend registry and `port_memory_tool.c`'s
`entries_for` / `mutable_entries_for` / `char_limit_for` (which dereference
the opaque `memory_store_t`).

## Systemic: generic helper names collide across translation units

Splitting this session produced FOUR link-time `multiple definition` errors
from unrelated functions sharing a generic name:

| name | module A | module B | resolution |
|---|---|---|---|
| `utc_now_iso` | `shell_hooks.c` - `char *(void)` | `status.c` - `void (char*, size_t)` | renamed `utc_now_iso_buf` |
| `send_json` | `api_server_http.c` | `web_dashboard_http.c` | renamed `api_send_json` |
| `free_tokens` | `gateway_status_proc.c` | `vault_login_classifier.c` | renamed `vl_free_tokens` |
| `ensure_dir` | `skills_hub_catalog.c` - recursive `fs_mkdir_p` | `setup_wizard.c` - plain `mkdir` | renamed `wiz_ensure_dir` |
| `file_exists`, `dir_exists` | `file_ops.c` | `skills.c` | renamed `skills_file_exists`, `skills_dir_exists` |

Note `ensure_dir` is not merely a name clash: the two bodies have DIFFERENT
semantics (recursive vs single-level), so they are genuinely two functions
that happened to share a name. The same is true of `utc_now_iso`.

**Practical rule for future splits:** before making a moved function
non-static, grep the whole tree for the bare name. A name that looks
module-private may already be exported elsewhere. Renaming the NEW copy (not
the pre-existing one) is the low-risk fix.

## skills.c: only the filesystem cluster was movable

After `skills_fs.c` (01b65ae) skills.c is 2500 lines. The remaining clusters
are all coupled, for three different reasons:

- **cache** (`skill_cache_*`, 4.9 KB) mutates the file-scope `g_cache` state
  and `skill_cache_preload` calls `skills_scan_all` -> `scan_dir_recursive`
  -> `scan_add_skill` (5.6 KB), so moving it drags in the whole scanner.
- **handlers** (`skills_*_handler`, 26 KB) have 28 distinct dependencies on
  the skill/bundle/curator/deps/search helpers - they ARE the tool's outer
  layer, not an extractable island.
- **bundles / curator / sync** are each tied to the others through
  `g_curator_stale_days` and the cache.

Left alone deliberately.

## port_memory_tool.c: the store API is the opaque struct

After `memory_entries.c` (546520b) the file is 744 lines. The remaining
`memory_store_*` cluster (24 fns, 20 KB) is the implementation of
`memory_store_t`, whose struct is defined in this file -
`include/memory_store.h` only has the opaque
`typedef struct memory_store_t memory_store_t;`.

Moving the store API means moving the struct definition, which would either
publish it or require a module-internal header carrying it. That is a
deliberate design change (a `memory_store.c` module), not a mechanical split,
and was not attempted. Same shape as `acp/server.c`.

## weixin markdown cluster: no external caller (possible parity gap)

The 17-function markdown cluster moved to `weixin_markdown.c` (aae3157) has
**no external caller**. Tree-wide:

| fn | refs | where |
|---|---|---|
| `weixin_truncate_message` | 2 | def + 1 intra-cluster call |
| `weixin_split_text_for_weixin_delivery` | 1 | def only |
| `weixin_normalize_markdown_blocks` | 1 | def only |
| `weixin_split_markdown_blocks` | 3 | def + 2 intra-cluster |
| `weixin__is_fence` | 4 | def + 3 intra-cluster |

Python *does* define `truncate_message`
(`gateway/platforms/base.py:4776`) as an adapter method, and the send path
uses it (`base.py:1831` notes the router skips truncation because `send()`
chunks natively). The C weixin send path never calls the port.

Not deleted: the port's job is parity, and the C port may be a pre-existing
behaviour gap rather than dead weight. **Open question** - should the C weixin
`send_message` path call `weixin_truncate_message`, or is the split a
deliberate Windows-port deviation?

## weixin.c: the HTTP/token cluster is anchored to `static weixin_state_t g_wx`

After `weixin_http.c` (8bb978f) weixin.c is 874 lines. Nine helpers looked
extractable by function-dep analysis, but six read the file-scope
`static weixin_state_t g_wx;`:

- `build_headers`, `wx_api_post`, `ctx_token_path`, `sync_buf_load`,
  `sync_buf_path`, `sync_buf_save`

Moving any of them means moving `g_wx` (or turning it into a passed-in
parameter, which changes every call site in the send/poll path). That is a
design change, not a mechanical split, so they stay. Same shape as the ACP
handlers and the skills cache.

Also note: `ctx_token_path` had to be made non-static because the moved
`ctx_token_save` calls it.

## yuanbao_md_* : published API, no callers (second dead-code cluster)

After `yuanbao_md.c` (c44f4f3) yuanbao.c is 935 lines. The twelve
`yuanbao_md_*` helpers were moved out; like the weixin markdown cluster they
have **no external caller**. Each is *declared* in
`include/hermes_gateway_yuanbao.h` (e.g. line 124 for
`yuanbao_md_chunk_markdown_text`), so they are published API rather than
private dead code - but nothing outside the cluster calls any of them.

That is now two independent markdown-chunking clusters (weixin, yuanbao) with
the same shape: ported, declared, and never wired into the send path. Worth a
single follow-up decision rather than two.

## yuanbao signature cache stays

`yb_sign_find`, `yb_sign_cache_set`, `yuanbao_compute_signature`,
`yuanbao_is_cache_valid`, `yuanbao_get_refresh_lock`, `yuanbao_clear_locks`,
`yuanbao_purge_expired`, `yuanbao_build_timestamp` all touch the static
`yb_sign_mutex`. Same anchor pattern as weixin's `g_wx` and ACP's session
table.

## yuanbao frame encoders stay (anchored to `static yuanbao_state_t g_yb`)

`encode_auth_bind`, `encode_ping_req`, `encode_send_c2c`,
`encode_send_sticker` look extractable - no external *function* deps - but
they all read the file-scope `static yuanbao_state_t g_yb;` (line 67) and use
the file-local `CMD_AUTH_BIND` / `CMD_PING` / `YB_INSTANCE_ID` defines.
`encode_send_c2c` also calls `encode_conn_msg`.

An extraction was attempted and reverted: the module failed with 15 errors
(`g_yb` undeclared, `CT_REQUEST`/`CMD_*`/`YB_INSTANCE_ID` undeclared,
`encode_conn_msg` implicit). Moving them means moving `g_yb`, which is the
same anchor pattern as weixin's `g_wx` and the ACP session table.

## api_server.c: handlers anchored to `static agent_state_t *g_agent`

After `api_server_sse.c` (4511c5b) and `api_server_readonly.c` (ef061e1),
api_server.c is 1521 lines (from 1639).

Seven handlers stay because they read file-scope statics:

- `handle_get_response`, `handle_tools_list`, `handle_agent_status`,
  `handle_get_models` - `static agent_state_t *g_agent` (line 71)
- `handle_health_detailed` - `g_port`, `start_time`
- `handle_metrics_get`, `handle_service_info` - `g_request_count`,
  `g_start_time`

First extraction attempt moved all eleven; it failed with `g_agent`
undeclared and was reverted to the four genuinely clean handlers.

## Duplicate symbol found by the SSE split

`api_server.c` had a `static send_sse_headers` byte-for-byte identical to
`gateway/platforms/api_server_adapter_handlers.c:46`. Static-ness hid the
clash; un-static-ing it to move it produced `multiple definition`. Fixed by
deleting the legacy copy and including
`gateway/platforms/api_server_adapter.h` from api_server.c.

## checkpoint_manager.c: `_cm_in_dirset` / `_cm_add_dir` need the opaque struct

After `checkpoint_fs.c` (b17b931) checkpoint_manager.c is 669 lines (from 771).
Ten of the twelve candidate helpers moved. Two stay:

- `_cm_in_dirset`, `_cm_add_dir` - both dereference `self->` on
  `tool_checkpoint_mgr_t`, which is an opaque typedef in
  `include/checkpoint_manager.h:27`; the struct body lives only in the .c.

Same opaque-struct anchor as the ACP session table, the memory store and the
skills cache. Moving them means publishing the struct or passing its fields
explicitly.

Note: `_run_git` and `_shadow_repo_path` are declared in
`src/tools/checkpoint_store.h` (split in 4bb8563), so the new module includes
that rather than redeclaring them.

## pairing.c: two private types anchor nearly everything

After `pairing_util.c` (bbc849c) pairing.c is 628 lines (from 643). Only
`hash_code` and `skip_ws` were extractable. Anchored:

| fns | anchor |
|---|---|
| `obj_get`, `obj_set_num`, `obj_set_str`, `obj_del`, `obj_has`, `pj_free` | `pj_val_t` - `struct pj_val` is private to pairing.c:30 |
| `load_json`, `save_json`, `path_for`, `pairing_store_open`, `pairing_store_close` | `pairing_store_t` - private struct |
| `uid_aliases`, `uids_match`, `normalize_uid` | look aliases up in the store |

## xstrdup is defined in eight places

Tree-wide `char *xstrdup(const char *)` definitions:

- `src/agent/llm_client.c:47` (static)
- `src/agent/verification_evidence.c:19` (static)
- `src/cli/port_cli_profiles.c:41` (static)
- `src/cli/port_gateway_signal_format.c:23` (static, `strdup(s ? s : "")`)
- `src/cli/port_models_pure.c:51` (static)
- `src/cli/port_models_validate.c:31` (static)
- `src/cron/cron_jobs_paths.c:40` (**non-static**, malloc+memcpy)
- `src/gateway/pairing.c` (**non-static**, `s ? strdup(s) : NULL`)

The two non-static copies differ in body, and `cron_jobs_paths.c:40` is linked
into the same binary - moving the pairing copy produced
`multiple definition of xstrdup`. Left static in pairing.c. **Needs one
decision: pick a canonical xstrdup (or rename each), then dedupe all eight.**
Same class of defect as the `send_sse_headers` duplicate.

## browser.c: CDP globals anchor the whole command layer

After `browser_dom.c` (4dd829d) browser.c is 1747 lines (from 1833). Only
`extract_title` and `html_to_text` were extractable.

| fns | anchor |
|---|---|
| `cdp_get_url`, `cdp_set_url` | `static g_cdp_url` |
| `cdp_send_command` | `static g_cdp_msg_id` |
| `cdp_supervisor_ping`, `cdp_generate_pdf` | call the two above (transitive) |
| `browser_supervisor_handler` | calls `cdp_supervisor_ping` (transitive) |
| `index_elements` | `static g_tab` |

Three revert cycles. Note the transitive case: a cluster can pass the
`ext=[]` test with the helpers INCLUDED and still be unmovable once the
helpers are left behind - re-run the dep check after each exclusion.

## Browser / pairing / models_dev: the session's anchor inventory

`g_cdp_url`, `g_cdp_msg_id`, `g_tab`, `g_wx`, `g_yb`, `yb_sign_mutex`,
`g_agent`, `g_port`, `g_request_count`, `g_start_time`, `INVISIBLE_CPS`,
`g_models_dev_cache_time` are all file-scope statics that pin their clusters.
Opaque structs pin the rest: `pj_val_t`, `pairing_store_t`,
`tool_checkpoint_mgr_t`, `memory_store_t`, `acp_server_t`, `skills_cache`.

## port_patch_parser.c: local structs are RICHER than the public view

After `patch_parser_util.c` (c6c5221) port_patch_parser.c is 346 lines (from
431). Only `match_file_marker`, `match_move_marker`, `emit_json_str`, `trim`
and `xstrndup` were extractable.

The .c typedefs its own `patch_op_t` / `hunk_t` / `hunk_line_t` /
`parse_result_t` / `op_type_t`, and they are NOT the same types as the public
`patch_*_t` in port_patch_parser.h - the local ones carry extra capacity fields
(`cap_lines`, `cap_hunks`, `cap_ops`) that the public view omits. So they cannot
be swapped for the public names, and every function touching them is anchored:
`hunk_push_line`, `result_push_op`, `finalize_hunk`, `close_cur_op`,
`result_free`, `op_free`, `hunk_free`, `patch_parser_result_free`,
`patch_parser_print_canonical`, `op_name`.

`op_type_t` IS just an alias of the public `patch_op_type_t`, but the .c's own
`typedef enum { OP_ADD, ... }` redeclares the same enumerators as the header, so
including both in one TU is a redeclaration error. Five revert cycles.

**Lesson:** when a .c typedefs a type that a public header also defines, compare
the STRUCT BODIES before assuming they are the same type. Extra fields mean the
local one is a different (richer) type and the functions using it are anchored;
identical enumerators mean including both headers in one TU is itself an error.

## session_to_json: third duplicate symbol, this one self-inflicted

`session_to_json` existed twice with DIFFERENT signatures and bodies:

- `src/gateway/platforms/api_server_adapter_sessions.c` -
  `json_t *(session *)`, dates to the base commit 82c953c
- `src/acp/acp_protocol.c:143` - `json_node_t *(acp_session_t *)`, which my
  9d5361f split un-static'd and thereby exposed

Fixed in 4f8f488 by renaming the NEWCOMER (the ACP one) to
`acp_session_to_json` - definition in acp_protocol.c, declaration in
acp_protocol.h, five call sites in acp/server.c. The pre-existing adapter
owner was left alone.

This is the third duplicate-symbol defect of the session and the second caused
by my own un-static-ing (send_sse_headers was the first). **When a split dies
with `multiple definition of X`, check `git log --oneline -S "X" -- <both files>`
to see which definition is older before picking which one to rename.**

## DIRECTION: Lua replaces curses (and SDL, and C#)

Per the WWW repository (`waefrebeorn/WWW`, the ~WuBu~ world-space engine), the
UI / game-logic / modding substrate is **Lua**, not ncurses and not SDL:

> C11 owns memory, the ECS, physics and the spatial index; Lua owns game
> logic, GUI and mods.

Consequences for this repo:

- The ncurses TUI is a **dead end on Windows**. It cannot link (no ncurses
  import libs - `lib/libncurses/lib/` does not exist, so `HAS_NCURSES=0`) and
  cannot compile (`mkfifo` does not exist anywhere in the MinGW headers).
  `make all` excludes every TUI object; only the Linux-only `tui` target
  references them. The header fixes in 8a88016 are kept because they are real
  defects and the headers are shared, NOT because `make tui` is a goal.
- The path forward is the **slerm-Lua** interpreter, ported in 1848df3 as
  `src/lv/` (slv_*). It is self-contained (standard C headers only), builds
  clean, and passes 27 + 5 tests. It is the substrate for a Lua-driven UI.

Open question: how far to take the TUI object registrations in
`build/config.mk` now that the target is being superseded - keep them correct
(cheap, honest) or drop TUI_OBJ entirely until the Lua UI lands.
