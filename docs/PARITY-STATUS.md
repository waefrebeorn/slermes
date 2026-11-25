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

`xai_video_*` submit/poll is implemented (real endpoints, bounded poll, terminal-status detection)
but its success path has never executed: no xAI credentials on this machine. Treat as unproven.

## Protocol defects found while verifying (fixed)

* `send_http_response()` formatted into `char[65536]`; `tools/list` is 89 KiB with 140 tools, so the
  body was truncated mid-string and no client could parse it.
* `tools/list` emitted `inputSchema` as a JSON **string**; MCP requires an object. Fixing that
  exposed five broken schema literals (`file_watch`, `send_message`, `session_crud`,
  `image_generate`, `discord_admin`), all repaired.
* `scripts/check_tool_schemas.py` now runs inside `scripts/flow_mcp.sh`, so a future typo fails the
  gate instead of reaching a client.

## Remaining four — each needs a subsystem, not a handler

`browser_exec`, `browser_vault`, `manage_connections`, `setup_beat`

* **`browser_exec`** (`tools/browser_use_cli.py:1047`) runs arbitrary `code` in the browser-use
  Python engine. There is no Python engine in this binary; `browser_cdp` already covers evaluate.
  A faithful port means either embedding the engine or redefining the tool — both are decisions, not
  ports. Do not ship a handler that pretends to execute code.
* **`browser_vault`** (`tools/browser_vault_tool.py:738`) reads the *browser profile* credential
  store (logins, cards, addresses as handles) and fills them on the live page. The C tree has
  `src/agent/browser_provider.c` and the vault *helper* `w15_browservau_3f8d5c_saved_origins(meta)`,
  but no store behind it. `include/hermes_vault.h` is a different thing — a service/key/value store
  with a master key, not browser logins.
* **`manage_connections`** (`tools/connectors/tool.py`) manages hosted connectors: client state,
  OAuth, MCP install. `grep -rl connectors src/` finds no store in C at all.
* **`setup_beat`** (`tools/setup_beat_tool.py`) needs the onboarding *run record* store plus the
  desktop card roundtrip. The C tree has `agent/onboarding_record.c`, `agent/onboarding_text.c` and
  the flow port in `include/noc_w17_onboarding_4983bc.h` (next_beat, is_active, valid_answer,
  build_values, constants) — but **no event application**: `session_rules/onboarding_flow.py` is
  429 lines with a `Beat` table and the reduce that turns `Answered`/`Skipped`/`Done` into the next
  record. Port that first, then the tool.

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
