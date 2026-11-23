#ifndef NOC_SCHEDDIAG_H
#define NOC_SCHEDDIAG_H

/* C port of cron/scheduler_diagnostics.py. All exports scheddiag_-prefixed.
 * Returned char* values are malloc'd and caller-owned (always non-NULL;
 * Python's empty string is represented as ""). */
char *scheddiag_external_worker_stderr_tail(const char *text);
char *scheddiag_format_run_error(const char *traceback_text);
int scheddiag_worker_stderr_tail_chars(void);
#endif
