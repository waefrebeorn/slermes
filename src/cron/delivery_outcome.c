/* delivery_outcome.c — C home for upstream Hermes cron/delivery_outcome.py.
 *
 * PoP: w7_deliveryout_terminal @ cron/delivery_outcome.py:_terminal
 * PoP: w7_deliveryout_settled_outcome @ cron/delivery_outcome.py:settled_outcome
 * PoP: w7_deliveryout_settle_queued @ cron/delivery_outcome.py:settle_queued_delivery
 * The queue row is injected by the fixture; only the outcome derivation is ported.
 */
#include "noc_w7_deliveryout_305ac3.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

/* _LEDGER_OUTCOME = {"delivered":"delivered","suppressed":"suppressed",
 *                    "failed":"failed","unknown":"failed"} */
static const char *ledger_outcome_for(const char *status) {
    if (!status) return NULL;
    if (!strcmp(status, "delivered"))  return "delivered";
    if (!strcmp(status, "suppressed")) return "suppressed";
    if (!strcmp(status, "failed"))     return "failed";
    if (!strcmp(status, "unknown"))    return "failed"; /* unknown -> failed */
    return NULL;
}

json_t *w7_deliveryout_terminal(const char *status, bool has_error, const char *error) {
    const char *outcome = ledger_outcome_for(status);
    if (!outcome) return json_null();
    json_t *err = json_null();
    if (status && (!strcmp(status, "failed") || !strcmp(status, "unknown"))) {
        if (has_error && error && error[0]) {
            err = json_string(error);
        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "delivery %s", status);
            err = json_string(buf);
        }
    }
    json_t *a = json_array();
    json_append(a, json_string(outcome));
    json_append(a, err);
    return a;
}

json_t *w7_deliveryout_settled_outcome(const char *status, bool has_error, const char *error) {
    json_t *t = w7_deliveryout_terminal(status, has_error, error);
    if (!t || t->type != JSON_ARRAY || t->c.count == 0) { json_free(t); return json_null(); }
    json_t *first = json_copy(json_get(t, 0));
    json_free(t);
    return first;
}

bool w7_deliveryout_settle_queued(bool has_job_id, const char *job_id,
                                  bool has_execution_id, const char *execution_id,
                                  const char *status, bool has_error, const char *error) {
    if (!has_job_id || !job_id || !job_id[0]) return false;
    if (!has_execution_id || !execution_id || !execution_id[0]) return false;
    json_t *t = w7_deliveryout_terminal(status, has_error, error);
    bool terminal = (t && t->type == JSON_ARRAY && t->c.count > 0);
    json_free(t);
    return terminal;
}
