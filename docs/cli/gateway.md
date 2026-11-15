<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
