/*
 * port_idle_watch.c — Port of tools/bot_desktop/idle_watch.py::next_check_in:
 * whole seconds until the idle-watcher screen could next be due.
 *
 *   runtime.DEFAULT_IDLE_STOP_MINUTES = 30   -> 1800s re-read cadence
 *   _MIN_DELAY_S = 30.0                      -> watcher cadence floor
 *   result = ceil(max(min(_MIN_DELAY_S, limit), limit - idle if idle < limit
 *                     else limit))
 */

#include "port_idle_watch.h"

#include <math.h>

#define DEFAULT_IDLE_STOP_SECS 1800.0 /* 30 * 60 */
#define MIN_DELAY_S 30.0

/* PoP: next_check_in @ tools/bot_desktop/idle_watch.py:next_check_in */
int idw_next_check_in(double limit, int idle_is_null, double idle)
{
    if (limit <= 0.0)
        return (int)DEFAULT_IDLE_STOP_SECS;
    double idl = idle_is_null ? 0.0 : idle;
    double floor_ = fmin(MIN_DELAY_S, limit);
    double tail = (idl < limit) ? (limit - idl) : limit;
    double v = ceil(fmax(floor_, tail));
    return (int)v;
}
