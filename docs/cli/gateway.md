<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
