#ifndef TODO_HYDRATE_H
#define TODO_HYDRATE_H

#include <stddef.h>

/* Forward declaration — the concrete struct is agent_state_t. */
struct agent_state_t;

/*
 * todo_hydrate.h — Port of Python run_agent.AIAgent._hydrate_todo_store
 *
 * Scans session messages for the most recent todo tool response and replays
 * it to reconstruct in-memory todo state on session resume.
 */

/*
 * Scan context messages for the last todo tool response and replay it into the
 * store. Returns the number of items restored, or 0 if none found / error.
 */
int todo_hydrate_from_context(void *state);

#endif /* TODO_HYDRATE_H */
