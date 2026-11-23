/*
 * prompt_turn.c — faithful C11 port of selected pure helpers in
 * tui_gateway/prompt_turn.py. Gate w10_promptturn_a4cf0e.
 *
 * PoP: w10_promptturn_a4cf0e_result_status @ tui_gateway/prompt_turn.py:_result_status
 * PoP: w10_promptturn_a4cf0e_is_successful_goal_turn @ tui_gateway/prompt_turn.py:_is_successful_goal_turn
 * PoP: w10_promptturn_a4cf0e_turn_outcome @ tui_gateway/prompt_turn.py:_turn_outcome
 */
#include "hermes_json.h"
#include "noc_w10_promptturn_a4cf0e.h"
#include <stdio.h>
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
        default: return 0;
    }
}

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* str() coercion (subset of Python str for JSON values). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupn("None", 4);
    switch (v->type) {
        case JSON_NULL:   return dupn("None", 4);
        case JSON_STRING: return dupn(v->str_val ? v->str_val : "", v->str_val ? strlen(v->str_val) : 0);
        case JSON_BOOL:   return dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupn(buf, strlen(buf));
        default: return dupn("", 0);
    }
}

static char *py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\v'||s[a]=='\f')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\v'||s[b-1]=='\f')) b--;
    return dupn(s + a, b - a);
}

static int starts_with(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

const char *w10_promptturn_a4cf0e_result_status(const json_t *result) {
    if (!result || result->type != JSON_OBJECT) return "complete";
    if (py_truthy(json_obj_get(result, "interrupted"))) return "interrupted";
    if (py_truthy(json_obj_get(result, "error"))) return "error";
    return "complete";
}

int w10_promptturn_a4cf0e_is_successful_goal_turn(const json_t *result, const char *status,
        const json_t *raw, int handoff) {
    if (!status || strcmp(status, "complete")) return 0;
    if (!raw || raw->type != JSON_STRING) return 0;
    char *s = py_strip(raw->str_val ? raw->str_val : "");
    int blank = !*s;
    free(s);
    if (blank) return 0;
    if (!result || result->type != JSON_OBJECT) return 1;
    if (py_truthy(json_obj_get(result, "failed")) || py_truthy(json_obj_get(result, "interrupted"))) return 0;
    const json_t *completed = json_obj_get(result, "completed");
    /* result.get("completed") is not False: only an explicit JSON false is False */
    if (completed && completed->type == JSON_BOOL && !completed->bool_val)
        return handoff ? 1 : 0;
    return 1;
}

json_t *w10_promptturn_a4cf0e_turn_outcome(const json_t *result,
        const struct json_t *error_surface, const char *error_text, const char *interrupt_prefix) {
    (void)error_surface;
    json_t *out = json_array();
    if (!result || result->type != JSON_OBJECT) {
        char *s = py_str(result);
        json_append(out, json_string(s));
        free(s);
        json_append(out, json_string("complete"));
        json_append(out, json_null());
        return out;
    }
    json_t *raw = json_obj_get(result, "final_response");
    json_t *raw_copy = raw ? json_copy(raw) : json_string("");
    const char *status = w10_promptturn_a4cf0e_result_status(result);
    const json_t *err = json_obj_get(result, "error");
    const json_t *failed = json_obj_get(result, "failed");
    const json_t *partial = json_obj_get(result, "partial");
    if (!py_truthy(raw_copy) && py_truthy(err) && (py_truthy(failed) || py_truthy(partial))) {
        json_free(raw_copy);
        raw_copy = json_string(error_text ? error_text : "");
    }
    if (!strcmp(status, "interrupted") && raw_copy->type == JSON_STRING) {
        char *s = py_strip(raw_copy->str_val ? raw_copy->str_val : "");
        int hit = starts_with(s, interrupt_prefix ? interrupt_prefix : "");
        free(s);
        if (hit) { json_free(raw_copy); raw_copy = json_string(""); }
    }
    json_append(out, raw_copy);
    json_append(out, json_string(status));
    json_t *lr = json_obj_get(result, "last_reasoning");
    if (lr && lr->type == JSON_STRING) {
        char *s = py_strip(lr->str_val ? lr->str_val : "");
        if (*s) json_append(out, json_string(s));
        else json_append(out, json_null());
        free(s);
    } else {
        json_append(out, json_null());
    }
    return out;
}
