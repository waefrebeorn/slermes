#ifndef NOC_W9_APIREQUEST_302923_H
#define NOC_W9_APIREQUEST_302923_H

#include <hermes_json.h>

/* C11 ports of selected pure helpers in agent/api_request_hooks.py.
 * Every symbol is prefixed w9_apirequest_302923_ and takes one borrowed
 * json_t *args object (the full fixture line) and returns a NEW json_t *
 * the caller must json_free(). */
json_t *w9_apirequest_302923_hook_payload_max_chars(json_t *a);
json_t *w9_apirequest_302923_is_sensitive_hook_key(json_t *a);
json_t *w9_apirequest_302923_hook_jsonable(json_t *a);
json_t *w9_apirequest_302923_sanitize_hook_payload(json_t *a);
json_t *w9_apirequest_302923_request_payload(json_t *a);

#endif /* NOC_W9_APIREQUEST_302923_H */
