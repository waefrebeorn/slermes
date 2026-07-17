/*
 * goal_contract.h — opaque API for Hermes session-goal data model.
 *
 * Faithful C port of the pure data layer of hermes_cli/goals.py:
 *   - GoalContract (outcome/verification/constraints/boundaries/stop_when)
 *   - GoalState (serializable per-session goal state)
 *   - parse_contract(text) -> (headline, contract)
 *   - JSON (de)serialization, contract render, subgoal render, meta-key
 *
 * This module is pure logic: no DB, no TTY, no network. The persistence
 * layer (SessionDB state_meta) and the judge/continuation loop live
 * elsewhere and wire this in.
 *
 * Opaque structs + minimal includes (only <stddef.h>, <stdbool.h>). Caller
 * never sees field layout; all access is via the functions below. Every
 * function returning char* yields a heap buffer the caller must free().
 *
 * PoP: goal_meta_key            @ hermes_cli/goals.py:_meta_key
 * PoP: goal_contract_is_empty   @ hermes_cli/goals.py:GoalContract.is_empty
 * PoP: goal_contract_to_dict    @ hermes_cli/goals.py:GoalContract.to_dict
 * PoP: goal_contract_from_dict  @ hermes_cli/goals.py:GoalContract.from_dict
 * PoP: goal_contract_render     @ hermes_cli/goals.py:GoalContract.render_block
 * PoP: parse_contract           @ hermes_cli/goals.py:parse_contract
 * PoP: goal_state_to_json       @ hermes_cli/goals.py:GoalState.to_json
 * PoP: goal_state_from_json     @ hermes_cli/goals.py:GoalState.from_json
 * PoP: goal_state_has_contract  @ hermes_cli/goals.py:GoalState.has_contract
 * PoP: goal_state_render_subgoals @ hermes_cli/goals.py:GoalState.render_subgoals_block
 */

#ifndef GOAL_CONTRACT_H
#define GOAL_CONTRACT_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default turn budget for a goal loop (goals.py DEFAULT_MAX_TURNS). */
extern const int GOAL_DEFAULT_MAX_TURNS;

/* ───────────────────────────────────────────────────────────────────
 * GoalContract — structured completion contract (5 free-form fields).
 * ─────────────────────────────────────────────────────────────────── */

typedef struct goal_contract_t goal_contract_t;

/* Allocate a zeroed (empty) contract. Caller frees with goal_contract_free(). */
goal_contract_t *goal_contract_new(void);
void goal_contract_free(goal_contract_t *c);

/* Reset all fields to empty. Safe with NULL. */
void goal_contract_clear(goal_contract_t *c);

/* A contract is empty when every field is blank. */
bool goal_contract_is_empty(const goal_contract_t *c);

/*
 * Field access by canonical name: "outcome", "verification", "constraints",
 * "boundaries", "stop_when". get returns "" for unknown field or NULL c.
 * set ignores unknown field. Values are copied (caller retains ownership
 * of the passed strings).
 */
const char *goal_contract_get(const goal_contract_t *c, const char *field);
void goal_contract_set(goal_contract_t *c, const char *field, const char *value);

/*
 * Populate a contract from a JSON object string {"outcome":..., ...}.
 * Unknown/missing keys are left empty. Returns true on parse success
 * (including an empty/partial object); false only on JSON parse error.
 */
bool goal_contract_from_json(goal_contract_t *c, const char *json);

/*
 * Render non-empty fields as a labelled block:
 *   - Outcome: ...
 *   - Verification: ...
 * Returns a heap string (caller frees). Empty contract -> "" (never NULL).
 */
char *goal_contract_render(const goal_contract_t *c);

/* ───────────────────────────────────────────────────────────────────
 * parse_contract — split user-typed goal text into (headline, contract).
 * ─────────────────────────────────────────────────────────────────── */

