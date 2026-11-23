/*
 * prompt_refresh.c — C port of upstream Hermes agent/prompt_refresh.py.
 *
 * PoP: w7_promptrefre_f02ec1_session_epoch_key @ agent/prompt_refresh.py:SESSION_EPOCH_KEY
 * PoP: w7_promptrefre_f02ec1_applied_epoch @ agent/prompt_refresh.py:_applied_epoch
 * PoP: w7_promptrefre_f02ec1_requested_epoch @ agent/prompt_refresh.py:requested_epoch
 * PoP: w7_promptrefre_f02ec1_stored_prompt_refresh_epoch @ agent/prompt_refresh.py:stored_prompt_refresh_epoch
 *
 * Agent/DB interactions are injected as JSON seams (spec.reader_present /
 * spec.epoch / spec.session_id) so the pure epoch comparison logic is testable
 * without constructing a live agent or store.
 */
#include "hermes_json.h"
#include "noc_w7_promptrefre_f02ec1.h"
#include <stdlib.h>
#include <string.h>

#define SESSION_EPOCH_KEY "prompt_refresh_epoch"

/* persistence.conversation.sessions.parse_model_config, then .get(KEY). */
static int applied_epoch(const json_t *session_row) {
    if (!session_row || session_row->type != JSON_OBJECT) return 0;
    json_t *raw = json_obj_get(session_row, "model_config");
    json_t *cfg = NULL;
    if (raw && raw->type == JSON_STRING && raw->str_val[0]) {
        char *err = NULL;
        cfg = json_parse(raw->str_val, &err);
        if (!cfg || cfg->type != JSON_OBJECT) { json_free(cfg); return 0; }
    } else if (raw && raw->type == JSON_OBJECT) {
        /* dict(raw) copy is read-only here */
        cfg = (json_t *)raw;
    } else {
        return 0;
    }
    json_t *v = json_obj_get(cfg, SESSION_EPOCH_KEY);
    int out = 0;
    if (v && (v->type == JSON_NUMBER || v->type == JSON_BOOL)) {
        double d = v->type == JSON_BOOL ? (v->bool_val ? 1 : 0) : v->num_val;
        out = (int)d;
    }
    if (cfg != raw) json_free(cfg);
    return out;
}

/* reader is injected: no reader or no session_id -> 0; non-int result -> 0. */
static int requested(const json_t *spec) {
    if (!spec || spec->type != JSON_OBJECT) return 0;
    json_t *rp = json_obj_get(spec, "reader_present");
    int present = rp && rp->type == JSON_BOOL && rp->bool_val;
    const char *sid = json_get_str(spec, "session_id", "");
    if (!present || !sid || !*sid) return 0;
    json_t *v = json_obj_get(spec, "epoch");
    if (v && (v->type == JSON_NUMBER || v->type == JSON_BOOL)) {
        double d = v->type == JSON_BOOL ? (v->bool_val ? 1 : 0) : v->num_val;
        return (int)d;
    }
    return 0;
}

struct json_t *w7_promptrefre_f02ec1_session_epoch_key(void) {
    return json_string(SESSION_EPOCH_KEY);
}
struct json_t *w7_promptrefre_f02ec1_applied_epoch(json_t *session_row) {
    return json_number(applied_epoch(session_row));
}
struct json_t *w7_promptrefre_f02ec1_requested_epoch(json_t *spec) {
    return json_number(requested(spec));
}
struct json_t *w7_promptrefre_f02ec1_stored_prompt_refresh_epoch(json_t *spec) {
    int epoch = requested(spec);
    json_t *row = json_obj_get(spec, "session_row");
    int applied = applied_epoch(row);
    json_t *out = json_object();
    if (epoch && applied < epoch) {
        json_set(out, "ret", json_number(epoch));
        json_set(out, "stamped", json_null());
    } else {
        json_set(out, "ret", json_number(0));
        json_set(out, "stamped", json_number(epoch));
    }
    return out;
}
