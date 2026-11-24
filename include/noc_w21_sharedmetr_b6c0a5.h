#ifndef NOC_W21_SHAREDMETR_B6C0A5_H
#define NOC_W21_SHAREDMETR_B6C0A5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/observability/shared_metrics_drift.py (gate w21_sharedmetr_b6c0a5).
 *
 * Packaging re-checks every pending shared-metrics row against the current
 * contract: a shape that matches neither the current nor a legacy field set is
 * tamper evidence (fail closed), while a value that fell out of a closed enum is
 * re-bucketed to the field's fallback bucket. Both decisions are ported, with
 * the contract's closed tables mirrored from the live module and the
 * plugin-catalog-backed enums supplied by the caller.
 */

/* PoP: w21_sharedmetr_b6c0a5_shape_known @ hermes_cli/observability/shared_metrics_drift.py:counter_dimensions_shape_is_known
 * `frozenset(dimensions) == _METRIC_FIELDS.get(metric_name)` or a member of
 * `_LEGACY_METRIC_FIELDS.get(metric_name, ())`. An unknown metric is false. */
json_t *w21_sharedmetr_b6c0a5_shape_known(const json_t *metric_name, const json_t *dimensions);

/* PoP: w21_sharedmetr_b6c0a5_conform @ hermes_cli/observability/shared_metrics_drift.py:conform_counter_dimensions
 * The re-bucketing pass:
 *   {"status":"ok","value":{...}}   every value in contract (or re-bucketed)
 *   {"status":"ok","value":null}    not safely re-bucketable (== Python None)
 *   {"status":"error","type":"KeyError","message":"'<key>'"}  unknown metric/field
 * `catalog` is the packaged plugin-catalog snapshot as
 *   {"<metric>": {"<field>": [values...]}}
 * and stands in for `_CatalogValues.values()`; a field with neither a table set
 * nor a catalog entry has no allowed values. NULL catalog reads as empty. */
json_t *w21_sharedmetr_b6c0a5_conform(const json_t *metric_name, const json_t *dimensions,
                                      const json_t *catalog);

/* PoP: w21_sharedmetr_b6c0a5_metric_identifier @ hermes_cli/observability/shared_metrics_contract.py:_metric_identifier
 * `_metric_identifier(value, max_length=...)`: strip + lower, then the closed
 * character set and length rules. NULL when Python would say "unknown"
 * (non-str, empty, too long, bad leading char, character outside the set).
 * Caller frees. */
char *w21_sharedmetr_b6c0a5_metric_identifier(const char *value, int max_length);

/* The mirrored contract's shape, so the tables can be checked against the live
 * module: {"metrics","metric_field_slots","legacy_sets","static_enum_fields",
 * "catalog_fields","ident_fields","enum_values"}. */
json_t *w21_sharedmetr_b6c0a5_contract_shape(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_SHAREDMETR_B6C0A5_H */