/*
 * Parse inline "field: value" lines out of text. Recognized field aliases
 * (e.g. "verify:", "constraints:", "stop when:") populate the contract;
 * all other lines join into the headline. The headline is the goal text
 * the user typed; the contract holds only structured fields.
 *
 * On success: *headline_out is a malloc'd headline (may be ""), *contract_out
 * is a newly allocated goal_contract_t (empty if no field lines). Caller frees
 * both (contract via goal_contract_free). Returns true.
 * Empty/NULL text -> "", empty contract, returns true.
 */
bool parse_contract(const char *text, char **headline_out, goal_contract_t **contract_out);

/* ───────────────────────────────────────────────────────────────────
 * GoalState — serializable per-session goal state.
 * ─────────────────────────────────────────────────────────────────── */

typedef struct goal_state_t goal_state_t;

/* Allocate with default fields (status="active", max_turns=DEFAULT_MAX_TURNS).
 * goal is copied. Caller frees with goal_state_free(). */
goal_state_t *goal_state_new(const char *goal);
void goal_state_free(goal_state_t *s);

/* Serialize to a JSON string (caller frees). Mirrors Python asdict(json). */
char *goal_state_to_json(const goal_state_t *s);

/*
 * Load state from a JSON string produced by goal_state_to_json (also tolerant
 * of older rows missing newer fields). s must be allocated via goal_state_new.
 * Returns true on success, false on JSON parse error.
 */
bool goal_state_from_json(const char *raw, goal_state_t *s);

/* True when the embedded contract exists and is non-empty. */
bool goal_state_has_contract(const goal_state_t *s);

/* Number of user-added subgoals. */
int goal_state_subgoal_count(const goal_state_t *s);

/* Append a non-blank subgoal (trimmed, de-duplicated against existing).
 * Returns 1 if added, 0 if blank/duplicate/no-state. */
int goal_state_add_subgoal(goal_state_t *s, const char *text);

/*
 * Render subgoals as a numbered block ("- 1. text"). Returns "" (never NULL)
 * when there are none. Caller frees.
 */
char *goal_state_render_subgoals(const goal_state_t *s);

/* State key for SessionDB state_meta: "goal:<session_id>".
 * Canonical implementation lives in port_goals_helpers.c (PoP: _meta_key);
 * declared here so data-model + persistence layers share one definition.
 * Caller frees. */
char *goal_meta_key(const char *session_id);

/* ───────────────────────────────────────────────────────────────────
 * GoalState rich accessors (used by GoalManager port).
 * ─────────────────────────────────────────────────────────────────── */

typedef enum { GOAL_STATUS_ACTIVE, GOAL_STATUS_PAUSED, GOAL_STATUS_DONE, GOAL_STATUS_CLEARED } goal_status_t;

/* Set/unset the embedded contract (takes ownership when adopt=true). */
void goal_state_set_contract(goal_state_t *s, goal_contract_t *c);
void goal_state_set_status(goal_state_t *s, goal_status_t st);
void goal_state_set_status_str(goal_state_t *s, const char *status);
goal_status_t goal_state_status(const goal_state_t *s);
const char *goal_state_status_str(const goal_state_t *s);
void goal_state_set_turns_used(goal_state_t *s, int n);
int  goal_state_turns_used(const goal_state_t *s);
int  goal_state_max_turns(const goal_state_t *s);
void goal_state_set_max_turns(goal_state_t *s, int n);
void goal_state_set_paused_reason(goal_state_t *s, const char *r);
const char *goal_state_paused_reason(const goal_state_t *s);
const char *goal_state_goal(const goal_state_t *s);
void goal_state_set_last_verdict(goal_state_t *s, const char *v);
void goal_state_set_last_reason(goal_state_t *s, const char *r);
int  goal_state_parse_failures(const goal_state_t *s);
void goal_state_set_parse_failures(goal_state_t *s, int n);

