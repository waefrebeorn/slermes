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

# Security Commands

## `/approve`

Approve a pending dangerous command execution. Gateway-only.

```
/approve
```

## `/deny`

Deny a pending dangerous command execution. Gateway-only.

```
/deny
```

## `/secrets`

Manage secrets: list, get, sync, and status.

```
/secrets [list|get|sync|status]
```

## `/auth`

Show provider authentication status.

```
/auth [status|providers]
```

## `/key`

Manage API keys for different providers.

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
