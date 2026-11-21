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

# Session Lifecycle

> **Audience:** Gateway developers and maintainers
> **Python reference:** `gateway/session.py` plus the split `gateway/session_*.py`
> modules (lifecycle, persistence, policy, recovery, transcript, ...),
> `gateway/run.py` plus the split `gateway/run_*.py` modules
> (startup, shutdown, watchers, agent_cache, busy, ...), and `gateway/config.py`
> in the quarry (`C:/Users/eman5/hermes-agent`).
> **C port:** `src/gateway/gw_session.c`, `session.c`, `session_context.c`,
> `session_ports.c`, `gateway_runtime.c`, `run.c`, `run_wrappers.c`,
> `restart_loop_guard.c`, `gateway_runner.c`, and `src/session_db.c`.
>
> This document describes the **Python reference lifecycle** (Sections 1–11)
> and the **verified C port state** (Section 12). The reference monoliths were
> split into many modules upstream; method homes below name the current
> module. Line counts are deliberately omitted — they drift; read the live
> files. C behavior parity is proven by the oracle/watercup suite
> (`tests/oracle/registry.json`, `bash tests/run_all_oracles.sh`,
> `tests/watercup/`), not by this document.

## Overview

A **session** represents a continuous conversation between the agent and one or more users on a
messaging platform. The session lifecycle governs when conversations persist, when they reset,
how they survive gateway restarts, and how messages queue during concurrent operations.

The session system lives primarily in two module families:

- `gateway/session.py` + `gateway/session_*.py` — Data model (`SessionSource`, `SessionEntry`,
  `SessionContext`), key generation (`build_session_key`), and the main store (`SessionStore`,
  with lifecycle/persistence/transcript/recovery methods split into mixin modules).
- `gateway/run.py` + `gateway/run_*.py` — Gateway runner (`GatewayRunner`) that wires sessions
  into the message processing pipeline: housekeeping/expiry watching, agent caching, restart
  recovery, and message queuing.

---

## 1. SessionSource — Message Origin Descriptor

`SessionSource` is a frozen record of *where a message came from*. It is attached to every
incoming `MessageEvent` and used for routing, isolation, and context injection.

### Fields

| Field | Type | Default | Description |
|---|---|---|---|
| `platform` | `Platform` | *(required)* | Enum identifying the messaging platform (telegram, discord, slack, signal, whatsapp, matrix, local, etc.). |
| `chat_id` | `str` | *(required)* | Platform-level chat/group/channel identifier. Routed through the adapter's `chat_id_key` transform. |
| `chat_name` | `Optional[str]` | `None` | Human-readable name of the chat or group. |
| `chat_type` | `str` | `"dm"` | One of `"dm"`, `"group"`, `"channel"`, `"thread"`. Controls session key generation and isolation. |
| `user_id` | `Optional[str]` | `None` | Platform-specific user identifier. Used for authorization and per-user session isolation. |
| `user_name` | `Optional[str]` | `None` | Display name of the message author. Injected into system prompt. |
| `thread_id` | `Optional[str]` | `None` | Forum topic / Discord thread / Slack thread identifier. Differentiates threaded conversations. |
| `chat_topic` | `Optional[str]` | `None` | Channel topic or description (Discord channel topic, Slack channel purpose). |
| `user_id_alt` | `Optional[str]` | `None` | Platform-specific stable alternative ID (Signal UUID, Feishu union_id). Used when `user_id` is ephemeral. |
| `chat_id_alt` | `Optional[str]` | `None` | Signal group internal ID — maps a Signal group V2 identifier to its canonical form. |
| `is_bot` | `bool` | `False` | True when the message author is a bot or webhook (Discord bots). |
| `guild_id` | `Optional[str]` | `None` | Discord guild / Slack workspace / Matrix server scope identifier. |
| `parent_chat_id` | `Optional[str]` | `None` | Parent channel when `chat_id` refers to a thread. |
| `message_id` | `Optional[str]` | `None` | ID of the triggering message. Used for pin/reply/react operations and Discord ID injection. |
| `role_authorized` | `bool` | `False` | True when adapter granted access via a platform role (not individual user ID). |

### Key Methods

- **`description`** (property: `str`) — Human-readable summary e.g. `"DM with Alice"`,
  `"group: My Group, thread: 12345"`.
- **`to_dict` / `from_dict`** — Serialization round-trip for persistence in `sessions.json`.

**C counterpart:** `gw_session_source_t` (`include/hermes_gateway_types.h`) carries the same
field set as fixed char buffers plus `has_data`; `session_source_set()` /
`session_source_description()` live in `src/gateway/gw_session.c`. See Section 12.

---

## 2. SessionEntry — Active Session Record

`SessionEntry` is the per-session metadata record stored in memory and persisted to
`{sessions_dir}/sessions.json`. Each entry maps a `session_key` to its current `session_id`.

### Fields

