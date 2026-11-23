/* scheduler_thread.c — C home for upstream Hermes cron/scheduler_thread.py.
 *
 * PoP: w7_schedulerth_restart_if_dead @ cron/scheduler_thread.py:SupervisedTickerThread.restart_if_dead
 * The supervisor's only branch is the crash-vs-shutdown decision; thread/loop mechanics
 * stay in the host runtime. Crashed==True means the target escaped with an exception,
 * which is the only state worth respawning (an external provider's start() returning
 * is normal and leaves _crashed False).
 */
#include "noc_w7_schedulerth_11060e.h"
#include <stdbool.h>

void w7_schedulerth_restart_if_dead(bool stop_event_set, bool thread_alive, bool crashed,
                                    long long restarts_in, bool *restarted,
                                    long long *restarts_out) {
    if (restarts_out) *restarts_out = restarts_in;
    if (restarted) *restarted = false;
    if (stop_event_set || thread_alive || !crashed) return;
    if (restarts_out) *restarts_out = restarts_in + 1;
    if (restarted) *restarted = true;
}
