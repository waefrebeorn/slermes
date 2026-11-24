/*
 * noc_w27_adapter_e9cb6b_p1.h - C surface for the p1 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p1).
 *
 * Bang-command tokenisation, the inline reply-fallback trio, the permanent
 * auth-error classifier, UTF-8 sizing, the outbound chunk-size clamp, the
 * bare-filename predicates, Matrix timestamp normalisation and log-URL
 * redaction.  No client, no socket, no clock: every symbol is a pure function
 * of the values it is handed.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P1_H
#define NOC_W27_ADAPTER_E9CB6B_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "!name rest" -> {"name","rest"} | null  (^!([A-Za-z][A-Za-z0-9_-]*)(?=$|\s)(.*)$). */
json_t *w27_adapter_bang_split(const json_t *text);

/* name -> [lower, lower with '_'->'-'] deduped in order. */
json_t *w27_adapter_bang_candidates(const json_t *name);

/* body -> [quoted_text|null, author_mxid|null] from "> <@a:b> q". */
json_t *w27_adapter_reply_fallback_extract(const json_t *body);

/* body -> body with the "> quote\n\n" prefix removed (unchanged when absent). */
json_t *w27_adapter_reply_fallback_strip(const json_t *body);

/* body -> [quote_block, reply_text] with quote + reply == body. */
json_t *w27_adapter_reply_fallback_split(const json_t *body);

/* errcode/http_status -> true only for a structured auth failure. */
json_t *w27_adapter_permanent_auth_error(const json_t *errcode, const json_t *http_status);

/* UTF-8 byte length of text. */
json_t *w27_adapter_utf8_len(const json_t *text);

/* configured max_message_length (raw) -> clamp(int(raw), 500, 15000) or 15000. */
json_t *w27_adapter_max_message_length(const json_t *raw);

/* kind "image"|"media" -> bare transport filename predicate. */
json_t *w27_adapter_looks_like_transport_filename(const json_t *text, const char *kind, int reject_spaces);

/* msgtype/body -> body is only the uploaded filename for that msgtype. */
json_t *w27_adapter_is_bare_media_filename(const json_t *msgtype, const json_t *body);

/* origin_server_ts (ms or s) -> seconds. */
json_t *w27_adapter_event_timestamp_seconds(const json_t *ts);

/* signed media URL -> scheme://host/path with query and fragment removed. */
json_t *w27_adapter_redact_url_for_log(const json_t *url);

#ifdef __cplusplus
}
#endif
#endif
