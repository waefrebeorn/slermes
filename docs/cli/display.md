<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Display Commands

7 commands in the `Display` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). All are CLI-only. Handlers:
`src/hermes_cli/write_approval_commands.c` (`/redraw`, `/verbose`, `/skin`,
`/statusbar`, `/busy`, `/indicator`) and `src/hermes_cli/cli_cmd_display.c` (`/pet`).
`/footer` is a **Config**-category command — see [config.md](config.md).

## `/redraw`

Forces a repaint. The CLI handler emits ANSI clear-screen + cursor-home and prints a
confirmation; the ncurses TUI has its own redraw path (`Ctrl+L`).

```
/redraw
```

## `/verbose`

Cycles tool-progress verbosity through three levels: `off` → `normal` → `verbose`
(not a binary toggle).

```
/verbose
```

## `/skin`

Shows or changes the display skin/theme. `/skin list` lists built-in skins;
`/skin <name>` activates one.

```
/skin [list|<name>]
```

## `/statusbar`

Shows or hides the context/model status bar: `on`, `off`, `status`.

```
/statusbar [on|off|status]
```

## `/busy`

Controls what Enter does while the agent is working: `queue`, `steer`, `interrupt`,
`status`.

```
/busy [queue|steer|interrupt|status]
```

## `/indicator`

Picks the busy-indicator style. Verified options: `default`, `dots`, `bar`, `face`.
With no argument it prints the current style.

```
/indicator [default|dots|bar|face]
```

## `/pet`

Petdex mascot management: `info`, `gallery`, `select <slug>`, `remove <slug>`,
`disable`, `scale <n>` (plus `enable` and `hatch` forms accepted by the handler).
Full documentation: [Pet System](../pet/index.md).

```
/pet [info|gallery|select <slug>|remove <slug>|disable|scale <n>]
```
