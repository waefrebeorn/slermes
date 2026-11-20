<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T17:36:51Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Config Commands

12 commands in the `Config` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`), plus 3 parity commands registered under
`Configuration` (listed at the bottom). Handlers are mainly in
`src/cli/cli_cmd_config.c` and `src/hermes_cli/cli_cmd_parity.c`. Behavior notes below
were checked against the handler bodies; parity vs Python is proven by the oracle suites.

## `/model` (`/m`)

Model management. Handler accepts `list` (alias `ls`), `show`, `info`, `providers`
(alias `prov`), and `set <model>`.

```
/model [list|show|info|providers|set <model>]
```

## `/config` (`/cfg`)

Show or edit configuration. CLI-only. Beyond plain `/config [key] [val]`, the handler
implements `show`, `get <key>`, `set <key> <val>`, `diff`, `export`, `groups`, `migrate`,
`profile`, `schema`, and `validate` forms.

```
/config [key] [val]
/config set display.pet.scale 0.5
```

## `/setup`

Setup wizard (provider, model, API key). CLI-only. Flags verified in `cmd_setup`:
`--quick`/`-q`, `--non-interactive`/`-n`, `--reset`/`-r`, `--reconfigure`, `--portal`/`-p`.
Sections: `model`, `tts`, `terminal`, `gateway`, `tools`, `agent`.

```
/setup [--quick|--non-interactive|--reset|--reconfigure|--portal|section]
```

## `/uninstall`

Removes installed binaries from the POSIX install paths it knows about
(`/usr/local/bin/slermes`, `/usr/bin/slermes`, `~/.local/bin/slermes`). CLI-only.
It does **not** auto-delete config or `.env`: it reports whether they exist and prints
manual removal instructions. On Windows the POSIX binary paths do not apply.

```
/uninstall
```

## `/backup`

Copies `config.yaml` and `.env` into `<home>/backups/slermes-backup-<timestamp>/`.
CLI-only. `full` additionally backs up sessions and plugin files.

```
/backup [config|full]
```

## `/topic` (`/t`)

Sets or replaces the system message with the given text.

```
/topic <text>
```

## `/reasoning` (`/re`)

Manages reasoning effort and display. Verified levels: `none`, `minimal`, `low`,
`medium`, `high`, `xhigh`; `on` maps to `medium`, `off` maps to `none`; `show` prints the
current effort; `hide` hides reasoning display.

```
/reasoning [none|minimal|low|medium|high|xhigh|on|off|show|hide]
```

## `/fast`

Toggles the fast-mode flag (priority processing). Accepts `normal`/`off`, `fast`/`on`,
and `status`; with no argument it prints the current mode. It is not a verbosity control
(older docs described it as one).

```
/fast [normal|fast|status]
```

## `/voice`

Voice input/output mode. Handler accepts `on`, `off`, `status` (alias `st`), `config`,
`tts`, and `key <...>` forms.

```
/voice [on|off|status|config|tts|key]
```

## `/yolo`

Toggles YOLO mode: when enabled, dangerous-command approvals are skipped.

```
/yolo
```

## `/personality` (`/p`)

Sets the system message (personality). The handler takes free text and installs it as
the system message — it does not look up predefined personality names.

```
/personality <system prompt text>
```

## `/footer`

Toggles the gateway metadata footer on replies: `on`, `off`, `status`.

```
/footer [on|off|status]
```

## Related parity commands (`Configuration` category)

These are registered under `Configuration` (not `Config`) and implemented in
`src/hermes_cli/cli_cmd_parity.c`:

- `/battery [on|off|status]` — color-coded battery indicator in the status bar; reads
  real battery state via `battery_read()`.
- `/codex-runtime` (`/codex_runtime`) `[auto|codex_app_server|status]` — toggles the codex
  app-server runtime for OpenAI/Codex models (persisted as `codex.runtime`).
- `/timestamps` (`/ts`) `[on|off|status]` — toggles `[HH:MM]` timestamps on messages and
  `/history`; delegates to `ccm_handle_timestamps_command`.
