<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Display Commands

## `/redraw`

Force a full UI repaint. Useful after terminal resize or display corruption. CLI-only.

```
/redraw
```

## `/verbose`

Toggle tool progress display verbosity. CLI-only.

```
/verbose
```

## `/skin`

Show current skin/theme or change to a different one. CLI-only.

```
/skin <name>
```

## `/statusbar`

Toggle the context/model status bar at the bottom of the TUI. CLI-only.

```
/statusbar
```

## `/busy`

Control what Enter does while the agent is working. CLI-only.

```
/busy [queue|steer|interrupt|status]
```

Modes:
- `queue` — Queue prompts during busy state
- `steer` — Enable mid-turn steering
- `interrupt` — Allow interrupting the agent
- `status` — Show current busy mode

## `/indicator`

Pick the TUI busy-indicator animation style. CLI-only.

```
/indicator
```

## `/footer`

Toggle gateway metadata footer on replies.

```
/footer [on|off|status]
```
