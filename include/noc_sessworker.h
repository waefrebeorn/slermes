#ifndef NOC_SESSWORKER_H
#define NOC_SESSWORKER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: sessworker_acquire_process @ gateway/session_worker_spawn.py:acquire_process
 *
 * Faithful model of the cancellation-safe handoff state machine in
 * acquire_process().  Given a spawned process id and whether the awaiting
 * caller was cancelled before the off-loop create() stored the process:
 *
 *   cancel == 0  -> the caller receives the process; nothing is closed.
 *                   (returned = process, cancelled = 0, closed = 0)
 *   cancel == 1  -> CancelledError is raised.  create() observes the
 *                   cancelled flag and closes the freshly spawned process
 *                   (ManagedWorker(process).close() runs unconditionally
 *                   there, even when spawn yielded None); the caller never
 *                   receives it.
 *                   (returned = NULL, cancelled = 1, closed = 1)
 *
 * The port models the cancel-before-store path: create() is the closer.
 */
typedef struct {
    int         cancelled;       /* 1 when CancelledError is raised */
    const char *returned;        /* process handed to the caller, or NULL  */
    int         closed;          /* 1 when ManagedWorker.close() ran      */
    const char *closed_process;  /* which process was closed, or NULL     */
} sessworker_outcome_t;

sessworker_outcome_t sessworker_acquire_process(const char *process, int cancel);

#ifdef __cplusplus
}
#endif

#endif /* NOC_SESSWORKER_H */
