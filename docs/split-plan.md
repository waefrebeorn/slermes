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
| `src/web_server.c` | 3437 | `routes` | 84 static fns dispatched through the routes table — a registry, not an island. Split = extract the route handlers that don't read table-adjacent state. |
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
