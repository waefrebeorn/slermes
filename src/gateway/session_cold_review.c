/*
 * session_cold_review.c - C port of upstream Hermes gateway/session_cold_review.py
 *
 * PoP: w14_sessioncol_bf4efd_stored_transcript @ gateway/session_cold_review.py:stored_transcript
 * PoP: w14_sessioncol_bf4efd_is_conversational @ gateway/session_cold_review.py:stored_transcript
 *
 * The read-only stored-transcript gate for a cold /review: no session is created;
 * a chat with no user/assistant turn answers an empty transcript.
 */
#include "noc_w14_sessioncol_bf4efd.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int conversational(const json_t *history) {
    if (!json_is_array(history)) return 0;
    for (size_t i = 0; i < json_len(history); i++) {
        const json_t *m = json_get(history, i);
        const json_t *r = json_is_object(m) ? json_obj_get(m, "role") : NULL;
        if (r && r->type == JSON_STRING && r->str_val &&
            (!strcmp(r->str_val, "user") || !strcmp(r->str_val, "assistant"))) return 1;
    }
    return 0;
}

json_t *w14_sessioncol_bf4efd_stored_transcript(const struct json_t *state) {
    json_t *out = json_object();
    const json_t *sid = json_obj_get(state, "session_id");
    const json_t *hist = json_obj_get(state, "history");
    json_set(out, "session_id", (sid && sid->type == JSON_STRING) ? json_copy(sid) : json_null());
    if (sid && sid->type == JSON_STRING && sid->str_val && *sid->str_val && conversational(hist))
        json_set(out, "history", json_copy(hist));
    else
        json_set(out, "history", json_array());
    return out;
}

json_t *w14_sessioncol_bf4efd_is_conversational(const struct json_t *state) {
    return json_bool(conversational(json_obj_get(state, "history")));
}
