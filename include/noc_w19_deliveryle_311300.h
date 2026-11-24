/* noc_w19_deliveryle_311300.h - public C API for persistence/sqlite/gateway/_delivery_ledger.py. */
#ifndef NOC_W19_DELIVERYLE_311300_H
#define NOC_W19_DELIVERYLE_311300_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_deliveryle_311300_expected_profile @ persistence/sqlite/gateway/_delivery_ledger.py:_expected_profile */
json_t *w19_deliveryle_311300_expected_profile(json_t *profile);
/* PoP: w19_deliveryle_311300_sweep_recoverable @ persistence/sqlite/gateway/_delivery_ledger.py:sweep_recoverable_obligations */
json_t *w19_deliveryle_311300_sweep_recoverable(json_t *rows, double now, json_t *platforms, json_t *targets,
                                                json_t *owner_pid, json_t *owner_started, json_t *dead_owners);
/* PoP: w19_deliveryle_311300_sweep_runtime @ persistence/sqlite/gateway/_delivery_ledger.py:sweep_failed_obligations_for_runtime */
json_t *w19_deliveryle_311300_sweep_runtime(json_t *rows, const char *platform, double now, json_t *profile,
                                            json_t *owner_pid, json_t *owner_started, json_t *due);
/* PoP: w19_deliveryle_311300_runtime_candidates @ persistence/sqlite/gateway/_delivery_ledger.py:list_runtime_retry_candidates */
json_t *w19_deliveryle_311300_runtime_candidates(json_t *rows, const char *platform, json_t *profile,
                                                 json_t *owner_pid, json_t *owner_started);
/* PoP: w19_deliveryle_311300_prune @ persistence/sqlite/gateway/_delivery_ledger.py:_prune_unlocked */
json_t *w19_deliveryle_311300_prune(json_t *rows, double now);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_DELIVERYLE_311300_H */
