#ifndef SLERMES_PORT_COMPACTION_EVENTS_H
#define SLERMES_PORT_COMPACTION_EVENTS_H

#include <hermes_json.h>

/* agent/compaction_events.py — the content-free ``hermes.compaction`` v1
 * payload builders. Every function returns a NEW json_t the caller owns
 * (json_free it), matching the Python dicts. Inputs are borrowed.
 *
 * The schema is a fixed field set; unknown categorical values collapse to a
 * closed-set default ("other"/"unknown"/"none") and never carry message,
 * summary, error or path text. */

/* Closed-set coercion: a string in `allowed`, else `dflt` (NULL -> null).
 * A non-string or empty value is always null. */
json_t *ce_closed(json_t *value, const char *const *allowed, int nallowed,
                  const char *dflt);

/* int(value) with bools rejected; json int or null. */
json_t *ce_int(json_t *value);

/* Closed failure class: "none" when unset, exception:/rollback: keep the
 * prefix, anything outside the known set becomes "other". */
json_t *ce_failure_class(json_t *value);

/* "compaction" only for a committed history/provider rewrite, else
 * "compaction.attempt". */
const char *ce_event_name(json_t *payload);

/* Build the v1 payload from an optional override object. */
json_t *ce_payload(json_t *values);

/* (commit_status, failure_class, history_rewritten) -> outcome. */
json_t *ce_attempt_outcome(json_t *commit_status, json_t *failure,
                           json_t *history_rewritten);

/* trigger_source -> [trigger, trigger_class]. */
json_t *ce_attempt_trigger(json_t *trigger_source);

/* The four record shapes. */
json_t *ce_attempt_payload(json_t *record, json_t *compression_count);
json_t *ce_micro_payload(json_t *record);
json_t *ce_prune_payload(const char *session_id, long tokens_before,
                         long tokens_after, long messages,
                         long tool_results_pruned);
json_t *ce_provider_native_payload(const char *session_id,
                                   json_t *compression_count);

#endif /* SLERMES_PORT_COMPACTION_EVENTS_H */