| Field | Type | Default | Description |
|---|---|---|---|
| `session_key` | `str` | *(required)* | Deterministic key identifying the conversation lane (see §4). |
| `session_id` | `str` | *(required)* | Unique identifier for this specific conversation incarnation. Format: `YYYYMMDD_HHMMSS_<8hex>`. |
| `created_at` | `datetime` | *(required)* | When this session incarnation was created. |
| `updated_at` | `datetime` | *(required)* | Last activity timestamp. Used for idle timeout and expiry checks. |
| `origin` | `Optional[SessionSource]` | `None` | The source that created this session, used for delivery routing. |
| `display_name` | `Optional[str]` | `None` | Chat display name (sourced from `SessionSource.chat_name`). |
| `platform` | `Optional[Platform]` | `None` | Platform enum, persisted for expiry policy lookup across restarts. |
| `chat_type` | `str` | `"dm"` | Chat type, also persisted for policy lookup. |
| `input_tokens` | `int` | `0` | Cumulative LLM input (prompt) tokens consumed. |
| `output_tokens` | `int` | `0` | Cumulative LLM output (completion) tokens consumed. |
| `cache_read_tokens` | `int` | `0` | Cumulative prompt cache read tokens. |
| `cache_write_tokens` | `int` | `0` | Cumulative prompt cache write tokens. |
| `total_tokens` | `int` | `0` | Total token count across all turns. |
| `estimated_cost_usd` | `float` | `0.0` | Estimated cumulative USD cost. |
| `cost_status` | `str` | `"unknown"` | Cost tracking status label. |
| `last_prompt_tokens` | `int` | `0` | Last API-reported prompt token count. Used for accurate compression pre-check. |

### Boolean Flags (State Machine)

SessionEntry has several boolean flags that form a simple state machine governing session
behavior on the next access.

| Flag | Type | Default | Description |
|---|---|---|---|
| `was_auto_reset` | `bool` | `False` | Set when a session was auto-reset due to policy expiry (idle/daily). Consumed once to inject a context notice. |
| `auto_reset_reason` | `Optional[str]` | `None` | `"idle"` or `"daily"` — why the previous session was auto-reset. |
| `reset_had_activity` | `bool` | `False` | Whether the expired session had any messages (`total_tokens > 0`). |
| `is_fresh_reset` | `bool` | `False` | Set by explicit `/new` or `/reset`. Triggers topic/channel skill re-injection on first message. Distinguished from `was_auto_reset` to avoid misleading "session expired" notices. |
| `expiry_finalized` | `bool` | `False` | Set by background expiry watcher after invoking `on_session_finalize` hooks, cleaning tool resources, and evicting the cached agent. Prevents redundant finalization across restarts. |
| `suspended` | `bool` | `False` | Hard force-wipe signal. Set by `/stop` or stuck-loop escalation (3+ consecutive restart failures). On next `get_or_create_session`, forces a new `session_id` regardless of `resume_pending`. |
| `resume_pending` | `bool` | `False` | Soft recovery marker. Set by crash recovery or drain timeout. On next access, preserves the existing `session_id` — the user continues on the same transcript. Cleared after the next successful turn completes. |
| `resume_reason` | `Optional[str]` | `None` | Why resume was marked: `"restart_timeout"`, `"shutdown_timeout"`, `"restart_interrupted"`. |
| `last_resume_marked_at` | `Optional[datetime]` | `None` | Timestamp of the last resume-pending marking. |

### State Transition Logic (get_or_create_session)

```
                    ┌──────────┐
                    │  Incoming │
                    │  Message  │
                    └────┬─────┘
                         │
                         ▼
              ┌──────────────────────┐
              │  session_key exists  │──── No ──► Create fresh SessionEntry
              │  AND !force_new      │
              └──────────┬───────────┘
                         │ Yes
                         ▼
              ┌──────────────────────┐
              │  entry.suspended?    │──── Yes ──► Auto-reset: new session_id
              └──────────┬───────────┘           (reason="suspended")
                         │ No
                         ▼
              ┌──────────────────────┐
              │ entry.resume_pending?│──── Yes ──► Return existing entry
              └──────────┬───────────┘           (preserve session_id)
                         │ No                     Clear flag on next successful turn
                         ▼
              ┌──────────────────────┐
              │   Policy says reset? │──── Yes ──► Auto-reset: new session_id
              └──────────┬───────────┘           (reason="idle"/"daily")
                         │ No
                         ▼
              ┌──────────────────────┐
              │  Return existing     │
              │  entry, bump         │
              │  updated_at          │
              └──────────────────────┘
```

**Priority order in `get_or_create_session`:**
1. `suspended=True` → always force-reset (hard wipe)
2. `resume_pending=True` → preserve session_id (soft recovery)
3. Policy expiry (idle/daily) → auto-reset
4. No trigger → return existing entry (bump `updated_at`)

---

## 3. SessionStore — Storage and Operations

`SessionStore` is the main storage layer. It maintains an in-memory dict (`_entries`) persisted
to `sessions.json`, with SQLite (`SessionDB`) as the canonical store for session metadata and
message transcripts.

### Constructor

