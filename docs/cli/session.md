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

Generate a summary of recent session activity.

```
/recap
```

## `/conv`

Display recent conversation messages for quick reference.

```
/conv
```

## `/history`

Show the full conversation history. CLI-only.

```
/history
```

## `/reset` (`/r`)

Reset session: clear all messages and start fresh.

```
/reset
```

## `/retry`

Retry the last LLM call. Re-sends the conversation with the same messages.

```
/retry
```

## `/compress` (`/cctx`)

Compress conversation context to reduce token usage.

```
/compress [keep_count]
```

**Example:** Keep 5 most recent messages:
```
/compress 5
```

## `/branch`

Branch from current session at a specified message index.

```
/branch [message_index]
```

## `/snapshot` (`/snap`)

Save a named snapshot of current session state. CLI-only.

```
/snapshot [name]
```

## `/status` (`/st`)

Show session status: model, provider, context usage, settings.

```
/status
```

## `/resume`

Resume a previously-named session by ID.

```
/resume <id>
```

## `/rollback`

List or restore state snapshots.

```
/rollback
```

## `/session-search`

Search past sessions by query text.

```
/session-search <query> [--limit N]
```

**Example:**
```
/session-search docker networking --limit 5
```

## `/session-export`

Export a session to JSON or markdown format.

```
/session-export <session_id> [json|markdown]
```

## `/session-import`

Import a session from a JSON file.

```
/session-import <filepath>
```

## `/background` (`/bg`)

Run a prompt in the background while continuing the current session.

```
/background <prompt>
```

## `/steer`

Inject a message after the next tool call (mid-turn steering).

```
/steer <text>
```

## `/queue`

Queue a prompt to be sent on the next turn.

```
/queue <prompt>
```

## `/goal`

Set a standing goal that persists across turns.

```
/goal <text>
```

## `/subgoal`

Add extra criteria to the active goal.

```
/subgoal <text>
```

## `/agents`

Show active subagents and running tasks from the delegate tool.

```
/agents
```

## `/usage`

Show token usage and session statistics.

```
/usage
```

## `/insights`

Show usage insights and analytics.

```
/insights [--days N]
```

## `/title`

Set a title for the current session.

```
/title <title>
```

## `/profile`

Show active profile name and home directory. CLI-only.

```
/profile
```

## `/whoami`

Show your slash command access level. CLI-only.

```
/whoami
```
