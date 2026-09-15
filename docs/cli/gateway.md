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

# Gateway Commands

## `/gateway`

Manage the gateway: status, list active platforms, stop, setup, restart.

```
/gateway [status|list|stop|setup|restart]
```

**Examples:**
```
/gateway status
/gateway list
/gateway restart
```

## `/platforms`

Show the status of all gateway/messaging platforms. Gateway-only.

```
/platforms
```

Shows connected platforms, connection status, and message counts.

## `/webhook`

Manage webhook subscriptions. Gateway-only.

```
/webhook [list|add|remove]
```

## `/restart`

Gracefully restart the gateway without losing active sessions.

```
/restart
```

## `/sethome`

Set the current chat channel as the home/default channel. Gateway-only.

```
/sethome
```

## `/handoff`

Hand off the current session to a messaging platform. CLI-only.

```
/handoff
```

## `/platform` (`/pf`)

Pause, resume, or list gateway platforms.

```
/platform
```
