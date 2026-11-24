/*
 * noc_w23_sharedmetr_68ac2c.h - C surface for upstream
 * hermes_cli/observability/shared_metrics_prepared_compaction.py
 * (gate w23_sharedmetr_68ac2c).
 *
 * Prepared compaction (``compression.prepare_ahead``): the snapshot state
 * string, the wait-time bucket vocabulary and the closed dimension set for one
 * prepared-compaction event.
 *
 * Seams (injected, never decided here): the profile config dict is handed in
 * as parsed JSON; nothing here reads the environment or the store.
 */
#ifndef NOC_W23_SHAREDMETR_68AC2C_H
#define NOC_W23_SHAREDMETR_68AC2C_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PREPARED_COMPACTION_MARK / PREPARED_COMPACTION_METRIC. */
const char *w23_sharedmetr_68ac2c_metric(void);

/* utils.is_truthy_value(value, default=False), over a parsed JSON value. */
int w23_sharedmetr_68ac2c_truthy(const json_t *value);

/* "on" when config["compression"]["prepare_ahead"] reads truthy, else "off". */
const char *w23_sharedmetr_68ac2c_prepare_ahead_state(const json_t *config);

/* Bucket a wait in seconds: none/lt_1s/1s_to_5s/5s_to_30s/30s_to_120s/gte_120s.
 * A bool, a non-number or a negative number is "none". */
const char *w23_sharedmetr_68ac2c_wait_bucket(const json_t *seconds);

/* req keys: event, origin, reason, failure_class, wait_seconds.
 * Returns the closed dimension object, or JSON null for an event outside the
 * closed set (which the caller never records). */
json_t *w23_sharedmetr_68ac2c_fields(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
