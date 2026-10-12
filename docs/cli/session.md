<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Session Commands

18 commands for managing conversations, sessions, and context.

## `/new` (`/n`)

Start a brand new conversation session. Clears all messages and resets state.

```
/new
```

## `/clear` (`/c`)

Clear conversation context while keeping the session alive. CLI-only.

```
/clear
```

## `/undo` (`/u`)

Remove the last assistant response. Useful when the model went off-track.

```
/undo
```

## `/save` (`/s`)

Save the current session to persistent storage. CLI-only.

```
/save
```

## `/load`

Load a previously saved session by ID.

```
/load <session_id>
```

**Example:**
```
/load abc123
```

## `/sessions`

List all saved sessions with timestamps.

```
/sessions
```

## `/stats`

Show statistics for the current session — message count, token usage, model info.

```
/stats
```

## `/recap`

Summarize the current conversation.

```
/recap
```

## `/conv`

Show recent messages in the current session.

```
/conv
```

## `/history`

Show full conversation history.

```
/history
```

## `/reset` (`/r`)

Clear and start fresh — same as `/new` but also resets config.

```
/reset
```

## `/retry`

Retry the last LLM call with the same prompt.

```
/retry
```

## `/compress` (`/cctx`)

Compress the conversation context to save tokens.

```
/compress
```

## `/branch`

Create a branch of the current session.

```
/branch <name>
```

## `/snapshot` (`/snap`)

Save a named snapshot of the current session state.

```
/snapshot <name>
```

## `/status` (`/st`)

Show configuration and status information.

```
/status
```

## `/resume`

Resume a previously paused session.

```
/resume
```

## `/rollback`

List and restore previous session snapshots.

```
/rollback
```