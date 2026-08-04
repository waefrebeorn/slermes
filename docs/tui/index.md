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

The TUI communicates with the agent backend via JSON-RPC methods. See the [Gateway Commands](cli/gateway.md) reference for the full method list.

### Pet Methods

The `/pet` command provides interactive petdex management:
- `/pet info` — Show active pet status
- `/pet gallery` — List installed pets
- `/pet select <name>` — Switch active pet
- `/pet remove <name>` — Remove a pet
- `/pet disable` / `/pet enable` — Toggle pet display
- `/pet scale <factor>` — Adjust pet size