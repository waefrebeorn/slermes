#ifndef NOC_W13_SCHEDULERB_E4B4E1_H
#define NOC_W13_SCHEDULERB_E4B4E1_H

#include "hermes_json.h"

/* PoP port of cron/scheduler_bookkeeping.py (gate w13_schedulerb_e4b4e1). */
#define W13_SCHEDULERB_E4B4E1_EMPTY_RESPONSE_ERROR \
    "Agent completed but produced empty response (model error, timeout, or misconfiguration)"

/* _classify_delivery_outcome(...) -> str */
json_t *w13_schedulerb_e4b4e1_classify(json_t *delivery_error, json_t *should_deliver,
        json_t *unresolved_origin, json_t *normalized_deliver, json_t *incident_acked,
        json_t *success, json_t *delivery_queued, json_t *notification_suppressed);
/* fail_empty_response(d, final_response) -> {"success":bool, "error":str|null} */
json_t *w13_schedulerb_e4b4e1_fail_empty(json_t *success, json_t *final_response, json_t *error);

#endif /* NOC_W13_SCHEDULERB_E4B4E1_H */
