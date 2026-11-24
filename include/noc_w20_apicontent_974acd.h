#ifndef NOC_W20_APICONTENT_974ACD_H
#define NOC_W20_APICONTENT_974ACD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/api_content.py (gate
 * w20_apicontent_974acd).
 *
 * Both writers are a single rowcount-returning UPDATE. The C side reproduces
 * the exact statement text, the parameter tuple (sidecar bytes scrubbed, the
 * content encoded through _encode_content) and the row-id guard, and returns
 * them as a plan; the caller binds them to a real holder.
 */

/* PoP: w20_apicontent_974acd_latest_sql @ persistence/sqlite/conversation/_store/api_content.py:set_latest_user_api_content
 * The statement text verbatim (newest ACTIVE user row of the session). */
const char *w20_apicontent_974acd_latest_sql(void);

/* PoP: w20_apicontent_974acd_row_sql @ persistence/sqlite/conversation/_store/api_content.py:set_message_api_content
 * The statement text verbatim (one durable row id). */
const char *w20_apicontent_974acd_row_sql(void);

/* PoP: w20_apicontent_974acd_encode_content @ persistence/sqlite/conversation/_store/messages.py:_encode_content
 * Sentinel-prefixed JSON for structured content, scalars unchanged, a literal
 * string already carrying the reserved "\x00json:" prefix JSON-string-wrapped. */
json_t *w20_apicontent_974acd_encode_content(const json_t *content);

/* PoP: w20_apicontent_974acd_plan_latest @ persistence/sqlite/conversation/_store/api_content.py:set_latest_user_api_content
 * {"sql":..., "params":[api_content, session_id, encoded content], "rowcount":N}
 * where N is the rowcount the holder's write reported. */
json_t *w20_apicontent_974acd_plan_latest(const char *session_id, const json_t *content,
                                          const char *api_content, int rowcount);

/* PoP: w20_apicontent_974acd_plan_row @ persistence/sqlite/conversation/_store/api_content.py:set_message_api_content
 * The row-addressed variant. A missing/blank session_id, a bool row_id, a
 * non-int row_id or row_id <= 0 answer {"rowcount":0} without touching the
 * store (the guard runs before the UPDATE is built). */
json_t *w20_apicontent_974acd_plan_row(const char *session_id, const json_t *row_id,
                                       const json_t *content, const char *api_content,
                                       int rowcount);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_APICONTENT_974ACD_H */
