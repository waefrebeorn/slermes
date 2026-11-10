/*
 * port_chat_completion_helpers.h -- pure helpers ported from
 * agent/chat_completion_helpers.py.
 *
 * The asyncio / SDK stream half stays in Python.
 */
#ifndef PORT_CHAT_COMPLETION_HELPERS_H
#define PORT_CHAT_COMPLETION_HELPERS_H

#include <stddef.h>

#include "../lib/libjson/json.h"

/* _status_code_from_value -- `kind` carries the Python type:
 * 0 = not int/str, 1 = int, 2 = str. Returns the code, or -1 for None. */
int cch_status_code_from_value(int kind, long long ival, const char *sval);

/* _status_code_from_payload -- the first parseable status across the
 * status_code / status / http_status keys, then the nested error object, then
 * top-level code. Returns -1 for None. */
int cch_status_code_from_payload(const json_t *payload);

/* _json_object_from_text -- a JSON object parsed from text, or NULL when the
 * text is not an object. Caller frees. */
json_t *cch_json_object_from_text(const char *text);

/* _payload_has_error_shape -- an "error" dict/str, or a message backed by a
 * code / error_code / parseable status. */
int cch_payload_has_error_shape(const json_t *payload);

/* _parse_provider_sse_events -- WHATWG SSE field rules. Returns a malloc'd
 * NULL-terminated array of json_t* events; caller frees each event and the
 * array. *out_count receives the length. */
json_t **cch_parse_provider_sse_events(const char *text, int *out_count);

#endif /* PORT_CHAT_COMPLETION_HELPERS_H */
