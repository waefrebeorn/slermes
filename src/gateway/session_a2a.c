/*
 * session_a2a.c — faithful C11 port of the pure forward-policy helpers in
 *                  gateway/session_a2a.py.
 *
 * PoP: w7_sessiona2a_da57ab_is_forward_policy @ gateway/session_a2a.py:is_forward_policy
 * PoP: w7_sessiona2a_da57ab_forward_author   @ gateway/session_a2a.py:forward_author
 *
 * Both helpers are pure: they inspect the source/platform labels and the
 * serialized request_json and never touch the store or the network.
 */
#include "noc_w7_sessiona2a_da57ab.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

/* Parse request_json and return its owned tree, or NULL on any error. */
static struct json_t *w7a2a_request(const char *request_json) {
    if (!request_json || !*request_json) return NULL;
    return json_parse(request_json, NULL);
}

int w7_sessiona2a_da57ab_is_forward_policy(const char *source, const char *platform,
        const char *request_json) {
    if (!source || strcmp(source, "a2a") || !platform || strcmp(platform, "a2a")) return 0;
    struct json_t *req = w7a2a_request(request_json);
    if (!req || req->type != JSON_OBJECT) { if (req) json_free(req); return 0; }
    struct json_t *src = json_obj_get(req, "source");
    if (!src || src->type != JSON_STRING || strcmp(src->str_val, "a2a")) {
        json_free(req); return 0;
    }
    struct json_t *ident = json_obj_get(req, "a2a_identity");
    int ok = 0;
    if (ident && ident->type == JSON_ARRAY && ident->c.count == 4) {
        ok = 1;
        for (size_t i = 0; i < 4; i++) {
            if (!ident->c.items[i] || ident->c.items[i]->type != JSON_STRING) { ok = 0; break; }
        }
    }
    json_free(req);
    return ok;
}

struct json_t *w7_sessiona2a_da57ab_forward_author(const char *source, const char *platform,
        const char *request_json) {
    if (!w7_sessiona2a_da57ab_is_forward_policy(source, platform, request_json)) return json_null();
    struct json_t *req = w7a2a_request(request_json);
    struct json_t *ident = json_obj_get(req, "a2a_identity");
    const char *peer = ident->c.items[2]->str_val; /* validated above */
    struct json_t *out = json_object();
    json_set(out, "id", json_string(peer));
    json_set(out, "name", json_string(peer));
    json_set(out, "is_bot", json_bool(true));
    json_free(req);
    return out;
}
