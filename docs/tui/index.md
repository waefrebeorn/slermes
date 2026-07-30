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

The TUI communicates with the agent backend via 92 JSON-RPC methods registered in `tui_json_rpc.c`.

### Pet Methods

| Method | Handler | Description |
|--------|---------|-------------|
| `pet.info` | `rpc_pet_info` | Get active pet state |
| `pet.gallery` | `rpc_pet_gallery` | List installed pets |
| `pet.select` | `rpc_pet_select` | Select active pet |
| `pet.remove` | `rpc_pet_remove` | Remove installed pet |
| `pet.disable` | `rpc_pet_disable` | Disable pet display |
| `pet.scale` | `rpc_pet_scale` | Set display scale |
| `pet.cells` | `rpc_pet_cells` | Get frame cells |
| `pet.thumb` | `rpc_pet_thumb` | Get thumbnail |

### Session Methods

| Method | Description |
|--------|-------------|
| `session.list` | List saved sessions |
| `session.create` | Create new session |
| `session.activate` | Activate session |
| `session.close` | Close session |
| `session.delete` | Delete session |
| `session.history` | Get message history |
| `session.status` | Get session status |
| `session.save` | Save session |
| `session.resume` | Resume session |
| `session.interrupt` | Interrupt running |
| `session.undo` | Undo last response |
| `session.compress` | Compress context |
| `session.branch` | Branch session |
| `session.steer` | Inject steer message |
| `session.title` | Set session title |
| `session.usage` | Get token usage |
| `session.cwd_set` | Set working directory |
| `session.most_recent` | Get most recent |
| `session.active_list` | List active sessions |

### Agent/Config Methods

| Method | Description |
|--------|-------------|
| `agents_list` | List active subagents |
| `config_show` | Show configuration |
| `model_options` | List available models |
| `model_save_key` | Save API key |
| `model_disconnect` | Disconnect model |
| `tools_list` | List available tools |
| `tools_show` | Show tool details |
| `tools_configure` | Configure tool |
| `toolsets_list` | List toolsets |
| `plugins_list` | List plugins |
| `plugins_manage` | Manage plugins |
| `skills_manage` | Manage skills |
| `skills_reload` | Reload skills |

### Interaction Methods

| Method | Description |
|--------|-------------|
| `prompt_submit` | Submit a prompt |
| `prompt_background` | Background prompt |
| `cli_exec` | Execute CLI command |
| `command_dispatch` | Dispatch slash command |
| `command_resolve` | Resolve command |
| `shell_exec` | Execute shell command |
| `complete_slash` | Complete command |
| `complete_path` | Complete file path |
| `terminal_resize` | Terminal resize event |

### File/Image Methods

| Method | Description |
|--------|-------------|
| `file_attach` | Attach file |
| `image_attach` | Attach image |
| `image_attach_bytes` | Attach image bytes |
| `image_detach` | Detach image |
| `clipboard_paste` | Paste from clipboard |
| `paste_collapse` | Collapse paste |
| `pdf_attach` | Attach PDF |
| `input_detect_drop` | Detect drag-drop |

### Delegate/Spawn Methods

| Method | Description |
|--------|-------------|
| `delegation_pause` | Pause delegation |
| `delegation_status` | Delegation status |
| `spawn_tree_list` | List spawn tree |
| `spawn_tree_load` | Load spawn tree |
| `spawn_tree_save` | Save spawn tree |
| `subagent_interrupt` | Interrupt subagent |

### Billing/Credits

| Method | Description |
|--------|-------------|
| `billing_state` | Get billing state |
| `billing_charge` | Initiate charge |
| `billing_charge_status` | Charge status |
| `billing_auto_reload` | Toggle auto-reload |
| `billing_step_up` | Step up billing |
| `credits_view` | View credits |

### Other Methods

| Method | Description |
|--------|-------------|
| `ping` | Health check |
| `echo` | Echo params |
| `insights_get` | Get insights |
| `project_facts` | Project facts |
| `preview_restart` | Preview restart |
| `browser_manage` | Browser management |
| `cron_manage` | Cron management |
| `llm_oneshot` | One-shot LLM call |
| `rollback_list` | List snapshots |
| `rollback_diff` | Show snapshot diff |
| `rollback_restore` | Restore snapshot |
| `voice_record` | Record voice |
| `voice_toggle` | Toggle voice |
| `voice_tts` | Text-to-speech |
