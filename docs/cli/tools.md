<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Tools Commands

7 commands in the `Tools` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). Handlers:
`src/hermes_cli/write_approval_commands.c` (`/tools`, `/tools-verify`, `/commands`,
`/toolsets`, `/paste`, `/browser`), `src/hermes_cli/cli_cmd_tools.c` (`/image`).

## `/tools`

Lists registered tools and their status; `/tools <name>` shows details for one tool.
CLI-only.

```
/tools [name]
```

## `/tools-verify`

Checks a fixed list of expected tool names against the live tool registry and reports
missing ones. CLI-only.

```
/tools-verify
```

## `/commands` (`/cmds`)

Lists all registered slash commands from `COMMANDS[]`, paginated (20 per page; pass a
page number). CLI-only.

```
/commands [page]
```

## `/toolsets`

Lists available toolsets, derived dynamically from the tool registry. CLI-only.

```
/toolsets
```

## `/image`

Intended to attach a local image for the next prompt. **Honest status:** the current
handler (`cmd_image` in `src/hermes_cli/cli_cmd_tools.c`) only prints an acknowledgement
of the path — it does not wire the image into the next prompt. Image attachment that
actually reaches the model happens through other surfaces (e.g. the TUI JSON-RPC
`image_attach` methods).

```
/image <path>
```

## `/paste`

Clipboard paste. CLI-only. The handler reads the clipboard via
`powershell.exe -NoProfile -Command "Get-Clipboard"` (the WSL path) and falls back to a
message suggesting `/image <path>` when unavailable. It reads text clipboard content;
it is not a general clipboard-image decoder.

```
/paste
```

## `/browser`

Connects browser tools to Chromium via CDP. CLI-only. Verified subcommands:
`connect <ws_url>`, `status`, `disconnect`.

```
/browser [connect <ws_url>|status|disconnect]
```

## Related commands in other categories

- `/deps` (System category) — installs Python bridge dependencies by running
  `pip install -r requirements-bridge.txt` (falls back to `pip3`). CLI-only.
- `/skills` (Skills category) — see [skills.md](skills.md).
- `/blueprint`, `/hatch`, `/learn`, `/suggestions` (Tools & Skills category) — parity
  handlers in `src/hermes_cli/cli_cmd_parity.c` delegating to the faithful
  `ccm_handle_*` wrappers; see the [command index](index.md).
