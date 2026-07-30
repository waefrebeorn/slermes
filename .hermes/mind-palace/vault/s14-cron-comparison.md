# Vault — Checkpoint 4 (continued) + Checkpoint 5

## Checkpoint 4 additions: CR04, SE04, SE07 (from previous session)

### CR04: AIAgent Execution in Cron — PORTED
- `src/cron/scheduler.c` — `cron_run_agent_job()` creates agent_state_t, loads hermes config, applies per-job model/provider/base_url overrides, runs `agent_chat()`, delivers response via `cron_send_notification()`
- `src/cron/scheduler.c` — `cron_add_prompt_job()` creates prompt-based jobs alongside legacy command-based ones
- `src/cron/scheduler.c` — `cron_run_loop()` dispatches: prompt→`cron_run_agent_job()` vs command→`system()`
- `src/cron/scheduler.h` — `cron_add_prompt_job()`, `cron_job_set_model()`, `cron_run_agent_job()` declarations
- `src/cron/cron_sqlite.c:29` — `prompt[4096]` field added to `cron_job_entry_t`

### SE04: System Prompt Storage in Session Meta — PORTED
- `src/tools/session_crud.c` — `set_system_prompt`/`get_system_prompt` operations added to session_crud handler
- Uses existing `session_meta_t.meta_json[4096]` blob for persistence
- `include/hermes_gateway.h` — `session_system_prompt[4096]` field added to `gw_session_entry_t`

### SE07: Telegram Topic Mode Binding — PORTED
- `src/tools/session_crud.c` — `bind_telegram_topic`/`get_telegram_topic_binding` operations added
- Uses `session_meta_t.meta_json` for persistence
- `include/hermes_gateway.h` — `telegram_topic_id[64]` field added to `gw_session_entry_t`

---

## Checkpoint 5: CR01, CR02, CR05, CR06, GW04, GW06, GW08

### CR01: One-Shot Job Advance Tracking — PORTED
- `src/cron/scheduler.h` — `repeat_count`/`run_count` fields on `cron_job_t`
- `src/cron/scheduler.c:cron_run_loop()` — increments `run_count` after each execution, deactivates job when `run_count >= repeat_count`
- `src/cron/cron_sqlite.c` — `repeat_count`/`run_count` fields added to `cron_job_entry_t`

### CR02: Field Immutability — PORTED
- `src/cron/scheduler.h` — `immutable_fields` bitmap on `cron_job_t` with `CRON_IMMUTABLE_NAME/SCHEDULE/COMMAND/PROMPT` bit flags
- `src/cron/scheduler.c` — `cron_job_is_field_immutable()`, `cron_job_set_immutable()`, `cron_job_update_field()` enforce immutability
- `src/cron/cron_sqlite.c` — `immutable_fields` field added to `cron_job_entry_t`

### CR05: Skill Loading Per Job — PORTED
- `src/cron/scheduler.c:cron_run_agent_job()` — calls `skill_cmd_scan()` before agent execution
- `src/cron/scheduler.h` — `skills_dir[1024]` field on `cron_job_t` for per-job skill directories
- `src/cron/cron_sqlite.c` — `skills_dir` field added to `cron_job_entry_t`

### CR06: Toolset Restriction for Cron — PORTED
- `src/cron/scheduler.c:cron_run_agent_job()` — copies `enabled_toolsets`/`disabled_toolsets` from job to `agent_state_t`
- `src/cron/scheduler.h` — `enabled_toolsets[1024]`/`disabled_toolsets[1024]` fields on `cron_job_t`
- `src/cron/cron_sqlite.c` — both fields added to `cron_job_entry_t`

### GW04: DeliveryRouter Abstraction — VERIFIED STALE
- `src/gateway/server.c:1524` — `gw_platform_send()` uses `gw_platform_find()` router + pre-send hooks
- Functionally equivalent to Python's DeliveryRouter class

### GW06: Automatic Platform Reconnection — VERIFIED STALE
- `src/gateway/server.c:1854-1859` — `gw_reconnect_reset()` on success, `gw_reconnect_delay()` (exponential backoff+jitter) on failure
- Wired in Telegram polling thread

### GW08: FIFO Queue Semantics — VERIFIED STALE
- `src/gateway/server.c:48-121` — bounded circular buffer (256 slots) with mutex+cond
- `gw_queue_push/pop/drain_all` maintain FIFO ordering
- Wired into polling thread at server.c:1863-1864

---

## Stale Claims Discovered This Session

| Battleship Claim | Reality | Evidence |
|-----------------|---------|----------|
| GW04: "no router abstraction" | `gw_platform_send()` IS a router | server.c:1524 |
| GW06: "no retry scheduling" | exponential backoff+jitter wired | server.c:1854-1859 |
| GW08: "Single slot per session" | 256-slot circular buffer with FIFO drain | server.c:48-121 |
| CR04: "system() only" | agent_chat() via cron_run_agent_job() | scheduler.c |
| SE04: "not stored separately" | set/get_system_prompt ops in session_crud | session_crud.c |
| SE07: "no session-level binding" | bind/get_telegram_topic_binding ops | session_crud.c |
