/*
 * session_worker_spawn.c — C home for upstream Hermes gateway/session_worker_spawn.py.
 *
 * PoP: sessworker_acquire_process @ gateway/session_worker_spawn.py:acquire_process
 *
 * The upstream function is a cancellation-safe handoff of a managed subprocess
 * acquired off the owner loop: create() runs in a thread via to_thread, the
 * caller awaits it under shield, and a CancelledError closes the freshly
 * spawned process instead of leaking it.  This port models that decision
 * state machine (see noc_sessworker.h for the exact outcome contract).
 */
#include "noc_sessworker.h"

#include <stddef.h>

sessworker_outcome_t sessworker_acquire_process(const char *process, int cancel) {
    sessworker_outcome_t out;
    out.cancelled      = 0;
    out.returned       = NULL;
    out.closed         = 0;
    out.closed_process = NULL;

    if (!cancel) {
        /* await asyncio.shield(pending) completed: under the lock
         * state['process'] = None, then the process is returned. */
        out.returned = process;
        return out;
    }

    /* CancelledError branch: under the lock state['cancelled'] = True and
     * state['process'] is taken (then cleared).  create() observes the flag
     * and closes the new process; the handler also closes a process that was
     * already stored.  Either way the caller never receives it. */
    out.cancelled = 1;
    out.closed = 1;               /* create(): ManagedWorker(process).close() */
    out.closed_process = process; /* may be NULL when spawn() yielded None */
    return out;
}
