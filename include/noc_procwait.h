#ifndef NOC_PROCWAIT_H
#define NOC_PROCWAIT_H
/* TERMINAL_TIMEOUT: the default wait and the cap on a requested one. */
int procwait_max_wait_timeout(void);
/* Floor for process_manage wait: returns 1 with *out set, 0 for None. */
int procwait_process_deadline_floor(const char *action, const char *timeout_str,
                                    int timeout_is_null, double *out);
#endif
