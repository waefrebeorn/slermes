#ifndef NOC_SANITIZEID_H
#define NOC_SANITIZEID_H
/* Bind-mountable directory name for a task id (Python semantics).
 * Caller owns the returned malloc'd string. */
char *sanitizeid_sanitize_task_id_for_path(const char *task_id);
#endif
