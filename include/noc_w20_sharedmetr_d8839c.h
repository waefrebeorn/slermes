#ifndef NOC_W20_SHAREDMETR_D8839C_H
#define NOC_W20_SHAREDMETR_D8839C_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/observability/shared_metrics_subscriber.py (gate
 * w20_sharedmetr_d8839c).
 *
 * Ported: `SharedMetricsSubscriber.__call__` (runtime filter, commit-ticket
 * bookkeeping, deactivate latch), `deactivate`, `take_saved`, `_persist`'s
 * special-metric routing, `_record_milestones` and `_record_features`'s
 * once-per-install latches.
 *
 * Seams the fixture states (they read the store, the clock or the contract):
 * `_classify`, `_record_engagement`, `milestones_for`, `features_for`, the
 * install-age buckets and the store's failures.
 *
 * Request:
 *   runtime_id?, event:{"metadata":...},
 *   classified: [[metric, dimensions, amount], ...],
 *   milestones_for: {metric: [name,...]}, features_for: {metric: [...]},
 *   milestones_done: [...], features_done: [...],
 *   engagement: {"surface":..., "route":[...]}|null,
 *   age: "...", store_errors: [metric,...], milestone_error, feature_error,
 *   active: bool
 * Answer:
 *   {"status":"ok","value":{"filtered":b,"engagement":...|null,
 *     "persisted":[{"kind":...,"metric":...,"dimensions":...,"amount":N}, ...],
 *     "milestones":[...],"features":[...],"tickets":{ticket:N},
 *     "active":b,"stopped":b}}
 */
json_t *w20_sharedmetr_d8839c_call(const json_t *request);

/* PoP: w20_sharedmetr_d8839c_persist_kind @ hermes_cli/observability/shared_metrics_subscriber.py:_persist
 * The store call a counter routes to: "noop" (feature_used), "once_per_day"
 * (feature_disabled), "client_active", "install_snapshot" or "counter". */
const char *w20_sharedmetr_d8839c_persist_kind(const char *metric_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_SHAREDMETR_D8839C_H */
