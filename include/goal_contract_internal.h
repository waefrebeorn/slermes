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