```python
SessionStore(sessions_dir: Path, config: GatewayConfig, has_active_processes_fn=None)
```

- `sessions_dir` — Directory where `sessions.json` lives.
- `config` — `GatewayConfig` instance for reset policy lookups.
- `has_active_processes_fn` — Optional callback keyed by `session_key` to check for running
  background processes. Sessions with active processes are never expired or pruned.

### Operations (methods)

The store surface is split across mixin modules upstream; the current home of each method
is noted where it moved out of `session.py`.

| Method | Description |
|---|---|
| `get_or_create_session(source, force_new=False)` | Core entry point (`session.py`). Returns existing or creates new `SessionEntry`. Evaluates `suspended`, `resume_pending`, and reset policy. Creates/ends SQLite records. |
| `update_session(session_key, last_prompt_tokens=None)` | Lightweight metadata update after an interaction (`session.py`). Bumps `updated_at`, optionally records `last_prompt_tokens`. |
| `reset_session(session_key, display_name=None)` | Explicit reset from `/new` or `/reset` (`session.py`). Creates new `session_id`, sets `is_fresh_reset=True`. Ends old SQLite session, creates new one. |
| `switch_session(session_key, target_session_id)` | Switch to a different existing session ID from `/resume` (`session.py`). Ends current SQLite session, reopens target. |
| `suspend_session(session_key)` | Mark `suspended=True` from `/stop` (`session_lifecycle.py`). Forces auto-reset on next access. |
| `mark_resume_pending(session_key, reason)` | Mark `resume_pending=True` (`session_lifecycle.py`). Preserves session_id on next access. Will NOT override `suspended=True`. |
| `clear_resume_pending(session_key)` | Clear `resume_pending` after a successful resumed turn (`session_lifecycle.py`). Called from the gateway after the conversation returns. |
| `suspend_recently_active(max_age_seconds=120)` | Legacy crash-recovery surface: mark recently-active sessions `resume_pending=True`. The current reference recovery is delivery-ledger + active-turn-marker based (`run_startup.py`, `session_recovery.py`); the C port retains this function (see §12). |
| `prune_old_entries(max_age_days)` | Drop entries older than `max_age_days` based on `updated_at` (`session_lifecycle.py`). Skips `suspended` entries and sessions with active processes. |
| `list_sessions(active_minutes=None)` | Return all sessions, optionally filtered by recent activity (`session.py`). Sorted by `updated_at` descending. |
| `lookup_by_session_id(session_id)` | Find the active `SessionEntry` for a persisted session ID (`session.py`). |
| `has_any_sessions` | Check if any sessions have ever been created (`session.py`; uses SQLite for history, not just the in-memory dict). |
| `append_to_transcript(session_id, message, skip_db=False)` | Append a message to the SQLite transcript (`session_transcript.py`). `skip_db=True` prevents duplicate writes when the agent already persisted. |
| `rewrite_transcript(session_id, messages)` | Full transcript replacement used by `/retry`, `/undo`, `/compress` (`session_transcript.py`). |
| `load_transcript(session_id)` | Load all messages from a session's SQLite transcript (`session_transcript.py`). |
| `rewind_session(session_id, n=1)` | Back up `n` user turns via soft-delete, keeping the audit trail (`session_transcript.py`). Returns `{rewound_count, turns_undone, target_text}`. |

### Internal helpers

- `_ensure_loaded` / `_ensure_loaded_locked` — Load `sessions.json` into `_entries`
  (`session_persistence.py`).
- `_save` / `_save_sessions_json` / `_save_entries` — Atomic write to `sessions.json` via
  temp file + replace (`session_persistence.py`).
- `_generate_session_key(source)` — Delegates to `build_session_key` with config params
  (`session_recovery.py`).
- `_is_session_expired(entry)` — Policy check from entry alone (no source needed), used by
  the background housekeeping watcher.
- `_should_reset(entry, source)` — Policy check returning `"idle"`, `"daily"`, or `None`.

### Storage layout

```
{sessions_dir}/
  sessions.json          # In-memory _entries dict, persisted as JSON
                           Maps session_key → SessionEntry (metadata only)
  {session_id}.jsonl     # (Legacy, removed in spec 002)
```

The canonical transcript store is SQLite via `SessionDB` (from `hermes_state`). The
`sessions.json` file persists the `session_key → session_id` mapping and entry metadata
(flags, timestamps, token counts). If SQLite is unavailable, the store falls back to
JSONL, but this is a degradation path.

---

## 4. SessionKey Generation Rules

Session keys are deterministic strings that identify a conversation lane. They are generated
by `build_session_key(source, group_sessions_per_user, thread_sessions_per_user)`
(`gateway/session.py`).

### Key format

```
agent:main:{platform}:{chat_type}[:{chat_id}][:{thread_id}][:{participant_id}]
```

### DM rules

| Scenario | Key |
|---|---|
| DM with chat_id | `agent:main:telegram:dm:12345` |
| DM with chat_id + thread | `agent:main:telegram:dm:12345:thread_678` |
| DM without chat_id, with participant_id | `agent:main:signal:dm:user_abc` |
| DM without chat_id or participant_id | `agent:main:telegram:dm` |
| WhatsApp DM (canonicalized) | `agent:main:whatsapp:dm:{canonical_number}` |

