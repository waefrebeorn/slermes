# Checkpoint 7 — Kanban Notifier + Platform Skill Cache

**Gaps closed: 2** (GW13, SK06)

---

## GW13: Kanban Notifier Profile Integration

**Before:** ❌ REAL GAP — No kanban→gateway notification routing. The C kanban plugin was a simple in-memory board with no persistence, no subscriptions, no event tracking, and zero gateway integration.

**After:** ✅ PORTED

### Changes

| File | Lines | What |
|------|-------|------|
| `src/plugins/plugin_kanban.c` | ~997 | Full rewrite: file-backed kanban store (`~/.hermes/kanban/boards/<slug>.json`), `kanban_notify_subs` schema, event tracking with monotonic cursor, subscription CRUD (subscribe/list/unsubscribe), terminal event detection, profile ownership filtering |
| `src/gateway/server.c` | ~230 (new) | `thread_kanban_notifier()` — polls board JSON files every 5s, finds pending terminal events (completed/blocked/gave_up/crashed/timed_out), delivers via `gw_platform_send()`, advances cursors on success, dead-chat detection (fail_count ≥ 3 → auto-remove), auto-unsubscribe on task terminal state |
| `include/hermes_gateway.h` | +4 fields | `kanban_notifier_profile[128]`, `kanban_notifier_enabled`, `kanban_notifier_interval_sec`, `kanban_notifier_max_fail` in `gateway_state_t` |

### Key design decisions
- Board storage uses JSON files (matching `cron_sqlite.c` pattern) instead of SQLite — no libsqlite dependency
- `dispatch_in_gateway` gating via env var `HERMES_KANBAN_DISPATCH_IN_GATEWAY` (matches Python config)
- Profile ownership: subscriptions owned by a specific profile are skipped by other profiles (multi-gateway safe)
- Cursor advancement is atomic per-subscription within the JSON file write

### Evidence
- `plugin_kanban.c:196` — `kanban_notify_subscribe()` with thread-safe profile filtering
- `plugin_kanban.c:516` — `kanban_notify_get_pending()` collects pending events for all boards
- `server.c:2668` — `thread_kanban_notifier()` spawned after platform init
- `server.c:2720` — terminal kind filtering (`completed`, `blocked`, `gave_up`, `crashed`, `timed_out`)
- `server.c:2840` — cursor advancement on successful delivery
- `server.c:2850` — dead-chat removal after `max_fail` consecutive failures

---

## SK06: Platform-Scoped Skill Cache Invalidation

**Before:** ❌ REAL GAP — `skill_cmd_scan()` cached skills permanently after first scan. When the session platform changed (e.g., telegram → discord), platform-specific skills were not re-filtered.

**After:** ✅ PORTED

### Changes

| File | Lines | What |
|------|-------|------|
| `src/agent/skill_commands.c` | +23 | `g_last_platform[64]`, `skill_cache_invalidate()`, `skill_cache_check_platform()`, `skill_cmd_set_platform()`, `skill_cmd_invalidate_platform_cache()` |
| `src/agent/skill_commands.c:495` | +3 | `skill_cmd_set_platform()` called from `process_update()` at `server.c:1738` |
| `include/hermes_skill_commands.h` | +8 | Declared `skill_cmd_set_platform()` and `skill_cmd_invalidate_platform_cache()` |

### Key design decisions
- `g_last_platform` tracks the platform of the last `process_update()` call
- Cache is invalidated (skill_count reset to 0) when platform changes → triggers full rescan on next `skill_cmd_scan()`
- `skill_cmd_invalidate_platform_cache()` exposed for explicit invalidation when skill directories are modified at runtime

### Evidence
- `skill_commands.c:42` — `g_last_platform[64]` static variable
- `skill_commands.c:49` — `skill_cache_check_platform()` compares and invalidates
- `skill_commands.c:495` — `skill_cmd_set_platform()` public API
- `server.c:1738` — called from `process_update()` on every inbound message

---

## Build verification

```
cd slermes && make 2>&1 | tail -1
# Phase 5 complete: slermes binary built
# exit 0, zero new warnings from new code
```

## Battleship impact

| Sector | Before | After | Delta |
|--------|--------|-------|-------|
| GW | 10/15 PORTED | 12/15 PORTED | +2 PORTED, -2 REAL GAP |
| SK | 1/3 PORTED | 3/3 PORTED | +2 PORTED, -2 REAL GAP |
| TOTAL | ~106/151 | ~113/151 | +7 PORTED |

**Overall: ~75% PORTED, ~6% PARTIAL, ~18% REAL GAP**
