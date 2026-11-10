#ifndef SLERMES_PORT_SESSION_MANAGED_WORKER_LOG_H
#define SLERMES_PORT_SESSION_MANAGED_WORKER_LOG_H

/* worker_log_path(profile_id): absolute-profile -> "home\\logs\\managed-worker.log"; else NULL.
 * Returns malloc'd string, caller frees. */
#define SMWL_LOG_NAME "managed-worker.log"
char *smwl_worker_log_path(const char *profile);

#endif /* SLERMES_PORT_SESSION_MANAGED_WORKER_LOG_H */
