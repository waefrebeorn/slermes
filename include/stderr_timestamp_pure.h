#ifndef HERMES_STDERR_TIMESTAMP_PURE_H
#define HERMES_STDERR_TIMESTAMP_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/stderr_timestamp.py.
 * argv helpers take a NULL-terminated argv array (argc entries). */

/* "%Y-%m-%d %H:%M:%S,mmm" (uses clock_gettime; not oracle-compared). */
void stderr_ts_timestamp(char *buf, size_t cap);

/* ^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2},\d{3}(?:\s|$) */
int stderr_ts_has_timestamp_prefix(const char *line);

/* negative rc -> 128 + |rc| */
int stderr_ts_command_exit_code(int returncode);

/* XPC_SERVICE_NAME set and != "0" */
int stderr_ts_is_launchd_supervised(const char *xpc_service_name);

/* delegated to gateway_status.looks_like_gateway_command_line */
int stderr_ts_is_gateway_run_argv(char **argv, int argc);

/* appends --external-supervisor if absent */
void stderr_ts_with_external_supervisor_flag(char **argv, int argc,
                                              char **out, int *outc);

/* upgrade stale launchd-wrapped gateway argv */
void stderr_ts_prepare_child_command(char **argv, int argc,
                                     const char *xpc_service_name,
                                     char **out, int *outc);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_STDERR_TIMESTAMP_PURE_H */
