<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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