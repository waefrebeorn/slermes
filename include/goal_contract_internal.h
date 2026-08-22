/*
 * goal_contract_internal.h — struct definitions shared by the goals port
 * implementation files (port_goals_data.c, port_goals_manager.c). This is an
 * INTERNAL header (not installed, not for consumers) — public callers use the
 * opaque API in goal_contract.h. Keeping the layout here lets the two
 * implementation files share the exact field layout without leaking it into
 * the public surface.
 */

#ifndef GOAL_CONTRACT_INTERNAL_H
#define GOAL_CONTRACT_INTERNAL_H

#include "goal_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define N_CONTRACT_FIELDS 5

struct goal_contract_t {
    char *fields[N_CONTRACT_FIELDS]; /* each owned, may be NULL/"" */
};

/* goals.py:GoalGate — quality-gate command attached to a goal. */
typedef struct goal_gate_s {
    char *command;
    int timeout_seconds;           /* DEFAULT_GATE_TIMEOUT_SECONDS */
    int max_retries;               /* DEFAULT_GATE_MAX_RETRIES */
    int attempts;                  /* failed runs so far */
    int has_last_exit_code;        /* Python Optional[int] presence flag */
    long last_exit_code;
    char *last_output_tail;        /* may be NULL */
    char *last_failed_fingerprint; /* may be NULL */
} goal_gate_t;

struct goal_state_t {
    char *goal;
    char *status;              /* active | paused | done | cleared */
    int turns_used;
    int max_turns;
    double created_at;
    double last_turn_at;
    char *last_verdict;        /* "done" | "continue" | "skipped" | NULL */
    char *last_reason;         /* NULL */
    char *paused_reason;       /* NULL */
    int consecutive_parse_failures;
    char **subgoals;           /* owned array of strings */
    size_t n_subgoals;
    long waiting_on_pid;       /* 0 => none */
    char *waiting_on_session;  /* NULL */
    double waiting_until;      /* 0.0 => none */
    char *waiting_reason;      /* NULL */
    double waiting_since;
    goal_contract_t *contract; /* owned */
    goal_gate_t **gates;       /* owned array (Python GoalState.gates) */
    size_t n_gates;
};

/* canonical contract field names (index -> name). */
static inline const char *goal_contract_field_name(int i) {
    switch (i) {
        case 0: return "outcome";
        case 1: return "verification";
        case 2: return "constraints";
        case 3: return "boundaries";
        case 4: return "stop_when";
        default: return NULL;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* GOAL_CONTRACT_INTERNAL_H */

/* judge_goal result (goals.py: judge_goal returns the 5-tuple
 * verdict / reason / prompt_fingerprint / wait_directive / transport_failed). */
typedef struct {
    char *verdict;          /* "done" | "continue" | "skipped" */
    char *reason;
    char *wait_directive;   /* park barrier JSON, NULL when not a wait */
    int  parse_failed;      /* consecutive judge-output parse failure */
    int  transport_failed;  /* API/auth/network error vs judge "continue" */
} goal_judge_result_t;

/* Injectable LLM seam for judge_goal. Mirrors Python's auxiliary-client
 * call signature: returns 0 on success (content set), non-zero on transport
 * failure (err = malloc'd error text). */
typedef struct {
    int (*call)(void *ctx, const char *system_prompt, const char *user_prompt,
                double timeout, int max_tokens,
                char **out_content, char **out_err);
    void *ctx;
} goal_judge_llm_vtab_t;

/* goals.py:load_goal/save_goal DB seam — get_meta/set_meta against
 * SessionDB state_meta, same shape as loops_manager_vtab_t. */
typedef struct {
    char *(*get_meta)(void *ctx, const char *key);
    int   (*set_meta)(void *ctx, const char *key, const char *value);
} goal_db_vtab_t;

/* goals.py:DEFAULT_GATE_TIMEOUT_SECONDS / DEFAULT_GATE_MAX_RETRIES */
#define GOALS_DEFAULT_GATE_TIMEOUT_SECONDS 300
#define GOALS_DEFAULT_GATE_MAX_RETRIES 3