- DMs always include `chat_id` when present, isolating each private conversation.
- `thread_id` further differentiates threaded DMs within the same DM chat.
- Without `chat_id`, falls back to `user_id_alt` or `user_id` as participant_id.
- Without any identifier, all DMs on that platform collapse to one shared session.

### Group/channel rules

| Scenario | Key |
|---|---|
| Group chat | `agent:main:telegram:group:-10012345` |
| Group chat, per-user isolation | `agent:main:telegram:group:-10012345:user_abc` |
| Thread in group, shared | `agent:main:discord:group:12345:thread_678` |
| Thread in group, per-user | `agent:main:discord:group:12345:thread_678:user_abc` |
| Channel | `agent:main:slack:channel:C12345` |
| WhatsApp group (canonicalized) | `agent:main:whatsapp:group:{canonical_id}:{participant}` |

- `chat_id` identifies the parent group/channel.
- `thread_id` differentiates threads within that parent.
- **Per-user isolation** (append `participant_id`) is controlled by:
  - `group_sessions_per_user` (default: `True`) — group/channel sessions are isolated.
  - `thread_sessions_per_user` (default: `False`) — threads are **shared** by default
    (Telegram forum topics, Discord threads, Slack threads all share one session per thread).
- `participant_id` = `user_id_alt` or `user_id` (in that priority).
- WhatsApp identifiers are canonicalized to handle JID/LID alias flips.

### Special case: WhatsApp

WhatsApp phone numbers go through `canonical_whatsapp_identifier`, which strips the
`@s.whatsapp.net` suffix and normalizes to E.164 format. This prevents session fragmentation
when the bridge returns different alias forms of the same phone number.

> **C port divergence:** the C store's `build_session_key()` currently emits the shorter
> `platform:chat_id` form — see Section 12 for the full mapping and the parser that
> understands the `agent:main:` format.

---

## 5. Multi-User Isolation Strategy

Multi-user isolation determines whether multiple users in the same chat share a conversation
or each get their own private session.

### Decision logic (is_shared_multi_user_session)

```python
def is_shared_multi_user_session(source, *, group_sessions_per_user, thread_sessions_per_user):
    if source.chat_type == "dm":
        return False  # DMs are always private
    if source.thread_id:
        return not thread_sessions_per_user  # Threads: shared unless per-user
    return not group_sessions_per_user       # Groups: isolated unless shared
```

### Summary

| Chat Type | Default | Config Control |
|---|---|---|
| DM | Private (never shared) | None — DMs are always private |
| Group/Channel | Per-user isolation | `group_sessions_per_user` (default: True) |
| Thread (forum, discord) | Shared (all participants see same context) | `thread_sessions_per_user` (default: False) |

### Impact on system prompt

