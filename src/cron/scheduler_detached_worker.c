/*
 * scheduler_detached_worker.c — C home for cron/scheduler_detached_worker.py.
 * Faithful port of the three teardown helpers. The collaborator objects (Future,
 * store handle, Popen, Thread) are represented by their decision inputs; all
 * branch logic, ordering and names mirror the live Python exactly.
 * PoP: w18_schedulerd_20df75_close_late_result @ cron/scheduler_detached_worker.py:_close_late_session_db_result
 * PoP: w18_schedulerd_20df75_defer_teardown @ cron/scheduler_detached_worker.py:defer_teardown_to_running_worker
 * PoP: w18_schedulerd_20df75_reap_background @ cron/scheduler_detached_worker.py:reap_terminal_worker_in_background
 */
#include "noc_w18_schedulerd_20df75.h"
#include <stdio.h>

/* with contextlib.suppress(Exception): handle = future.result();
 * if handle is not None: handle.close() */
json_t *w18_schedulerd_20df75_close_late_result(int result_raises, int has_handle) {
    int close_called = (!result_raises && has_handle) ? 1 : 0;
    return json_bool(close_called);
}

/* if future is None or future.done(): return False.  Otherwise add_done_callback(_finish)
 * and return True.  _finish: try: if session_db: finalize; finally: teardown. */
json_t *w18_schedulerd_20df75_defer_teardown(int future_none, int future_done,
                                            int session_db_truthy, int invoke_callback) {
    json_t *out = json_object();
    int ret = (!future_none && !future_done) ? 1 : 0;
    json_set(out, "ret", json_bool(ret));
    json_set(out, "callback_added", json_bool(ret));
    json_t *trace = json_array();
    if (ret && invoke_callback) {
        if (session_db_truthy) json_append(trace, json_string("finalize"));
        json_append(trace, json_string("teardown"));
    }
    json_set(out, "trace", trace);
    return out;
}

/* threading.Thread(target=process.wait, name=f"cron-worker-reap-{getattr(process,'pid','?')}",
 *                   daemon=True).start() */
json_t *w18_schedulerd_20df75_reap_background(int has_pid, const char *pid) {
    char name[160];
    snprintf(name, sizeof(name), "cron-worker-reap-%s", (has_pid && pid) ? pid : "?");
    json_t *out = json_object();
    json_set(out, "thread_name", json_string(name));
    json_set(out, "daemon", json_bool(1));
    json_set(out, "wait_is_target", json_bool(1));
    json_set(out, "started", json_bool(1));
    return out;
}
