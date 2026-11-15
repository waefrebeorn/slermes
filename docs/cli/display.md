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
