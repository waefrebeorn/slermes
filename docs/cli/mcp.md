<!-- PARITY:AUTO -->
| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | 313 PASS / 22,604 (1.4%) · 0/6230 ported match · 11511/11511 gap mismatch |
**Phase (v672):** PORT phase

**Upstream sync:** 2,072 ahead / 2,639 behind (last merge 2026-09-15 (upstream fetched))

_Generated 2026-09-15T06:22:36Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

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
