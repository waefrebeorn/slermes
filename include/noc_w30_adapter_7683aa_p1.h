/*
 * noc_w30_adapter_7683aa_p1.h - C surface for upstream
 * plugins/platforms/mattermost/adapter.py (gate w30_adapter_7683aa_p1).
 *
 * The outbound half of the Mattermost adapter: the value->value helpers that
 * sit between the gateway and the REST calls.  Every network seam (aiohttp
 * session, the API response body, the WAL of the WebSocket) stays in the
 * caller; this header owns only what the values decide.
 *
 * PoP: w30_mm_p1_format_message            @ adapter.py:MattermostAdapter.format_message
 * PoP: w30_mm_p1_with_mentions_disabled    @ adapter.py:_with_mentions_disabled
 * PoP: w30_mm_p1_channel_id_set            @ adapter.py:_channel_id_set
 * PoP: w30_mm_p1_post_result               @ adapter.py:_post_result
 * PoP: w30_mm_p1_url_filename              @ adapter.py:_url_filename
 * PoP: w30_mm_p1_channel_info              @ adapter.py:MattermostAdapter.get_chat_info
 * PoP: w30_mm_p1_api_request_url           @ adapter.py:MattermostAdapter._api
 * PoP: w30_mm_p1_auth_header               @ adapter.py:MattermostAdapter._auth_header
 * PoP: w30_mm_p1_request_headers           @ adapter.py:MattermostAdapter._headers
 * PoP: w30_mm_p1_broken_thread_root        @ adapter.py:MattermostAdapter._last_post_failure_is_broken_thread_root
 * PoP: w30_mm_p1_reconnect_step            @ adapter.py:MattermostAdapter._ws_loop
 * PoP: w30_mm_p1_url_and_token             @ adapter.py:_url_and_token
 * PoP: w30_mm_p1_validate_config           @ adapter.py:validate_mattermost_config
 * PoP: w30_mm_p1_constants                 @ adapter.py:<module constants>
 */
#ifndef NOC_W30_ADAPTER_7683AA_P1_H
#define NOC_W30_ADAPTER_7683AA_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "![alt](url)" -> "url" (Mattermost renders the bare URL as a preview). */
json_t *w30_mm_p1_format_message(const char *content);

/* payload -> the same payload with props.disable_mentions = true. */
json_t *w30_mm_p1_with_mentions_disabled(const json_t *payload);

/* list-of-ids or "a,b,c" -> sorted array of stripped, non-empty strings. */
json_t *w30_mm_p1_channel_id_set(const json_t *raw);

/* {"success","message_id","error"} from an API post body + the error label. */
json_t *w30_mm_p1_post_result(const json_t *data, const char *error);

/* Last path segment of url, query stripped, or fallback when that is empty. */
json_t *w30_mm_p1_url_filename(const char *url, const char *fallback);

/* {"name","type"} for get_chat_info(api_data, chat_id). */
json_t *w30_mm_p1_channel_info(const json_t *data, const char *chat_id);

/* {"blocked","url"}: {method} {base}/api/v4/{path}, ".." refused. */
json_t *w30_mm_p1_api_request_url(const char *base_url, const char *path);

/* {"Authorization": "Bearer <token>"} */
json_t *w30_mm_p1_auth_header(const char *token);

/* auth header + Content-Type: application/json */
json_t *w30_mm_p1_request_headers(const char *token);

/* (status, error body) -> True only for a clear invalid/missing thread root. */
json_t *w30_mm_p1_broken_thread_root(const json_t *status, const char *error);

/* {"sleep","next"}: the WebSocket reconnect backoff step for one attempt. */
json_t *w30_mm_p1_reconnect_step(const json_t *delay, const json_t *rnd);

/* config -> {"url","token"} (config first, MATTERMOST_* env fallback). */
json_t *w30_mm_p1_url_and_token(const json_t *config);

/* config -> True when both the token and the URL are non-blank. */
json_t *w30_mm_p1_validate_config(const json_t *config);

/* The module/class constants this adapter is tuned with, as an object. */
json_t *w30_mm_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_ADAPTER_7683AA_P1_H */
