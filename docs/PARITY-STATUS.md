# Tool parity: where it stands and what each remaining name actually needs

Measured with `scripts/check_tool_parity.py` against `toolsets.BUILTIN_TOOL_NAMES` in the Python
quarry, read back from `slermes tools` on the built binary. Numbers here are from the run noted at
the bottom; re-run the script rather than trusting the text.

## Done and verified over JSON-RPC

Verified means the tool answered through `mcp-serve` on :9100 with the payloads shown in the commit
that introduced it — not that the name appears in the registry.

| tool | behaviour proven |
| --- | --- |
| `kanban_request_changes` | needs a non-blank reason; drives `kanban_reopen_review_task`; event appended |
| `desktop_project` | create / switch by slug, name or id / list against the real `projects.db` |
| `manage_catalog` | validation order (unknown key, action, kind, items) then upstream's desktop-only text |
| `xai_video_edit`, `xai_video_extend` | prompt, video_url, provider gate, auth gate — in upstream's order |
| `setup_beat` | done / skip / value events end to end on a real state.db, record persisted, reminder rendered; cards answer the desktop-only line |

`xai_video_*` submit/poll is implemented (real endpoints, bounded poll, terminal-status detection)
but its success path has never executed: no xAI credentials on this machine. Treat as unproven.

## Onboarding: ported in three verified pieces

`session_rules/onboarding_flow.py:apply` -> `src/agent/onboarding_flow_apply.c`
`agent/onboarding_text.py` reminders -> `src/agent/onboarding_reminders.c`
`session_rules/onboarding_first_tasks.py` -> `src/agent/onboarding_task_catalog_data.c` (generated)

Each has a differential oracle against the quarry (`tests/oracle/onboarding_*_oracle.py`), and
`tests/unit/test_setup_beat.c` drives the tool on a real state.db. Not ported: `build_guidance` (the
`build_*` reminder keys), which needs each task's prompt fields and a skill-name resolver.

## Protocol defects found while verifying (fixed)

* `send_http_response()` formatted into `char[65536]`; `tools/list` is 89 KiB with 140 tools, so the
  body was truncated mid-string and no client could parse it.
* `tools/list` emitted `inputSchema` as a JSON **string**; MCP requires an object. Fixing that
  exposed five broken schema literals (`file_watch`, `send_message`, `session_crud`,
  `image_generate`, `discord_admin`), all repaired.
* `scripts/check_tool_schemas.py` now runs inside `scripts/flow_mcp.sh`, so a future typo fails the
  gate instead of reaching a client.
* `registry_dispatch` sanitised whole error results: `{"error": ...}` came back as
  `[TOOL_ERROR] {...}`, unparseable, and the MCP server answered -32603 for every such tool. The
  message field is sanitised now, the envelope survives.
* `hermes_state_db_open` returned NULL on any database an older build had made — the schema SQL
  indexes columns the ALTER TABLE migrations add later (`sessions.parent_session_id`), and the first
  exec failing aborted the open. Migrations now run after a failed pass and the schema is retried.

## Remaining three — each needs a subsystem, not a handler

`browser_exec`, `browser_vault`, `manage_connections`

* **`browser_exec`** (`tools/browser_use_cli.py:1047`) runs arbitrary `code` in the browser-use
  Python engine. There is no Python engine in this binary; `browser_cdp` already covers evaluate.
  A faithful port means either embedding the engine or redefining the tool — both are decisions, not
  ports. Do not ship a handler that pretends to execute code.
* **`browser_vault`** (`tools/browser_vault_tool.py:738`) reads the *browser profile* credential
  store (logins, cards, addresses as handles) and fills them on the live page. The C tree has real
  pieces: `src/agent/vault_backends/local.c` implements a local backend over a records array
  (`w22_local_259750_list_items`, `get_meta`, `resolve_password`, `resolve_otp`), and
  `w15_browservau_3f8d5c_saved_origins`, `w11_browservau_c65b17_list(backends, can_prompt)`. What is
  missing is the registry that loads those records — `src/agent/vault_backends/__init__.c`,
  `base.c`, `browser.c`, `unlock.c` are `port_pending` stubs, so nothing enumerates enabled backends
  or opens the vault file. `include/hermes_vault.h` is a different store (service/key/value with a
  master key), not browser logins.

  Consequence: `browser_vault`'s `list` action is reachable once that registry exists; `fill`,
  `save_login`, `enter_code` and `unlock` additionally need a live page and the backend unlock path.
* **`manage_connections`** (`tools/connectors/tool.py`). Correction to an earlier note in this file:
  the C tree does have a substantial connectors port — `src/tools/connectors/` is ~3.8k lines, and
  `w19_tool_6f4666_manage_connections(args)` already performs upstream's validation and target
  normalization, returning the request (`{"kind":"managed"|"mcp","action","targets"}`) or the same
  error strings. What is missing is *execution*: every action — `status`, `display`, `connect`,
  `rename`, the MCP ops — goes through the hosted portal client (`PortalConnectorClient`,
  `managed_client(creates_guest=True)` in `tools/connectors/managed.py`), which is live network I/O
  against the Hermes backend and has no C counterpart. So the tool is not registered: a handler that
  could only ever answer "Connectors are not available in this session." is a name without behaviour,
  and this file exists to avoid counting those.

## Standing gates

```
build:            LINK_RC=0, no exclusions
port object gate: PASS (see tests/verify_build_health.py --no-link)
unit:             PASS
flow-mcp:         PASS (initialize, tools/list, tools/call, tool schemas)
oracles:          w28_adapter 70/0 · w28_files 85/0 · w32_methodscon 70/0 · w25_reporter 35/0
```

## Traps worth remembering

* `scripts/check_tool_parity.py` reporting **0 tools** means `slermes.exe` could not start, not that
  the registry is empty: a shell that sourced `build/winlibs-env.sh` has no MSYS2 runtime on PATH, so
  the binary exits 127 with no output. Run it with `PATH=/c/msys64/mingw64/bin:$PATH` first.
* A leftover `slermes.exe` holds the output file: the link dies with `cannot open output file
  slermes.exe: Permission denied`. Kill by PID, and pass `-FI` to `tasklist` — inside a nested
  `bash -lc` MSYS rewrites `/FI` into `C:/msys64/FI` and the kill loop silently does nothing.
* A second `mcp-serve` cannot bind :9100; curls then hit the *old* binary and appear to show the bug
  you just fixed. Verify zero processes, rebuild, start exactly one server.
