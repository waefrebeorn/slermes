/* noc_w18_schedulerd_20df75.h — public C API for cron/scheduler_detached_worker.py port. */
#ifndef NOC_W18_SCHEDULERD_20DF75_H
#define NOC_W18_SCHEDULERD_20DF75_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_schedulerd_20df75_close_late_result @ cron/scheduler_detached_worker.py:_close_late_session_db_result */
json_t *w18_schedulerd_20df75_close_late_result(int result_raises, int has_handle);
/* PoP: w18_schedulerd_20df75_defer_teardown @ cron/scheduler_detached_worker.py:defer_teardown_to_running_worker */
json_t *w18_schedulerd_20df75_defer_teardown(int future_none, int future_done,
                                            int session_db_truthy, int invoke_callback);
/* PoP: w18_schedulerd_20df75_reap_background @ cron/scheduler_detached_worker.py:reap_terminal_worker_in_background */
json_t *w18_schedulerd_20df75_reap_background(int has_pid, const char *pid);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SCHEDULERD_20DF75_H */
