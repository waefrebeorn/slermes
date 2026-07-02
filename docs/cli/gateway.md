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
