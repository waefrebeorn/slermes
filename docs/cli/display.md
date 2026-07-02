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
