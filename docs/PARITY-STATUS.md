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
* **`browser_vault`** (`tools/browser_vault_tool.py:738`) reads the credential store (logins, cards,
  addresses as handles) and fills them on the live page. The store half is now real and proven:

  | layer | file | verified by |
  |---|---|---|
  | Fernet tokens | `src/agent/fernet.c` | spec vectors byte-for-byte + interop with `cryptography` (`fernet` 19) |
  | encrypted document | `src/agent/vault_file.c` | C reads the vault Python writes and vice versa (`vault-file` 20) |
  | local backend | `src/agent/vault_local_store.c` | vs `LocalLoginBackend` over the same files (`vault-local-backend` 16) |
  | `action=list` | `src/tools/browser_vault_list.c` | vs `browser_vault_list()` on a populated and an empty vault (`browser-vault-list` 8) |

  Still missing, and why the tool stays unregistered: `fill`, `save_login` and `enter_code` need the
  live page and its supervisor (`_ensure_supervisor`, `_fenced_page_op`), `unlock` needs
  `vault_backends/unlock.py` and a masked prompt owned by the surface, and the manager backends
  (1Password, Bitwarden) need their CLIs plus the session unlock store. Registering the four with a
  handler that can only ever answer "no browser session" is the stub this file exists to prevent; the
  `list` result alone is honest but it is one action out of five, so the name stays unclaimed until
  the session path exists.
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

## Live browser: what works, what does not

`browser_cdp` was dead and looked like a configuration problem. Two defects fixed:

* `lib/libwebsocket` used `read()`/`write()`/`close()` on a socket. On Windows a socket is not a CRT
  file descriptor: `connect` "succeeded" and the handshake never left the process. Every byte now goes
  through `ws_io_send`/`ws_io_recv`/`ws_io_close`.
* `cdp_send_command` returned the first frame that arrived, which is often an *event*
  (`executionContextCreated`, `loadEventFired`) rather than the response. It now matches the response
  id and discards events.

Verified live against headless Edge 158 (`--remote-debugging-port=9222`), one MCP server per check:

```
browser_cdp {cmd: Runtime.evaluate, params: {expression: "1+1", returnByValue: true}}
  -> {"id":1,"result":{"result":{"type":"number","value":2,"description":"2"}}}
```

The vault fill path was then driven end to end against a local page (`http://127.0.0.1:8899/`, a form
with an email and a password input): `vl_build_inspection_js` -> classify -> `select_password` ->
`vl_build_fill_js` -> `Runtime.evaluate`. In one evaluation it reported `{"filled":1}` and the
password input held the value. A wrong `expected_origin` correctly refused with
`{"refused":"origin_changed","found":"http://127.0.0.1:8899"}`.

**`browser_vault` is registered with two actions, both driven through a live page:**

```
action=list   -> items with handle/label/kind/origin/identifier; never a password
action=fill   -> success:true, item_id, origin, filled:2   (username + password; 3/3 separate calls)
                 shadow DOM: a custom element with its own <form> fills too (filled:2 there as well)
                 handle + wrong page_origin -> refused: origin_mismatch, naming both origins
                 unknown handle              -> {"error":"no vault item with id 'vault_nope'"}
                 password arg                -> one-off fill without touching the stored secret
                 action=unlock               -> {"error":"... save_login, enter_code and unlock are not implemented"}
```

The join needed one thing no layer could do alone -- reaching the page -- so `browser.c` now exposes
`browser_cdp_evaluate()`, and the tool uses that client rather than the second CDP implementation in
`browser_cdp_tool.c` (which still reads a single frame per call).

**Resolved.** The "some calls land elsewhere" artifact was not CDP at all. Two defects stacked:
`ws_io_send` called `send()` once per header and once per payload, so a 2 KB script could go out
truncated and be silently dropped by the browser; and `src/tools/registry.c` kept a process-wide
idempotency cache keyed on (tool name, raw args), so a repeated call with identical arguments —
read this selector again, click it twice — was answered from the first call's result without ever
reaching the tool. Reads froze at their first value and writes after the first did nothing.

Both fixed. Identical repeated reads now return fresh values, and the fill chain is stable across
separate calls:

```
fill 6/6 -> {"filled":1} with the value in the password input
wrong expected_origin -> {"refused":"origin_changed","found":"http://127.0.0.1:8899"}
```

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
