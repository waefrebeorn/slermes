/*
 * elicitation.c -- faithful C11 port of upstream Hermes acp_adapter/elicitation.py.
 *
 * PoP: w9_elicitation_379c63_client_supports_form    @ acp_adapter/elicitation.py:client_supports_form_elicitation
 * PoP: w9_elicitation_379c63_build_clarify_elicitation @ acp_adapter/elicitation.py:build_clarify_elicitation
 * PoP: w9_elicitation_379c63_elicited_answer          @ acp_adapter/elicitation.py:elicited_answer
 *
 * The ACP tool-call construction (build_clarify_permission) is SDK-bound; the three
 * pure helpers above are ported verbatim, including JSON-dumps formatting of a
 * multi-select answer list.
 */
#include "noc_w9_elicitation_379c63.h"
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

int w9_elicitation_379c63_client_supports_form(const json_t *params) {
    if (!params || params->type != JSON_OBJECT) return 0;
    static const char *const KEYS[] = { "clientCapabilities", "capabilities" };
    for (size_t k = 0; k < 2; k++) {
        json_t *caps = json_obj_get(params, KEYS[k]);
        if (!caps || caps->type != JSON_OBJECT) continue;
        json_t *el = json_obj_get(caps, "elicitation");
        if (!el || el->type != JSON_OBJECT) continue;
        json_t *form = json_obj_get(el, "form");
        if (form && form->type != JSON_NULL) return 1;  /* form is not None */
    }
    return 0;
}

json_t *w9_elicitation_379c63_build_clarify_elicitation(const char *session_id, const json_t *prompt) {
    json_t *choices_src = (prompt && prompt->type == JSON_OBJECT) ? json_obj_get(prompt, "choices") : NULL;
    json_t *choices = json_array();
    if (choices_src && choices_src->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(choices_src); i++)
            json_append(choices, json_copy(json_get(choices_src, i)));
    }

    json_t *schema;
    if (json_len(choices) > 0) {
        json_t *sch = json_object();
        json_set(sch, "type", json_string("string"));
        json_set(sch, "enum", json_copy(choices));
        if (prompt && py_truthy(json_obj_get(prompt, "multi_select"))) {
            json_t *wrap = json_object();
            json_set(wrap, "type", json_string("array"));
            json_set(wrap, "items", sch);
            schema = wrap;
        } else {
            schema = sch;
        }
    } else {
        schema = json_object();
        json_set(schema, "type", json_string("string"));
    }

    json_t *props = json_object();
    json_set(props, "answer", schema);
    json_t *req = json_array();
    json_append(req, json_string("answer"));
    json_t *rs = json_object();
    json_set(rs, "type", json_string("object"));
    json_set(rs, "properties", props);
    json_set(rs, "required", req);

    json_t *out = json_object();
    json_set(out, "sessionId", json_string(session_id ? session_id : ""));
    json_set(out, "mode", json_string("form"));
    json_set(out, "message", json_string(prompt ? json_get_str(prompt, "question", "") : ""));
    json_set(out, "requestedSchema", rs);
    json_free(choices);
    return out;
}

/* json.dumps(list_of_strings, ensure_ascii=False): ", "-separated quoted items. */
typedef struct { char *b; size_t len, cap; } sb_t;
static void sb_add(sb_t *s, const char *p, size_t n) {
    if (s->len + n + 1 > s->cap) {
        size_t nc = s->cap * 2 + n + 1;
        char *nb = (char *)realloc(s->b, nc);
        if (!nb) return;
        s->b = nb; s->cap = nc;
    }
    memcpy(s->b + s->len, p, n);
    s->len += n;
    s->b[s->len] = '\0';
}
static char *join_strings(const json_t *arr) {
    size_t n = json_len(arr);
    sb_t s;
    s.cap = 64; s.len = 0; s.b = (char *)malloc(s.cap);
    if (s.b) s.b[0] = '\0';
    sb_add(&s, "[", 1);
    for (size_t i = 0; i < n; i++) {
        if (i) sb_add(&s, ", ", 2);
        char *q = json_dumps(json_get(arr, i), 0);
        if (q) { sb_add(&s, q, strlen(q)); free(q); }
    }
    sb_add(&s, "]", 1);
    return s.b;
}

json_t *w9_elicitation_379c63_elicited_answer(const json_t *response) {
    if (!response || response->type != JSON_OBJECT) return json_null();
    json_t *action = json_obj_get(response, "action");
    if (!action || action->type != JSON_STRING || !action->str_val) return json_null();
    const char *a = action->str_val;
    if (strcmp(a, "accept") && strcmp(a, "decline") && strcmp(a, "cancel")) return json_null();
    if (strcmp(a, "accept") != 0) return json_string("");

    json_t *content = json_obj_get(response, "content");
    json_t *answer = NULL;
    if (content && py_truthy(content) && content->type == JSON_OBJECT)
        answer = json_obj_get(content, "answer");

    if (answer && answer->type == JSON_ARRAY) {
        size_t n = json_len(answer);
        int all_strings = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *e = json_get(answer, i);
            if (!e || e->type != JSON_STRING) { all_strings = 0; break; }
        }
        if (all_strings) {
            char *joined = join_strings(answer);
            json_t *r = json_string(joined ? joined : "");
            free(joined);
            return r;
        }
    }
    if (answer && answer->type == JSON_STRING) return json_copy(answer);
    return json_null();
}
