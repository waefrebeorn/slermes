/* noc_w18_runtimedow_f303ab.h — public C API for gateway/runtime_downgrade_exit.py port. */
#ifndef NOC_W18_RUNTIMEDOW_F303AB_H
#define NOC_W18_RUNTIMEDOW_F303AB_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

#define W18_RUNTIMEDOW_POLL_S 30.0
#define W18_RUNTIMEDOW_CONFIRMATIONS 2
#define W18_RUNTIMEDOW_SERVICE_RESTART_EXIT_CODE 75
#define W18_RUNTIMEDOW_MARKER "CREATE TABLE IF NOT EXISTS session_admissions"

/* PoP: w18_runtimedow_f303ab_downgrade_reason @ gateway/runtime_downgrade_exit.py:downgrade_reason */
json_t *w18_runtimedow_f303ab_downgrade_reason(int has_skew, json_t *skew,
                                              int update_in_progress, int predates);
/* PoP: w18_runtimedow_f303ab_tree_predates @ gateway/runtime_downgrade_exit.py:tree_predates_runtime */
json_t *w18_runtimedow_f303ab_tree_predates(json_t *sources);
/* PoP: w18_runtimedow_f303ab_watcher @ gateway/runtime_downgrade_exit.py:runtime_downgrade_watcher */
json_t *w18_runtimedow_f303ab_watcher(int running, int max_polls, json_t *polls);
/* PoP: w18_runtimedow_f303ab_constants @ gateway/runtime_downgrade_exit.py:<module constants> */
json_t *w18_runtimedow_f303ab_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_RUNTIMEDOW_F303AB_H */
