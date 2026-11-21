<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# MCP Commands

2 commands in the `MCP` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). Both handlers live in
`src/hermes_cli/cli_cmd_mcp.c`.

## `/mcp`

MCP (Model Context Protocol) server management. Registry subcommands: `status`, `list`,
`test <name>`. The handler additionally accepts `add`, `remove` (alias `rm`), and
`reload` forms. With no argument it behaves like `status`/`list` and shows servers and
their tools.

```
/mcp [status|list|test <name>]
```

**Examples:**
```
/mcp status
/mcp list
/mcp test filesystem
```

## `/reload-mcp`

**Print-only in the current build.** The handler reports how many MCP servers are loaded
and instructs you to edit `mcp_servers` in `~/.slermes/config.yaml` and restart — it does
not hot-reload configuration. Older docs claimed "applies changes without restarting";
that is not what the code does.

```
/reload-mcp
```
