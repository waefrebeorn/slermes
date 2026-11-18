<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Gateway Commands

7 commands in the `Gateway` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). Handlers: `src/cli/cli_cmd_gateway.c` and
`src/hermes_cli/write_approval_commands.c`.

Commands flagged `gateway_only` are skipped by `commands_dispatch()` in CLI mode — they
are meant for the gateway/messaging surface. Commands flagged `cli_only` are the reverse.

## `/gateway`

Manage the gateway: `status`, `list`, `stop`, `setup`, `restart`.

```
/gateway [status|list|stop|setup|restart]
```

## `/platforms`

Shows configured gateway/messaging platforms and (with `-v`/`verbose`) credential status.
Gateway-only.

```
/platforms [-v|verbose|list]
```

## `/webhook`

Manage webhook subscriptions. Gateway-only.

```
/webhook list
/webhook add <url> [secret]
/webhook remove <id>
```

## `/restart`

Saves the current session, then re-execs the binary. The handler resolves the executable
via `/proc/self/exe` and reads original arguments from `/proc` — a POSIX-only path that
does not work on native Windows.

```
/restart
```

## `/sethome`

Sets the home channel. Gateway-only. With no argument, prints the current home channel.

```
/sethome <platform:chat_id>
```

## `/handoff`

Session handoff to a messaging platform. CLI-only. Subcommands verified in the handler:
`request <platform>`, `claim <id>`, `complete <id>`, `status`, `list`.

```
/handoff [request|claim|complete|status|list]
```

## `/platform` (`/pf`)

Gateway platform control. The registry description says "pause, resume, or list", but the
current handler (`cmd_platform` in `src/cli/cli_cmd_gateway.c`) implements the overview
(no argument) and `list` only — pause/resume are not wired in this handler.

```
/platform [list]
```
