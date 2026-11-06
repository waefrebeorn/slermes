#ifndef SLERMES_PORT_IDLE_WATCH_H
#define SLERMES_PORT_IDLE_WATCH_H

/* next_check_in(limit, idle): whole seconds until the idle-watcher screen could
 * next be due (see the .c for the clamp). idle_is_null distinguishes idle=None
 * (treated as 0.0) per the live Python. */
int idw_next_check_in(double limit, int idle_is_null, double idle);

#endif /* SLERMES_PORT_IDLE_WATCH_H */
