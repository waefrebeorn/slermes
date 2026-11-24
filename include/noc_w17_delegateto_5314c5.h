#ifndef NOC_W17_DELEGATETO_5314C5_H
#define NOC_W17_DELEGATETO_5314C5_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/delegate_tool_child_run.py.
 * Pure per-run helpers, result-entry assembly and heartbeat constants. */

json_t *w17_delegateto_5314c5_num(const json_t *value, const json_t *def);
json_t *w17_delegateto_5314c5_str_or_none(const json_t *value);
json_t *w17_delegateto_5314c5_is_image_url(const char *ref);
json_t *w17_delegateto_5314c5_budget_warning_text(double idle_seconds, double child_timeout);
json_t *w17_delegateto_5314c5_fabricated_entry(int idx, const char *status, const json_t *error,
                                              const json_t *child, double duration);
json_t *w17_delegateto_5314c5_append_missed_steer(const json_t *entry, const json_t *late_steer);
json_t *w17_delegateto_5314c5_tier_in_effect(const json_t *child);
json_t *w17_delegateto_5314c5_constants(void);

#endif /* NOC_W17_DELEGATETO_5314C5_H */
