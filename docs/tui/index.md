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

# Terminal UI (TUI) Reference

The TUI provides a full-screen terminal interface with split-pane layout, session management, and real-time pet animations.

## Layout

```
┌──────────────────────────────────────────────────┐
│ [Header]  Model: gpt-4  Status: Ready            │
├───────────────────────┬──────────────────────────┤
│                       │                          │
│    Chat Panel         │    Tool Output           │
│    (messages)         │    (scrollable)          │
│                       │                          │
│                       │                          │
├───────────────────────┴──────────────────────────┤
│ > /prompt input area                    [pet]    │
│ [Status Bar]  Tokens: 1,234  Mode: Chat         │
└──────────────────────────────────────────────────┘
```

## Panels

### Header Bar
Shows current model, provider, status, and session info.

### Chat Panel
Main conversation display. Shows user messages and assistant responses with proper formatting.

### Tool Output Panel
Right-side panel showing tool execution results, file contents, and command output.

### Input Area
Bottom prompt input with command completion, history navigation, and multi-line editing.

### Status Bar
Shows token usage, context size, current mode, and pet animation.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+C` | Interrupt current operation |
| `Ctrl+D` | Exit (if input empty) |
| `Tab` | Command/file completion |
| `Up/Down` | Input history navigation |
| `PageUp/PageDown` | Scroll chat panel |
| `Ctrl+L` | Redraw screen |
| `Ctrl+W` | Delete word backward |
| `Ctrl+U` | Clear input line |
| `Enter` | Submit prompt |
| `Shift+Enter` | New line in input |
| `Esc` | Cancel/close panel |

## JSON-RPC Methods

The TUI communicates with the agent backend via JSON-RPC methods. See the [Gateway Commands](../cli/gateway.md) reference for the full method list.

### Pet Methods

The `/pet` command provides interactive petdex management:
- `/pet info` — Show active pet status
- `/pet gallery` — List installed pets
- `/pet select <name>` — Switch active pet
- `/pet remove <name>` — Remove a pet
- `/pet disable` / `/pet enable` — Toggle pet display
- `/pet scale <factor>` — Adjust pet size