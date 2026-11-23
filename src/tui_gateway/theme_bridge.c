/*
 * theme_bridge.c — C home for tui_gateway/theme_bridge.py.
 * PoP: themeb_theme_request_ex @ tui_gateway/theme_bridge.py:theme_request
 *
 * The live function blocks on the renderer (_ask / server_requests.send) and on
 * time.monotonic().  The _ex seam injects those three hidden inputs explicitly:
 * `answer_json` (the theme.apply reply text), `result` (the theme.pick reply) and
 * `elapsed` seconds.  Applying a theme is a 15 s store write; a picker waits 300 s.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define THEME_PICK_TIMEOUT_S 300.0

static char *strip(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b-1])) b--;
    char *o = (char *)malloc(b - a + 1);
    memcpy(o, s + a, b - a); o[b - a] = 0;
    return o;
}

json_t *themeb_theme_request_ex(const json_t *payload, const char *answer_json,
                                const json_t *result, double elapsed) {
    if (!payload || !json_has(payload, "picker")) {
        if (!answer_json || !*answer_json) {
            json_t *o = json_object();
            json_set(o, "success", json_bool(false));
            json_set(o, "error", json_string("No Hermes Desktop window answered the theme request."));
            return o;
        }
        json_t *parsed = json_parse(answer_json, NULL);
        if (!parsed) {
            json_t *o = json_object();
            json_set(o, "success", json_bool(false));
            json_set(o, "error", json_string("No Hermes Desktop window answered the theme request."));
            return o;
        }
        return parsed;  /* json.loads(answer) */
    }
    /* Picker path */
    if (!result || result->type == JSON_NULL) {
        json_t *o = json_object();
        json_set(o, (elapsed >= THEME_PICK_TIMEOUT_S) ? "timed_out" : "cancelled", json_bool(true));
        return o;
    }
    if (result->type == JSON_OBJECT) {
        json_t *picked = json_obj_get(result, "picked");
        if (picked && picked->type == JSON_OBJECT) {
            json_t *o = json_object();
            json_set(o, "success", json_bool(true));
            json_set(o, "picked", json_copy(picked));
            return o;
        }
        json_t *said = json_obj_get(result, "said");
        if (said && said->type == JSON_STRING) {
            char *t = strip(said->str_val);
            if (t[0]) {
                json_t *o = json_object();
                json_set(o, "said", json_string(t));
                free(t);
                return o;
            }
            free(t);
        }
    }
    json_t *o = json_object();
    json_set(o, "cancelled", json_bool(true));
    return o;
}
