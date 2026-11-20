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