/* ───────────────────────────────────────────────────────────────────
 * OS liveness primitive (pure POSIX, fail-safe).
 * ─────────────────────────────────────────────────────────────────── */

/* goal_pid_alive(): whether a process with the given pid is alive.
 * (Python: _pid_alive — delegates to a cross-platform liveness check.)
 * Fail-safe: any error / non-positive pid resolves to false so a stale
 * barrier can never wedge the loop. Uses kill(pid, 0) on POSIX. */
bool goal_pid_alive(long pid);

/* goal_session_waiting(): whether a goal parked on a session should stay
 * parked. Delegates to an injected callback (host owns process_registry);
 * fail-safe: NULL callback / empty id => not waiting.
 * (Python: _session_waiting.) */
bool goal_session_waiting(int (*cb)(const char *session_id), const char *session_id);

/* Remove subgoal by 1-based index. Returns malloc'd removed text (caller frees)
 * or NULL if out of range / no state. */
char *goal_state_remove_subgoal(goal_state_t *s, int index_1based);

/* ───────────────────────────────────────────────────────────────────
 * GoalManager — per-session goal orchestration surface (pure data layer).
 * Persistence + process liveness are injected via a vtable so this module
 * stays free of DB / TTY / network / OS-process dependencies.
 * ─────────────────────────────────────────────────────────────────── */

/* Pluggable backend: load/save a goal-state JSON for a session, and report
 * process liveness for the wait-barrier status display. Any callback may be
 * NULL (no-op / treat as dead). */
typedef struct {
    /* load: return malloc'd JSON string for session (or NULL if none). */
    char *(*load)(const char *session_id);
    /* save: persist JSON string for session. Return 0 on success. */
    int   (*save)(const char *session_id, const char *json);
    /* pid_alive: return 1 if the given pid is alive, else 0. NULL => 0. */
    int   (*pid_alive)(long pid);
    /* session_waiting: return 1 if a parked session barrier is still active,
     * else 0. NULL => 0. */
    int   (*session_waiting)(const char *session_id);
} goal_manager_vtab_t;

typedef struct goal_manager_t goal_manager_t;

/* Create a manager for session_id. vtab may be NULL (in-memory only).
 * default_max_turns < 1 => GOAL_DEFAULT_MAX_TURNS. Caller frees via
 * goal_manager_free. */
goal_manager_t *goal_manager_new(const char *session_id, const goal_manager_vtab_t *vtab, int default_max_turns);
void goal_manager_free(goal_manager_t *m);

const char *goal_manager_session_id(const goal_manager_t *m);
goal_state_t *goal_manager_state(goal_manager_t *m);

/* Introspection (mirror GoalManager.is_active/has_goal/has_contract). */
bool goal_manager_is_active(const goal_manager_t *m);
bool goal_manager_has_goal(const goal_manager_t *m);
bool goal_manager_has_contract(const goal_manager_t *m);

/* Printable one-liner for /goal status (mirror GoalManager.status_line).
 * Uses vtab->pid_alive / vtab->session_waiting when present for the park
 * display; absent callbacks treat a set barrier as still parked. Returns
 * malloc'd string (caller frees). */
char *goal_manager_status_line(const goal_manager_t *m);

/* Mutation (mirror GoalManager.set/set_contract/pause/resume/clear/
 * mark_done/add_subgoal/remove_subgoal/clear_subgoals). Each persists via
 * the vtab when present. set/set_contract/add_subgoal/remove_subgoal return
 * the (malloc'd) cleaned text or NULL on no-goal/empty; mark_done/pause/
 * resume/clear return the updated state (or NULL). */
