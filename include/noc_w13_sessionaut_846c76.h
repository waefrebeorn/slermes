#ifndef NOC_W13_SESSIONAUT_846C76_H
#define NOC_W13_SESSIONAUT_846C76_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_sessionaut_846c76_identities @ gateway/session_automation.py:_identities
 * Parses raw JSON identity strings, keeping those that decode to a non-empty list. */
struct json_t *w13_sessionaut_846c76_identities(const struct json_t *raws);
/* PoP: w13_sessionaut_846c76_queued_tick @ gateway/session_automation.py:_queued_tick
 * row is {"status","principal_id","request_id"}; true for the route's own queued
 * heartbeat tick of process. */
struct json_t *w13_sessionaut_846c76_queued_tick(const struct json_t *row,
                                                  const struct json_t *process,
                                                  const struct json_t *route);
/* PoP: w13_sessionaut_846c76_notification_metadata @ gateway/session_automation.py:automation_notification_metadata
 * Returns {} / {"notification_category":"diagnostic"} / {"error":"invalid_params"}. */
struct json_t *w13_sessionaut_846c76_notification_metadata(const struct json_t *metadata);
/* PoP: w13_sessionaut_846c76_admission_display @ gateway/session_automation.py:admission_display
 * Returns the async-result card (or {}) for an automation admission payload. */
struct json_t *w13_sessionaut_846c76_admission_display(const struct json_t *payload);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONAUT_846C76_H */