When `shared_multi_user_session=True`, the system prompt omits a fixed user name and instead
states: *"Multi-user {thread|session} — messages are prefixed with [sender name]. Multiple
users may participate."* Individual sender names are prefixed on each user message by the
gateway at runtime, preserving prompt caching (the system prompt doesn't change per-turn).

**C counterpart:** `is_shared_multi_user_session()` in `src/gateway/gw_session.c` implements
the identical decision logic (verified against the Python source above).

---

## 6. Reset Policy

Reset policies control when a session automatically loses context (gets a new `session_id`).

### Policy modes (SessionResetPolicy)

| Mode | Behavior | Default Config |
|---|---|---|
| `"none"` | Never auto-reset. Context managed only by compression. | — |
| `"idle"` | Reset after N minutes of inactivity from `updated_at`. | `idle_minutes: 1440` (24h) |
| `"daily"` | Reset at a specific hour each day (local time). | `at_hour: 4` (4 AM) |
| `"both"` | Whichever triggers first — daily boundary OR idle timeout. | **(default)** |

### Policy evaluation

```python
# Idle check
idle_deadline = entry.updated_at + timedelta(minutes=policy.idle_minutes)
if now > idle_deadline: return "idle"

# Daily check
today_reset = now.replace(hour=policy.at_hour, minute=0, second=0, microsecond=0)
if now.hour < policy.at_hour:
    today_reset -= timedelta(days=1)  # Reset hasn't happened yet today
if entry.updated_at < today_reset: return "daily"
```

### Per-platform/per-type policies

Reset policies are configurable per platform and session type via `config.get_reset_policy`.
This allows different platforms to have different expiry rules (e.g., Telegram DMs reset
after 24h idle, but Slack groups persist indefinitely).

### Exclusions

Sessions with active background processes are **never** expired or reset. The
`has_active_processes_fn` callback checks for running processes when evaluating policies.

### Reset effects

When a reset triggers:

1. Old session is ended in SQLite (with reason `"session_reset"`).
2. New `session_id` is generated (`YYYYMMDD_HHMMSS_<8hex>`).
3. New `SessionEntry` is created with `was_auto_reset=True` and the reset reason.
4. `reset_had_activity` is set if the old session had any turns (`total_tokens > 0`).
5. The old AIAgent cache entry is evicted on the next housekeeping watcher pass.
6. On the first message after reset, a context notice is injected: "Session expired due to inactivity / daily reset."

**C counterpart:** `session_should_reset()` in `src/gateway/gw_session.c` implements
none/idle/daily/both against `g_gw.reset_policy_mode`, `reset_policy_idle_min`, and
`reset_policy_at_hour`; the daily check approximates the boundary with seconds-since-reset-hour
(with wraparound). Reset ends the SQLite session with reason `"session_reset"`
(`db_end_session`, gw_session.c).

---

## 7. Restart Recovery Flow

The restart recovery system ensures that in-flight sessions are preserved across gateway
restarts, crashes, and drain timeouts. It is the solution to issue #7536.

### Startup recovery sequence

```
Gateway starts
       │
       ▼
┌───────────────────────────────┐
│ Check for .clean_shutdown     │── Exists? ──► Skip suspension (clean exit)
│ marker                        │
└───────────────────────────────┘
       │ Missing
       ▼
┌───────────────────────────────┐
│ Recover interrupted turns:    │── Active-turn markers + delivery-ledger
│ active-turn markers, resume-  │   claims (run_startup.py); legacy surface:
│ pending marking               │   suspend_recently_active(120s)
└───────────────────────────────┘
       │
       ▼
┌───────────────────────────────┐
│ _suspend_stuck_loop_sessions  │── Suspends sessions that have been
│ (run_startup.py)              │   active across repeated restarts
└───────────────────────────────┘
       │
       ▼
┌───────────────────────────────┐
│ Queue inbound messages while  │
│ startup restore runs          │
│ (_startup_restore_in_progress)│
└───────────────────────────────┘
       │
       ▼
┌───────────────────────────────┐
│ _schedule_resume_pending_     │
│ sessions: synthesize events   │
│ and let the agent auto-       │
│ continue; ledger-claimed rows │
│ clear resume_pending first    │
└───────────────────────────────┘
```

### Crash-recovery marking

On startup without a `.clean_shutdown` marker (crash or unexpected exit), recently-active
sessions are marked for resume:

- `resume_pending=True`, `resume_reason="restart_interrupted"`,
  `last_resume_marked_at=now`.
- Entries already `resume_pending=True` are not double-marked.
- Entries explicitly `suspended=True` stay hard-wiped.

The historical surface for this was `suspend_recently_active(max_age_seconds=120)`. The
current reference implements recovery through durable active-turn markers and
delivery-ledger claims in `gateway/run_startup.py` / `gateway/session_recovery.py`
(`_resume_pending_candidates`, `_schedule_resume_pending_sessions`,
`_clear_resume_pending_for_claimed_obligations`). The C port keeps both: marker-based
`session_recover_interrupted_turns()` plus the legacy `suspend_recently_active(120)` pass
(Section 12).

### Stuck-loop detection

Consecutive restart-interrupted boots are tracked in
`{HERMES_HOME}/gateway/restart_loop.json` via `gateway/restart_loop_guard.py` (boot window +
chain logic). Sessions active across repeated restarts are auto-suspended by
`_suspend_stuck_loop_sessions` so the user gets a clean slate. (Older revisions used a
`restart_counts.json` counter file; the current reference and the C port both use
`restart_loop.json`.)

### Drain-timeout marking

On graceful shutdown/restart, the drain system calls `mark_resume_pending` for any session
that was mid-turn when the drain timeout fired. Reasons:

- `"restart_timeout"` — killed during restart drain
- `"shutdown_timeout"` — killed during shutdown drain
- `"restart_interrupted"` — crash recovery

All three reasons are in `_AUTO_RESUME_REASONS` (`gateway/run.py`) and eligible for startup
auto-resume.

### Auto-resume on next access

When `get_or_create_session` encounters `resume_pending=True`:

1. It returns the existing entry **without** creating a new `session_id`.
2. The existing transcript is loaded intact.
3. The marking is not cleared here — it survives until the next successful turn completes
   (`clear_resume_pending` is called from the gateway after the conversation returns a real
   response).
4. If the resumed turn is interrupted again, the `resume_pending` flag remains set and the
   next restart retries; the restart-loop guard handles terminal escalation.

### Clean shutdown marker (.clean_shutdown)

Written at the end of a graceful shutdown (`gateway/run_shutdown.py`); skipped when the drain
timed out with interrupted agents. On next startup (`gateway/run_startup.py`,
`_consume_clean_shutdown_marker`):

- If present: skip crash-recovery suspension — active agents were already drained.
- Then delete the marker.

This prevents unwanted auto-resets after `hermes update`, `hermes gateway restart`, or
`/restart`.

---

## 8. Message Queuing Flow

The message queuing system handles two scenarios:

1. **Interrupt follow-ups** — When a user sends multiple messages while the agent is
   processing, subsequent messages are queued as single-slot pending messages.
2. **`/queue` FIFO** — Explicit `/queue` commands that must each produce their own full
   agent turn, in order, without merging.

### Data structures

```
adapter._pending_messages: Dict[session_key, MessageEvent]
    └── Single "next-up" slot per session. Overwritten on repeat sends
        (burst collapse). Shared with photo-burst follow-ups.

self._queued_events: Dict[session_key, List[MessageEvent]]
    └── Overflow buffer. Each /queue invocation appends here when the
        slot is occupied. Promoted one-at-a-time after each drain.
```

### Enqueue (_enqueue_fifo)

```
_enqueue_fifo(session_key, event, adapter)
       │
       ▼
┌───────────────────────────────────────┐
│ Is slot free?                         │
│ (session_key NOT in _pending_messages)│── Yes ──► Place event in slot
└───────────────────────────────────────┘
       │ No
       ▼
Append to _queued_events[session_key] (overflow tail)
```

### Dequeue / promotion (_promote_queued_event)

Called at the drain site after the slot was consumed (`gateway/run_busy.py`). If there's an
overflow item:

- When `pending_event is None` (slot was empty), return overflow head as the new event.
- When `pending_event` exists, stage overflow head in the slot for the next recursion.
- If no adapter is available, push back to `_queued_events` (don't silently drop).

### Queue depth

`_queue_depth(session_key, adapter)` returns `len(overflow) + (1 if slot occupied else 0)`.

### Clearing

Queued events for a session are cleared on `/new` and `/reset` (via the reset command
handler).

### FIFO invariant

Each `/queue` invocation produces exactly one full agent turn, in FIFO order, with no
merging. The single-slot `_pending_messages` + overflow `_queued_events` design ensures
that repeated sends during an active turn don't cause out-of-order processing.

**C counterpart:** `grun_u_enqueue_fifo` / `grun_u_promote_queued_event`
(`src/gateway/run_wrappers.c`) and the per-session queue map in
`src/gateway/gateway_runner.c` mirroring `_queued_events`.

---

## 9. Session Context Injection

`SessionContext` is built from a `SessionSource` and `GatewayConfig` and injected into the
agent's system prompt. It tells the agent:

- Where the current message came from
- What platforms are connected
- Where it can deliver scheduled task outputs
- Whether this is a shared multi-user session

### Construction (build_session_context)

```python
def build_session_context(source, config, session_entry=None) -> SessionContext
```

(`gateway/session.py`.)

1. Collects connected platforms from config.
2. Collects home channels for each platform.
3. Determines `shared_multi_user_session` via `is_shared_multi_user_session`.
4. Attaches session metadata (key, id, timestamps) if `session_entry` is provided.

### PII redaction (build_session_context_prompt)

The dynamic system prompt section (`## Current Session Context`) can optionally redact
personally identifiable information before sending to the LLM
(`build_session_context_prompt(context, *, redact_pii=False)` in `gateway/session.py`):

- User IDs → `user_<12hex>` (SHA-256 prefix)
- Chat IDs → `<platform>:<12hex>` or just `<12hex>`
- Platforms excluded from redaction: Discord (needs raw IDs for `@mentions`),
  and any plugin-registered platform not marked `pii_safe`.

Redaction applies only to the system prompt text. Routing, session keys, and adapter
operations always use the original values.

**C counterpart:** `build_session_context_prompt()` in `src/gateway/server.c` emits the
`## Current Session Context` block and hashes IDs via `pii_hash()` (gw_session.c) for the
PII-safe platforms (telegram, whatsapp, signal); other platforms get the plain
`session_source_description()`. Note that `src/gateway/session_context.c` ports a different
Python module — `gateway/session_context.py`, the session-id env/thread-local plumbing
(Section 12).

---

## 10. Background Housekeeping Watcher

The reference runs `_session_housekeeping_watcher` (`gateway/run_watchers.py`,
`GatewaySessionWatchersMixin`) every 300 seconds (5 min), after an initial 60s settle.
Older revisions named this task `_session_expiry_watcher`; the C port's wrapper
(`grun_u_session_expiry_watcher`) preserves that historical name.

### Responsibilities

1. **Sweep idle cached agents** — `_sweep_idle_cached_agents` evicts agents idle beyond
   `_AGENT_CACHE_IDLE_TTL_SECS` (3600s / 1h), regardless of session reset policy. This
   prevents unbounded memory growth in gateways with long-lived sessions.
2. **Shed memory under pressure** — `_sweep_agent_cache_under_pressure` releases LRU
   transcripts when the heap is over budget (cgroup/RSS pressure); they reload from the
   persisted session on the next turn.
3. **Prune stale entries** — hourly, calls `session_store.prune_old_entries` based on
   `config.session_store_max_age_days`, preventing `sessions.json` from growing unbounded.
4. **Finalize expired sessions** — expired entries are finalized (plugin
   `on_session_finalize` hooks, cached-agent resource cleanup, eviction, override clearing)
   and marked `expiry_finalized=True` so finalization is not repeated across restarts.

### Failure handling

- Per-session retry count: each failed finalize is retried up to 3 consecutive times.
- After 3 failures, the entry is force-marked `expiry_finalized=True` to prevent infinite
  retry loops.

---

## 11. Agent Cache

The gateway maintains an LRU cache of `AIAgent` instances keyed by `session_key` to
preserve prompt caching across turns.

### Cache properties (Python reference)

- **Max size:** 128 entries (`_AGENT_CACHE_MAX_SIZE`, `gateway/run.py`).
- **Eviction policy:** Least-recently-used (LRU via `OrderedDict`).
- **Idle TTL:** 3600s (1h) — enforced by the housekeeping watcher.
- **Lock:** `_agent_cache_lock` (threading) for thread safety.

### Cache lifecycle

```
Message arrives
    │
    ▼
get_or_create_session  →  session_key obtained
    │
    ▼
Lookup _agent_cache[session_key]
    │
    ├── Hit → move_to_end, reuse AIAgent (preserves prompt cache)
    │
    └── Miss → create new AIAgent, store in cache
                (if at capacity, popitem(last=False) evicts LRU entry)
    │
    ▼
run_conversation  →  agent processes message
    │
    ▼
Housekeeping watcher evicts agent when session finalizes
```

### Cleanup flow

When a session expires:
1. `_cleanup_agent_resources(agent)` — shuts down memory provider, closes tool resources.
2. `_evict_cached_agent(key)` — removes from `_agent_cache` so the agent can be GC'd.

**C counterpart:** `src/gateway/gateway_runtime.c` — hive-backed LRU with a pthread mutex
(`gw_agent_cache_get/put`, `gw_agent_cache_sweep_idle`). The C cap is
`GW_AGENT_CACHE_MAX` = 32 (sized for real gateway usage of 1–3 sessions) versus the Python
128; the idle TTL matches at 3600s. Pressure sweep: `gw_sweep_agent_cache_under_pressure`
(`src/gateway/run.c`).

---

## 12. C Port State (verified)

Everything below was checked against the C source tree at rewrite time. File existence and
function names are verified; **behavioral** parity claims belong to the oracle/watercup
suite — run `bash tests/run_all_oracles.sh` and the watercup gates, and audit coverage with
`python scripts/audit_oracle_registry.py` (registry: `tests/oracle/registry.json`). Live
port counts: the `PARITY:AUTO` sentinel and `docs/parity-summary.md`.

### Module mapping

| Python reference surface | C location | Verified symbols / notes |
|---|---|---|
| `SessionSource` (session.py) | `include/hermes_gateway_types.h` (`gw_session_source_t`), `src/gateway/gw_session.c` | `session_source_set`, `session_source_description`, `gw_session_set_source`, hive-backed source LRU (`gw_source_cache_entry_t`, `source_cache_put`) |
| `SessionEntry` (session.py) | `include/hermes_gateway_types.h` (`gw_session_entry_t`) | `key`, `session_id`, `source`, `last_active`, `resume_pending`, `resume_reason`, `last_resume_marked_at`, `suspended`, `expiry_finalized`, `active_turn_token`/`active_turn_started_at`, `model_override`, `session_system_prompt`, `telegram_topic_id`, `update_prompt_pending` |
| `SessionStore` (session.py + session_lifecycle/persistence mixins) | `src/gateway/gw_session.c` (hive-backed `g_gw.sessions` table) | `session_get_or_create`, `session_find`, `session_find_by_key`, `session_create`, `save_entries`, `save_sessions_json`, `prune_old_entries`, `suspend_recently_active`, `mark_resume_pending`, `clear_resume_pending`, `switch_session`, `set_expiry_finalized`, `session_cleanup_idle`, `session_limit_max` |
| `build_session_key` (session.py) | `src/gateway/gw_session.c`; `gss_build_session_key` in `session_ports.c` | **Divergence:** C emits `platform:chat_id` (and `platform:chat_id:thread_id` in `gss_build_session_key`), not the full `agent:main:{platform}:{chat_type}:...` lane key. `gateway_parse_session_key` (`run_pure.c`) parses the full `agent:main:` format. |
| `is_shared_multi_user_session` (session.py) | `src/gateway/gw_session.c` | Identical decision logic (dm → private; thread → `!thread_sessions_per_user`; else `!group_sessions_per_user`) |
| Reset policy (`SessionResetPolicy`, `_should_reset`) | `src/gateway/gw_session.c` | `session_should_reset` — none/idle/daily/both from `g_gw.reset_policy_*`; daily boundary approximated via seconds-since-reset-hour with wraparound; reset ends SQLite session with reason `"session_reset"` |
| Active-turn markers / crash recovery (session.py, run_startup.py, session_recovery.py) | `src/gateway/session.c` | `session_mark_turn_active`, `session_clear_turn_active`, `session_recover_interrupted_turns`, `session_discard_active_turn_markers`, `session_drain_spooled_drops`; startup in `run.c` uses `marker_max_age = max(3600, 2×HERMES_AGENT_TIMEOUT)` (default timeout 1800s), then legacy `suspend_recently_active(120)` |
| `.clean_shutdown` consumption (run_startup.py) | `src/gateway/run.c` | `gw_consume_clean_shutdown_marker` |
| Restart-loop guard (restart_loop_guard.py) | `src/gateway/restart_loop_guard.c` | `restart_loop_state_path` → `{home}/gateway/restart_loop.json`; `restart_loop_load_boots`, `restart_loop_save_boots`, `restart_loop_record_boot`, `restart_loop_is_tripped`, `restart_loop_check_and_record`, `restart_loop_chain_gap` |
| Stuck-loop suspension (run_startup.py) | `src/gateway/run_wrappers.c` | `grun_u_suspend_stuck_loop_sessions` |
| Housekeeping/expiry watcher (run_watchers.py) | `src/gateway/run_wrappers.c`, `gateway_runtime.c`, `run.c` | `grun_u_session_expiry_watcher` (historical name), `gw_agent_cache_sweep_idle` (cleanup thread), `gw_sweep_agent_cache_under_pressure` |
| Agent cache (run.py, run_agent_cache.py) | `src/gateway/gateway_runtime.c` | Hive LRU + pthread mutex; `GW_AGENT_CACHE_MAX` 32 (Python: 128), `GW_AGENT_CACHE_IDLE_TTL` 3600 (matches) |
| Message queuing (run.py `_enqueue_fifo`, run_busy.py `_promote_queued_event`) | `src/gateway/run_wrappers.c`, `gateway_runner.c` | `grun_u_enqueue_fifo`, `grun_u_promote_queued_event`; per-session queue map mirroring `_queued_events` |
| `gateway/session_context.py` (session-id plumbing — NOT the SessionContext prompt record) | `src/gateway/session_context.c` | `set_current_session_id` (env `HERMES_SESSION_ID` + pthread thread-local), `scoped_current_session_id`(+`_restore`), `set_session_vars`, `clear_session_vars`, `reset_session_vars`, `get_session_env`, `gw_session_is_messaging_surface` |
| `build_session_context_prompt` (session.py) | `src/gateway/server.c` | `## Current Session Context` block; `pii_hash` for telegram/whatsapp/signal IDs; multi-user detection |
| `SessionDB` (hermes_state) | `src/session_db.c` | SQLite session/message operations (`db_open`, `db_end_session`, transcript CRUD) |
| `gateway/session_state.py` (relay per-session state) | `src/gateway/session_state.c` | `relay_session_state_registry_new`, `relay_session_state_get_or_create`, turn/conversation state clear — a separate concern from `SessionStore` |

### Known C-vs-reference divergences (honest port state)

- **Session key format:** C store keys are `platform:chat_id` (see above); the full
  `agent:main:` lane format is parsed (`gateway_parse_session_key`) but not yet emitted by
  the C key builder.
- **Entry flags:** `was_auto_reset`, `auto_reset_reason`, `reset_had_activity`, and
  `is_fresh_reset` are not fields of `gw_session_entry_t`; auto-reset context notices and
  fresh-reset skill re-injection are reference behaviors still to be carried by the C
  entry record.
- **Per-entry token/cost counters:** the Python `SessionEntry` carries cumulative
  token/cost fields; the C entry does not — usage accumulates on `agent_state_t`
  (`session_total_tokens`, `session_estimated_cost_usd`, surfaced by the CLI status bar).
- **Agent cache cap:** 32 (C) vs 128 (Python); idle TTL matches (3600s).
- **Daily reset boundary:** C approximates with seconds-since-reset-hour rather than
  comparing against the exact local-time boundary timestamp.

### Oracle coverage

No `tests/sta_oracle_session.py` / `sta_oracle_gw_session.py` /
`sta_oracle_session_context.py` entries exist in `tests/oracle/registry.json` at rewrite
time — these surfaces are currently **C units exist**, with behavioral proof owed to the
watercup gateway gates and future differential oracles. Check current coverage with
`python scripts/audit_oracle_registry.py` rather than trusting this paragraph.

---

## Appendix: Key Configuration

| Config Key | Type | Default | Description |
|---|---|---|---|
| `group_sessions_per_user` | `bool` | `true` | Isolate group/channel sessions per user |
| `thread_sessions_per_user` | `bool` | `false` | Isolate thread sessions per user |
| `session_store_max_age_days` | `int` | `0` | Prune sessions older than N days (0=disabled) |
| `agent.gateway_auto_continue_freshness` | `int` | `3600` | Seconds for resume freshness window |
| `agent.gateway_timeout` | `int` | `1800` | Agent turn timeout (30 min default) |

### Reset policy (per-platform/type, in config.yaml)

```yaml
session_reset:
  mode: both            # none | idle | daily | both
  at_hour: 4            # daily reset hour (local time)
  idle_minutes: 1440    # idle timeout (24h)
  notify: true          # notify user on auto-reset
```

Platform-specific overrides can be set under `platforms.<name>.session_reset`.
