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

# Security Commands

5 commands in the `Security` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). Handlers: `src/cli/cli_cmd_security.c`
(`/secrets`, `/auth`, `/key`) and `src/hermes_cli/write_approval_commands.c`
(`/approve`, `/deny`).

## `/approve`

Approves a pending dangerous command. Gateway-only (skipped by CLI dispatch). The
handler also supports `list` (show the approval cache) and `clear` (clear cached
approvals).

```
/approve [list|clear]
```

## `/deny`

Denies pending dangerous commands: resets the session approval state and clears the
approval cache. Gateway-only.

```
/deny
```

## `/secrets`

Manage secrets: `list`, `get`, `sync`, `status`.

```
/secrets [list|get|sync|status]
```

## `/auth`

Provider authentication status. Registry subcommands: `status`, `providers`. The handler
also implements `login`, `refresh`, `tokens`, and `validate` forms (provider-specific
flows live in the `port_auth_*` modules). There is no `logout` subcommand in this C
handler.

```
/auth [status|providers|login|refresh|tokens|validate]
```

## `/key`

Manage API keys: `list`, `set <provider>`, `show <provider>`, `unset <provider>`.

```
/key [list|set <provider>|show <provider>|unset <provider>]
```

**Examples:**
```
/key list
/key set openai
/key show openai
/key unset openai
```
