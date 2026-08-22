/*
 * loops_state_internal.h — private layout of loop_state_t, shared only by
 * loops_pure.c (serialization/pure helpers) and loops_manager.c (manager).
 * Not part of the public API: callers use the loops_* functions.
 */
#ifndef SLERMES_LOOPS_STATE_INTERNAL_H
#define SLERMES_LOOPS_STATE_INTERNAL_H

struct loop_state {
    char   *prompt;
    char   *status;            /* active | paused | done | cleared */
    char   *mode;              /* interval | self_paced */
    double  interval_seconds;
    double  current_delay;
    long    times;
    char   *until;
    long    max_ticks;
    long    ticks_fired;
    double  created_at;
    double  last_fired_at;
    double  next_due_at;
    int     awaiting_response;
    char   *last_response_digest;
    char   *paused_reason;
    char   *last_stop_reason;
};

#endif /* SLERMES_LOOPS_STATE_INTERNAL_H */
