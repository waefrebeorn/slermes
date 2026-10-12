<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# MCP Commands

## `/mcp`

Manage MCP (Model Context Protocol) servers: status, list, test.

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

Reload MCP server configurations from config.yaml. Applies changes without restarting.

```
/reload-mcp
```
