#ifndef NOC_W7_SCHEDULERTH_11060E_H
#define NOC_W7_SCHEDULERTH_11060E_H
#include "hermes_json.h"
#include <stdbool.h>

/* C home for upstream Hermes cron/scheduler_thread.py (gate w7_schedulerth_11060e). */

/* PoP: w7_schedulerth_restart_if_dead @ cron/scheduler_thread.py:SupervisedTickerThread.restart_if_dead
 * Returns [restarted(bool), restarts_after(int)] as a JSON array.
 * Python: if stop_event.is_set() or thread.is_alive() or not crashed: return False
 *         else restarts += 1; spawn + start; return True. */
void w7_schedulerth_restart_if_dead(bool stop_event_set, bool thread_alive, bool crashed,
                                    long long restarts_in, bool *restarted,
                                    long long *restarts_out);

#endif /* NOC_W7_SCHEDULERTH_11060E_H */
