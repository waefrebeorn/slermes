<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# CLI Commands — Reference

Slermes has **82 slash commands**, all implemented as real C11 handlers in `src/cli/port_cli_command_registry.c`. Zero stubs.

## Quick Reference Table

| Command | Alias | Category | Description |
|---------|-------|----------|-------------|
| `/new` | `/n` | Session | Start a new conversation |
| `/clear` | `/c` | Session | Clear conversation context |
| `/undo` | `/u` | Session | Remove last assistant response |
| `/save` | `/s` | Session | Save current session |
| `/load` | — | Session | Load session |
| `/sessions` | — | Session | List saved sessions |
| `/stats` | — | Session | Show session statistics |
| `/recap` | — | Session | Summarize activity |
| `/conv` | — | Session | Show recent messages |
| `/history` | — | Session | Show full history |
| `/reset` | `/r` | Session | Clear and start fresh |
| `/retry` | — | Session | Retry last LLM call |
| `/compress` | `/cctx` | Session | Compress context |
| `/branch` | — | Session | Branch session |
| `/snapshot` | `/snap` | Session | Save named snapshot |
| `/status` | `/st` | Session | Show config info |
| `/resume` | — | Session | Resume session |
| `/rollback` | — | Session | List/restore snapshots |
| `/model` | `/m` | Config | Model management |
| `/config` | `/cfg` | Config | Show/edit configuration |
| `/setup` | — | Config | Setup wizard |
| `/uninstall` | — | Config | Uninstall Slermes |
| `/backup` | — | Config | Backup config/sessions |
| `/topic` | `/t` | Config | Set personality |
| `/reasoning` | `/re` | Config | Reasoning management |
| `/fast` | — | Config | Toggle fast mode |
| `/voice` | — | Config | Toggle voice |
| `/yolo` | — | Config | Toggle YOLO mode |
| `/personality` | `/p` | Config | Set personality |
| `/tools` | — | Tools | List tools |
| `/tools-verify` | — | Tools | Verify tool registration |
| `/commands` | `/cmds` | Tools | List commands |
| `/toolsets` | — | Tools | List toolsets |
| `/image` | — | Tools | Attach image |
| `/paste` | — | Tools | Attach clipboard image |
| `/browser` | — | Tools | Connect browser |
| `/deps` | — | Tools | Install bridge deps |
| `/skills` | — | Skills | Manage skills |
| `/skills-hub` | `/hub` | Skills | Skills hub |
| `/bundles` | — | Skills | List bundles |
| `/curator` | — | Skills | Curator status |
| `/reload-skills` | — | Skills | Re-scan skills |
| `/help` | `/h` | Help | Show help |
| `/exit` | `/quit` | System | Exit program |
| `/stop` | — | System | Kill background processes |
| `/doctor` | — | System | Diagnostics |
| `/completions` | — | System | Shell completions |
| `/reload` | — | System | Reload .env |
| `/copy` | — | System | Copy to clipboard |
| `/update` | — | System | Update |
| `/debug` | — | System | Debug report |
| `/logs` | — | System | View logs |
| `/dump` | — | System | Dump debug info |
| `/send` | — | System | Send message |
| `/approve` | — | Security | Approve command |
| `/deny` | — | Security | Deny command |
| `/secrets` | — | Security | Manage secrets |
| `/auth` | — | Security | Auth status |
| `/key` | — | Security | API keys |
| `/gateway` | — | Gateway | Gateway management |
| `/platforms` | — | Gateway | Platform status |
| `/webhook` | — | Gateway | Webhook management |
| `/restart` | — | Gateway | Restart gateway |
| `/sethome` | — | Gateway | Set home channel |
| `/handoff` | — | Gateway | Hand off session |
| `/platform` | `/pf` | Gateway | Platform control |
| `/redraw` | — | Display | Force repaint |
| `/verbose` | — | Display | Toggle verbosity |
| `/skin` | — | Display | Change theme |
| `/statusbar` | — | Display | Toggle status bar |
| `/busy` | — | Display | Busy behavior |
| `/indicator` | — | Display | Indicator style |
| `/footer` | — | Config | Toggle footer |
| `/mcp` | — | MCP | MCP management |
| `/reload-mcp` | — | MCP | Reload MCP config |
| `/plugins` | — | Plugins | Plugin management |
| `/dashboard` | — | System | Web dashboard |
| `/cron` | — | Cron | Scheduled tasks |
| `/memory` | — | Memory | Memory setup |
| `/kanban` | — | Kanban | Kanban board |
| `/profile` | — | Session | Show profile |
| `/whoami` | — | Session | Show access level |
| `/pet` | — | Display | Petdex mascot |
| `/session-search` | — | Session | Search sessions |
| `/session-export` | — | Session | Export session |
| `/session-import` | — | Session | Import session |
| `/background` | `/bg` | Session | Background prompt |
| `/steer` | — | Session | Inject message |
| `/queue` | — | Session | Queue prompt |
| `/goal` | — | Session | Set goal |
| `/subgoal` | — | Session | Add subgoal |
| `/agents` | — | Session | Show subagents |
| `/usage` | — | Session | Token usage |
| `/insights` | — | Session | Usage insights |
| `/title` | — | Session | Set session title |

## Per-Category Documentation

- [Session Commands](session.md) — `/new`, `/clear`, `/undo`, `/save`, etc.
- [Config Commands](config.md) — `/model`, `/config`, `/setup`, etc.
- [Tools Commands](tools.md) — `/tools`, `/image`, `/browser`, etc.
- [Security Commands](security.md) — `/approve`, `/deny`, `/secrets`, etc.
- [Gateway Commands](gateway.md) — `/gateway`, `/platforms`, `/webhook`, etc.
- [Display Commands](display.md) — `/redraw`, `/skin`, `/verbose`, etc.
- [Skills Commands](skills.md) — `/skills`, `/skills-hub`, `/bundles`, etc.
- [MCP Commands](mcp.md) — `/mcp`, `/reload-mcp`
- [Pet System](../pet/index.md) — `/pet` mascot
