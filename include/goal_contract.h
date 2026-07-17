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

/* Append a non-blank subgoal (trimmed, de-duplicated against existing). */
void goal_state_add_subgoal(goal_state_t *s, const char *text);

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

#ifdef __cplusplus
}
#endif

#endif /* GOAL_CONTRACT_H */