goal_state_t *goal_manager_set(goal_manager_t *m, const char *goal, int max_turns, const goal_contract_t *contract);
goal_state_t *goal_manager_set_contract(goal_manager_t *m, const goal_contract_t *contract);
goal_state_t *goal_manager_pause(goal_manager_t *m, const char *reason);
goal_state_t *goal_manager_resume(goal_manager_t *m, bool reset_budget);
goal_state_t *goal_manager_clear(goal_manager_t *m);
void goal_manager_mark_done(goal_manager_t *m, const char *reason);
char *goal_manager_add_subgoal(goal_manager_t *m, const char *text);
char *goal_manager_remove_subgoal(goal_manager_t *m, int index_1based);
int  goal_manager_clear_subgoals(goal_manager_t *m);

/* /subgoal render helper. Returns malloc'd string (caller frees). */
char *goal_manager_render_subgoals(const goal_manager_t *m);

/* Wait-barrier controls (mirror GoalManager.wait_on/wait_on_session/
 * wait_for_seconds/stop_waiting/is_waiting). now is the current epoch
 * seconds (caller passes time(NULL)). is_waiting() auto-clears a satisfied
 * barrier (lazy), using vtab->pid_alive/session_waiting when present. */
goal_state_t *goal_manager_wait_on(goal_manager_t *m, long pid, const char *reason, double now);
goal_state_t *goal_manager_wait_on_session(goal_manager_t *m, const char *session_id, const char *reason, double now);
goal_state_t *goal_manager_wait_for_seconds(goal_manager_t *m, int seconds, const char *reason, double now);
bool goal_manager_stop_waiting(goal_manager_t *m);
bool goal_manager_is_waiting(goal_manager_t *m, double now);

/* next_continuation_prompt() — the canonical user-role message for the next
 * loop turn. Returns malloc'd string (caller frees) or NULL when not active.
 * Mirrors GoalManager.next_continuation_prompt. */
char *goal_manager_next_continuation_prompt(const goal_manager_t *m);

/* render_contract() — /goal show helper. Returns malloc'd string (caller
 * frees). Mirrors GoalManager.render_contract. */
char *goal_manager_render_contract(const goal_manager_t *m);

/* ───────────────────────────────────────────────────────────────────
 * Judge prompt helpers (pure text shaping — no LLM call).
 * ─────────────────────────────────────────────────────────────────── */

/* goal_judge_max_tokens(): canonical implementation lives in
 * port_goals_helpers.c (reads auxiliary.goal_judge.max_tokens from config,
 * falls back to 4096). Declared here so callers share one definition. */
extern int goal_judge_max_tokens(void);

/* Injectable variant: resolve via the provided callback (used by the judge
 * loop driver / tests). Falls back to 4096 when resolver is NULL/<0.
 * (Python: _goal_judge_max_tokens.) */
int goal_judge_max_tokens_resolved(int (*resolve)(void));

/* _extract_json_object(): best-effort first JSON object from a model reply.
 * Returns malloc'd JSON string (caller frees) or NULL when none. */
char *goal_judge_extract_json_object(const char *raw);

/* _parse_judge_response(): parse a judge reply into (verdict, reason,
 * parse_failed, wait_directive_json). verdict is one of "done"/"continue"/
 * "wait". wait_directive_json is a malloc'd JSON object {"pid":N} |
 * {"seconds":N} | {"session_id":"..."} or NULL. All out-params are written;
 * the caller must free *wait_directive_out when non-NULL. */
void goal_judge_parse_response(const char *raw,
                               char **verdict_out, char **reason_out,
                               bool *parse_failed_out, char **wait_directive_out);

/* _render_background_block(): render running background processes for the
 * judge prompt. procs is a NULL-terminated array of process dicts built by
 * the caller (keys: status, pid, command, uptime_seconds, output_preview,
 * session_id, watch_patterns, watch_hit, notify_on_complete). Returns malloc'd
 * string (caller frees), "" when nothing running. */
typedef struct { const char *key; const char *val; } goal_bg_kv_t; /* unused placeholder */
char *goal_judge_render_background_block(const char *const *json_processes);

#ifdef __cplusplus
}
#endif

#endif /* GOAL_CONTRACT_H */
