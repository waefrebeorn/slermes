/*
 * server_requests.c - C home for upstream Hermes tui_gateway/server_requests.py.
 * PoP: w15_serverrequ_cdbd30_can_replay_request @ tui_gateway/server_requests.py:can_replay_request
 * PoP: w15_serverrequ_cdbd30_is_not_shown @ tui_gateway/server_requests.py:_is_not_shown
 * PoP: w15_serverrequ_cdbd30_is_response_frame @ tui_gateway/server_requests.py:is_response_frame
 *
 * Faithful ports of the stateless request-frame predicates. The request registry,
 * blocking waits and transport sinks are live process state and stay with the
 * live Python owner; these helpers are the pure wire-level decisions.
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_w15_serverrequ_cdbd30.h"

/* Mirrors NOT_SHOWN_CODE in the live module. */
#define NOT_SHOWN_CODE 4404

struct json_t *w15_serverrequ_cdbd30_can_replay_request(const char *method,
                                                         const struct json_t *params) {
    int vault_script = method && !strcmp(method, "preview.act") &&
                       !strcmp(json_get_str(params, "action", ""), "vault");
    return json_bool(!vault_script ? 1 : 0);
}

struct json_t *w15_serverrequ_cdbd30_is_not_shown(const struct json_t *frame) {
    json_t *err = json_obj_get(frame, "error");
    json_t *code = err && err->type == JSON_OBJECT ? json_obj_get(err, "code") : NULL;
    /* Strict: Python compares an int code; json_get_num() coercing "4404" would be wrong. */
    int not_shown = code && code->type == JSON_NUMBER &&
                    code->num_val == (double)NOT_SHOWN_CODE;
    return json_bool(not_shown ? 1 : 0);
}

struct json_t *w15_serverrequ_cdbd30_is_response_frame(const struct json_t *obj) {
    int ok = obj && obj->type == JSON_OBJECT &&
             !json_has(obj, "method") && json_has(obj, "id") &&
             (json_has(obj, "result") || json_has(obj, "error"));
    return json_bool(ok ? 1 : 0);
}

