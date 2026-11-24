#ifndef NOC_W18_NOTIFICATIO_E552E1_H
#define NOC_W18_NOTIFICATIO_E552E1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/notification_park.py (gate w18_notificatio_e552e1).
 * Pure predicates over a parked event plus the bounded-capacity admission
 * decision (park state supplied by the caller, not process-global). */

/* PoP: w18_notificatio_e552e1_parkable @ tui_gateway/notification_park.py:_parkable */
json_t *w18_notificatio_e552e1_parkable(const json_t *evt);
/* PoP: w18_notificatio_e552e1_is_progress @ tui_gateway/notification_park.py:_is_progress */
json_t *w18_notificatio_e552e1_is_progress(const json_t *evt);
/* PoP: w18_notificatio_e552e1_key @ tui_gateway/notification_park.py:_key */
json_t *w18_notificatio_e552e1_key(const json_t *evt);
/* PoP: w18_notificatio_e552e1_admit_locked @ tui_gateway/notification_park.py:_admit_locked
 * state: JSON array of [deadline, evt] rows; mirrors MAX_PARKED / MAX_PARKED_PER_KEY.
 * Returns {"admitted":bool,"displaced":[evt,...]}. */
json_t *w18_notificatio_e552e1_admit_locked(const json_t *state, const json_t *evt, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_NOTIFICATIO_E552E1_H */
