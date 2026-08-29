#ifndef HERMES_LOOPS_PURE_H
#define HERMES_LOOPS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/loops.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

int  loops_parse_interval_token(const char *token);
char *loops_format_interval(double seconds);
int  loops_response_signals_complete(const char *response);

int  loops_min_interval_seconds(const char *config_json);
int  loops_max_ticks_default(const char *config_json);
int  loops_self_paced_floor_seconds(const char *config_json);
int  loops_self_paced_ceiling_seconds(const char *config_json);

char *loops_parse_loop_args(const char *text);

/* LoopState (loops.py::LoopState) — opaque. Construct via from_json. */
typedef struct loop_state loop_state_t;

/* PoP: loops_state_from_json @ hermes_cli/loops.py:LoopState.from_json */
loop_state_t *loops_state_from_json(const char *raw);
/* PoP: loops_state_cadence_label @ hermes_cli/loops.py:LoopState.cadence_label */
char *loops_state_cadence_label(const loop_state_t *s);
/* PoP: loops_state_remaining_label @ hermes_cli/loops.py:LoopState.remaining_label
 * `now` is injected (Python uses time.time()) for deterministic output. */
char *loops_state_remaining_label(const loop_state_t *s, double now);
/* PoP: loops_state_is_due @ hermes_cli/loops.py:LoopManager.is_due */
int loops_state_is_due(const loop_state_t *s, double now);
/* PoP: loops_state_status_line @ hermes_cli/loops.py:LoopManager.status_line
 * `now` injected for deterministic remaining-label computation. */
char *loops_state_status_line(const loop_state_t *s, double now);

/* PoP: awaiting_response @ hermes_cli/loops.py:LoopState.awaiting_response */
bool loops_state_awaiting_response(const loop_state_t *s);

/* PoP: ticks_fired @ hermes_cli/loops.py:LoopState.ticks_fired */
long loops_state_ticks_fired(const loop_state_t *s);

/* PoP: loops_digest_response @ hermes_cli/loops.py:_digest_response
 * Returns malloc'd lowercase SHA-256 hex digest (free with free()). */
char *loops_digest_response(const char *response);

void loops_state_free(loop_state_t *s);

/* PoP: LoopState.to_json @ hermes_cli/loops.py — malloc'd JSON (free with free()). */
char *loops_state_to_json(const loop_state_t *s);

/* ── LoopManager (loops.py::LoopManager) ────────────────────────────────
 * Persistence goes through an injectable vtab (mirrors the goals_manager
 * pattern): load/save against the SessionDB state_meta table. NULL vtab =>
 * in-memory only (Python's "DB unavailable" degradation). */
typedef struct {
    /* load: malloc'd JSON for "loop:<session_id>" or NULL. */
    char *(*load)(const char *session_id);
    /* save: persist JSON under "loop:<session_id>"; 0 on success. */
    int   (*save)(const char *session_id, const char *json);
    /* list: malloc'd (JSON array of {"session_id","json"}) for prefix scan,
     * or NULL. Used by list_active_loops. */
    char *(*list_prefix)(const char *prefix);
} loops_manager_vtab_t;

typedef struct loop_manager loop_manager_t;

loop_manager_t *loops_manager_new(const char *session_id, const loops_manager_vtab_t *vtab);
void loops_manager_free(loop_manager_t *m);
const char *loops_manager_session_id(const loop_manager_t *m);
loop_state_t *loops_manager_state(loop_manager_t *m);
void loops_manager_refresh(loop_manager_t *m);
int  loops_manager_is_active(const loop_manager_t *m);
int  loops_manager_has_loop(const loop_manager_t *m);
char *loops_manager_status_line(loop_manager_t *m, double now);

/* PoP: GoalManager-backed mutation surface. `now` injected for determinism. */
loop_state_t *loops_manager_set(loop_manager_t *m, const char *prompt,
                                int interval_seconds, long times, const char *until,
                                const char *route_json, double now);
loop_state_t *loops_manager_pause(loop_manager_t *m, const char *reason);
loop_state_t *loops_manager_resume(loop_manager_t *m, double now, double self_paced_floor);
int  loops_manager_clear(loop_manager_t *m);
void loops_manager_mark_done(loop_manager_t *m, const char *reason);

/* PoP: is_due (now injected). */
int  loops_manager_is_due(loop_manager_t *m, double now);
/* PoP: fire_tick — returns malloc'd wakeup message or NULL. */
char *loops_manager_fire_tick(loop_manager_t *m, double now, double self_paced_floor);
/* PoP: abandon_tick. */
void  loops_manager_abandon_tick(loop_manager_t *m);
/* PoP: complete_tick — returns malloc'd decision JSON
 * {"status","stopped","reason","message"} (free with free()). */
char *loops_manager_complete_tick(loop_manager_t *m, const char *last_response,
                                  double now, double self_paced_floor,
                                  double self_paced_ceiling);

/* PoP: goal_blocks_loop_tick @ hermes_cli/loops.py — goal liveness via
 * injectable hooks (NULL hooks => false, Python's except path). */
typedef struct {
    int (*goal_is_active)(const char *session_id);
    int (*goal_is_waiting)(const char *session_id);
} loops_goal_hooks_t;
extern loops_goal_hooks_t loops_goal_hooks;
int loops_goal_blocks_loop_tick(const char *session_id);

/* PoP: clear_loop / list_active_loops / migrate_loop_to_session
 * (module-level persistence surface). */
int   loops_clear_loop(const char *session_id, const loops_manager_vtab_t *vtab);
char *loops_list_active_loops(const char *prefix, const loops_manager_vtab_t *vtab);
int   loops_migrate_loop_to_session(const char *old_sid, const char *new_sid,
                                    const loops_manager_vtab_t *vtab);

/* PoP: dispatch_loop_command @ hermes_cli/loops.py — surface-agnostic
 * /loop handler. Returns malloc'd JSON {"output","created"}.
 * `now` + config knobs injected for determinism. */
char *loops_dispatch_loop_command(loop_manager_t *mgr, const char *args,
                                  const char *route_json, double now,
                                  int min_interval, int max_ticks,
                                  double self_paced_floor, double self_paced_ceiling);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LOOPS_PURE_H */
