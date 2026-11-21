<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Session Commands

38 commands in the `Session` category of the live registry (`COMMANDS[]` in
`src/hermes_cli/partial_compress.c`). Handlers are mainly in `src/cli/cli_cmd_session.c`
and `src/hermes_cli/cli_cmd_parity.c`. Descriptions below were checked against the handler
bodies; behavioral parity vs Python is proven by the oracle suites, not by this page.

## `/new` (`/n`)

Saves the previous session first (if a DB is open and messages exist), then clears context and generates a new session ID.

## `/clear` (`/c`)

Wipes messages but preserves the session ID and iteration count. CLI-only.

## `/undo` (`/u`)

Restores the previous agent snapshot when one is available; otherwise removes the last turn (pops trailing messages until a user/system message is reached).

## `/save` (`/s`)

Opens the session DB if needed, then persists the current session. CLI-only.

## `/load`

Loads a saved session by ID.

```
/load <session_id>
```

## `/sessions`

Lists saved sessions from the session store.

## `/stats`

Prints message count, iterations, session ID, registered tool count, model, and provider.

## `/recap`

Summarizes recent activity: turn counts by role and tool usage over a recent window.

## `/conv`

Shows recent messages (default 10). Supports `/conv [N] [-role <role>]`.

## `/history`

Shows full history; supports count, `-role` filter, and search-term arguments. CLI-only.

## `/reset` (`/r`)

Saves the previous session, clears all messages, and starts a fresh session ID. It does **not** reset configuration (older docs claimed it did).

## `/retry`

Finds the last user message and re-runs the LLM call from it.

## `/compress` (`/cctx`)

Requires more than 4 messages. Keeps the system message plus the 3 most recent messages and replaces the dropped range with a generated summary message. The argument is used as a *focus* text embedded in the summary — note the registry's `args_hint` says `<keep_count>`, but the handler (`cmd_compress` in `src/cli/cli_cmd_session.c`) treats it as focus text.

```
/compress <keep_count>
```

## `/branch`

Branches at a message index (default: end of conversation): generates a new session ID and drops messages after the branch point. The argument is a numeric index, not a name.

```
/branch [message_index]
```

## `/snapshot` (`/snap`)

Stores a named checkpoint (`SNAP_<session>_<name>`, or a timestamped name when no argument is given). CLI-only.

```
/snapshot [name]
```

## `/status` (`/st`)

Shows session status and configuration info; `/status gateway` (or `platform`) prints gateway/platform status text.

## `/pause`

Engages the global emergency stop (new work is paused). `/pause off` resumes; `resume`, `stop`, and `disengage` are accepted as resume aliases. Backed by the estop sentinel (`src/hermes_cli/cli_cmd_parity.c`).

```
/pause [reason | off]
```

## `/title`

Sets a user title for the session and saves it to the DB immediately.

```
/title <title>
```

## `/resume`

Loads a saved session by ID (`/resume <session_id>`). It resumes *saved* sessions, not 'paused' ones.

```
/resume <id>
```

## `/usage`

Shows token/character usage statistics for the current session; `/usage tokens` focuses on token counts.

## `/background` (`/bg`)

Runs the prompt through `agent_chat` and prints the result. Note: the current handler executes synchronously despite the name.

```
/background <prompt>
```

## `/whoami`

Prints a fixed access level (`admin (C translation build)`) and a hardcoded platform string. It is a display handler, not a live authorization check. CLI-only.

## `/profile`

Shows the active profile; `/profile home` prints the Slermes home directory, `/profile model` the active model. CLI-only.

## `/goal`

Sets a standing goal; `/goal show` and `/goal clear` manage it. Goal data is handled by the goals port.

```
/goal <text>
```

## `/agents`

Lists active subagents and running tasks via `delegate_list`.

## `/rollback`

Without arguments, lists available checkpoints; with a name, restores that snapshot.

## `/queue`

Without arguments, shows the queued prompt; with a prompt, queues it for the next turn.

```
/queue <prompt>
```

## `/subgoal`

Adds extra criteria to the active goal. The current CLI handler acknowledges the criteria text; goal judging lives in the goals module.

```
/subgoal <text>
```

## `/insights`

Usage insights/analytics; supports `--days N` and `--source S` filters.

```
/insights [--days N]
```

## `/steer`

Injects a message into the conversation. Role options: `-u/--user`, `-a/--assistant`, `-s/--system` (default system).

```
/steer <text>
```

## `/session-search`

Searches past sessions via the `session_search` tool handler; default limit 10.

```
/session-search <query> [--limit N]
```

## `/session-export`

Exports a saved session as JSON or markdown (default: markdown).

```
/session-export <session_id> [json|markdown]
```

Subcommands: `json`, `markdown`.

## `/session-import`

Imports a session from a JSON export file.

```
/session-import <filepath>
```

## `/egress`

Reports Docker egress-proxy state from `HERMES_EGRESS_PROXY` / `HERMES_EGRESS_ENABLED` environment variables.

```
/egress [status]
```

Subcommands: `status`.

## `/journey` (`/learning`)

Opens the learning-journey timeline; delegates to `ccm_handle_journey_command` (`src/hermes_cli/commands_mixin_wrappers.c`). Subcommands: `list`, `delete <id>`, `edit <id>`. CLI-only.

```
/journey [list|delete <id>|edit <id>]
```

Subcommands: `list`, `delete`, `edit`.

## `/moa`

Runs one prompt through the default Mixture-of-Agents preset, then restores the model; delegates to `hermes_cli_moa_cmd_cmd_moa` (`src/hermes_cli/moa_cmd.c`).

```
/moa <prompt>
```

## `/prompt`

Composes the next prompt in `$EDITOR` (markdown), then sends it; delegates to `ccm_handle_prompt_compose_command`.

## `/start`

Acknowledges a platform start ping without sending a reply — print-only by design, mirroring Python.
