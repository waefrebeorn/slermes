/* noc_w19_processes_bd7796.h - public C API for hermes_cli/local_runtime/processes.py port. */
#ifndef NOC_W19_PROCESSES_BD7796_H
#define NOC_W19_PROCESSES_BD7796_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_processes_bd7796_server_child_env @ hermes_cli/local_runtime/processes.py:server_child_env
 * The environment a native inference child gets: every base key whose upper-case form holds
 * none of _API_KEY / _TOKEN / _SECRET / PASSWORD / _CREDENTIALS, values untouched. */
json_t *w19_processes_bd7796_server_child_env(json_t *base_env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PROCESSES_BD7796_H */
