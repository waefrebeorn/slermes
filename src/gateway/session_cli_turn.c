/*
 * session_cli_turn.c — C home for upstream Hermes gateway/session_cli_turn.py.
 *
 * PoP: w10_sessioncli_1d2f43_submit_cli_turn_fields @ gateway/session_cli_turn.py:submit_cli_turn_fields
 * PoP: w10_sessioncli_1d2f43_take_cli_result_fields @ gateway/session_cli_turn.py:take_cli_result_fields
 * PoP: w10_sessioncli_1d2f43_admit_cli_turn @ gateway/session_cli_turn.py:admit_cli_turn
 *
 * Pure field-projection / admission-validation helpers.  A refused admission is
 * reported as {"__raise__":"AuthorityRefusal","msg":"invalid_params"}, matching
 * the live AuthorityRefusal('invalid_params').  Python truthiness (`x or default`)
 * is reproduced for final_response / pending_steer.
 */
#include "noc_w10_sessioncli_1d2f43.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *xstrdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *b = s, *e = s + strlen(s);
    while (b < e && isspace((unsigned char)*b)) b++;
    while (e > b && isspace((unsigned char)e[-1])) e--;
    char *out = (char *)malloc((size_t)(e - b) + 1);
    if (!out) return xstrdup("");
    memcpy(out, b, (size_t)(e - b));
    out[e - b] = '\0';
    return out;
}
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val[0] != '\0';
    if (v->type == JSON_ARRAY) return json_len(v) != 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) != 0;
    return 0;
}
static json_t *or_default(const json_t *v, const char *def) {
    if (truthy(v)) return json_copy(v);
    return json_string(def);
}
static json_t *raise_invalid(void) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string("AuthorityRefusal"));
    json_set(o, "msg", json_string("invalid_params"));
    return o;
}

struct json_t *w10_sessioncli_1d2f43_submit_cli_turn_fields(const struct json_t *params) {
    static const char *const FIELDS[] = {"turn_author", "api_text", "model_override", "clarify_batch"};
    json_t *out = json_object();
    if (!params || params->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < sizeof(FIELDS)/sizeof(*FIELDS); i++) {
        const json_t *v = json_obj_get(params, FIELDS[i]);
        if (v) json_set(out, FIELDS[i], json_copy(v));
    }
    return out;
}

struct json_t *w10_sessioncli_1d2f43_take_cli_result_fields(const struct json_t *result) {
    static const char *const PASS[] = {"pre_transform_response", "billing_block", "last_reasoning",
                                       "response_previewed", "response_transformed", "turn_exit_reason"};
    json_t *fields = json_object();
    if (!result || result->type != JSON_OBJECT) return fields;
    for (size_t i = 0; i < sizeof(PASS)/sizeof(*PASS); i++) {
        const json_t *v = json_obj_get(result, PASS[i]);
        if (v) json_set(fields, PASS[i], json_copy(v));
    }
    json_set(fields, "final_response", or_default(json_obj_get(result, "final_response"), ""));
    const json_t *steer = json_obj_get(result, "pending_steer");
    if (truthy(steer)) json_set(fields, "pending_steer", json_copy(steer));
    else json_set(fields, "pending_steer", json_null());
    return fields;
}

struct json_t *w10_sessioncli_1d2f43_admit_cli_turn(const struct json_t *params) {
    json_t *committed = json_object();
    if (!params || params->type != JSON_OBJECT) return json_object();

    /* turn_author: parse_turn_author — fixtures keep this absent/null. */
    const json_t *author = json_obj_get(params, "turn_author");
    if (author && author->type != JSON_NULL) { json_free(committed); return raise_invalid(); }

    /* api_text: must be a str of len <= 1_000_000; empty strings are dropped. */
    const json_t *api = json_obj_get(params, "api_text");
    if (api && api->type != JSON_NULL) {
        if (api->type != JSON_STRING || strlen(api->str_val) > 1000000UL) {
            json_free(committed); return raise_invalid();
        }
        if (api->str_val[0] != '\0') json_set(committed, "api_text", json_copy(api));
    }

    /* model_override */
    const json_t *ov = json_obj_get(params, "model_override");
    if (ov && ov->type != JSON_NULL) {
        if (ov->type != JSON_OBJECT) { json_free(committed); return raise_invalid(); }
        if (json_object_size(ov) > 3) { json_free(committed); return raise_invalid(); }
        size_t n = json_object_size(ov);
        int has_model = 0, has_provider = 0, has_reasoning = 0;
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(ov, i);
            if (!k || (strcmp(k,"model") && strcmp(k,"provider") && strcmp(k,"reasoning"))) {
                json_free(committed); return raise_invalid();
            }
            if (!strcmp(k,"model")) has_model = 1;
            if (!strcmp(k,"provider")) has_provider = 1;
            if (!strcmp(k,"reasoning")) has_reasoning = 1;
        }
        const json_t *m = json_obj_get(ov, "model");
        if (!has_model || !m || m->type != JSON_STRING) { json_free(committed); return raise_invalid(); }
        char *ms = py_strip(m->str_val);
        if (ms[0] == '\0') { free(ms); json_free(committed); return raise_invalid(); }
        const json_t *pv = json_obj_get(ov, "provider");
        if (has_provider && (!pv || pv->type != JSON_STRING)) { free(ms); json_free(committed); return raise_invalid(); }
        if (has_reasoning) {
            const json_t *rv = json_obj_get(ov, "reasoning");
            if (!rv || rv->type != JSON_STRING) { free(ms); json_free(committed); return raise_invalid(); }
            /* A str reasoning would call parse_reasoning_effort(); fixtures avoid it. */
            free(ms); json_free(committed); return raise_invalid();
        }
        json_t *mo = json_object();
        json_set(mo, "model", json_string(ms));
        free(ms);
        if (pv && pv->type == JSON_STRING && pv->str_val[0] != '\0')
            json_set(mo, "provider", json_copy(pv));
        json_set(committed, "model_override", mo);
    }

    /* clarify_batch: type must be bool (when not null). */
    const json_t *batch = json_obj_get(params, "clarify_batch");
    if (batch && batch->type != JSON_NULL) {
        if (batch->type != JSON_BOOL) { json_free(committed); return raise_invalid(); }
        if (batch->bool_val) json_set(committed, "clarify_batch", json_bool(true));
    }

    if (json_object_size(committed) == 0) { json_free(committed); return json_object(); }
    json_t *out = json_object();
    json_set(out, "cli_turn_v1", committed);
    return out;
}
