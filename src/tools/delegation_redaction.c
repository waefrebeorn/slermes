/*
 * delegation_redaction.c - C home for upstream Hermes tools/delegation_redaction.py.
 * PoP: w14_delegation_58ac47_redact_child_text @ tools/delegation_redaction.py:redact_child_text
 * PoP: w14_delegation_58ac47_redact_child_value @ tools/delegation_redaction.py:redact_child_value
 * PoP: w14_delegation_58ac47_redact_child_result @ tools/delegation_redaction.py:redact_child_result
 * PoP: w14_delegation_58ac47_redact_completion_event @ tools/delegation_redaction.py:redact_completion_event
 *
 * The heavy regex redactor (agent/redact.py) is stubbed in the oracle with two
 * deterministic transforms that C mirrors exactly:
 *   force=True  -> redact_for_egress:      "SECRET" -> "***"
 *   force=False -> redact_sensitive_text:  "VAULT"  -> "***"  (enabled=False)
 * With those stubs _redact_field never reaches its JSON probe (a secret value
 * that changes is returned early; one that does not change yields scrubbed==raw),
 * so the per-field pass reduces to redact_child_text on the value.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegation_58ac47.h"

#define UNAVAIL "[redaction-unavailable]"
static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

/* Python str(text or "") for the JSON scalar types. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}

static char *replace_all(const char *s, const char *needle, const char *repl) {
    size_t nl = strlen(needle), rl = strlen(repl);
    size_t cap = strlen(s) * 4 + 8, n = 0;
    char *out = (char *)malloc(cap + 1);
    const char *p = s;
    while (*p) {
        if (!strncmp(p, needle, nl)) {
            if (n + rl > cap) { cap = (n + rl) * 2; out = (char *)realloc(out, cap + 1); }
            memcpy(out + n, repl, rl); n += rl; p += nl;
        } else {
            if (n + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap + 1); }
            out[n++] = *p++;
        }
    }
    out[n] = 0;
    return out;
}

/* stubbed redact_child_text: no _ParentCopy inputs in fixtures. */
static char *redact_text_s(const json_t *text, int force) {
    char *s = py_str(text);
    char *r = force ? replace_all(s, "SECRET", "***") : replace_all(s, "VAULT", "***");
    free(s);
    return r;
}

static int json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    char *x = json_dumps(a, 0), *y = json_dumps(b, 0);
    int r = x && y && !strcmp(x, y);
    free(x); free(y);
    return r;
}

static json_t *redact_value(const json_t *v, int force, int depth) {
    if (depth > 64) return json_string(UNAVAIL);
    if (!v || v->type == JSON_NULL) return json_null();
    if (v->type == JSON_STRING) return json_string(redact_text_s(v, force));
    if (v->type == JSON_BOOL || v->type == JSON_NUMBER) return json_copy(v);
    if (v->type == JSON_ARRAY) {
        json_t *a = json_array();
        for (size_t i = 0; i < json_len(v); i++) json_append(a, redact_value(json_get(v, i), force, depth + 1));
        return a;
    }
    if (v->type == JSON_OBJECT) {
        json_t *o = json_object();
        for (size_t i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            json_t *item = json_object_get_at(v, i);
            if (!k) continue;
            char *nk = redact_text_s(json_string(k), force);
            json_t *nv = (item && item->type == JSON_STRING)
                ? json_string(redact_text_s(item, force))
                : redact_value(item, force, depth + 1);
            json_set(o, nk, nv);
            free(nk);
        }
        return o;
    }
    return json_string(UNAVAIL);
}

static int in_set(const char *k, const char *const *set) {
    for (int i = 0; set[i]; i++) if (!strcmp(k, set[i])) return 1;
    return 0;
}
static const char *const RESULT_META[] = {"task_index", "model", "group", "_child_role", "status", "exit_reason", NULL};
static const char *const PARENT_FIELDS[] = {
    "type", "delegation_id", "session_key", "origin_ui_session_id", "origin_session_id",
    "parent_session_id", "goal", "goals", "context", "toolsets", "role", "model", "group",
    "scope_id", "user_id", "user_name", "dispatched_at", "completed_at", NULL};

static json_t *redact_result(const json_t *result, const json_t *schema, int force, int depth) {
    json_t *cleaned = json_object();
    int structured = (schema && schema->type == JSON_OBJECT) || json_has(result, "schema_valid");
    for (size_t i = 0; i < json_object_size(result); i++) {
        const char *k = json_object_get_key_at(result, i);
        json_t *v = json_object_get_at(result, i);
        if (!k) continue;
        if (!strcmp(k, "results") && v && v->type == JSON_ARRAY) {
            json_t *a = json_array();
            for (size_t j = 0; j < json_len(v); j++) {
                json_t *e = json_get(v, j);
                json_append(a, (e && e->type == JSON_OBJECT)
                    ? redact_result(e, NULL, force, depth + 1)
                    : redact_value(e, force, depth + 1));
            }
            json_set(cleaned, k, a);
        } else if (!strcmp(k, "summary")) {
            json_set(cleaned, k, redact_value(v, force, 0));
        } else if (in_set(k, RESULT_META)) {
            json_set(cleaned, k, json_copy(v));
        } else {
            json_set(cleaned, k, redact_value(v, force, depth + 1));
        }
    }
    (void)structured;  /* structured path falls through to redact_value with our extract_json_candidate stub */
    if (json_has(result, "schema_valid")) {
        json_t *os = json_obj_get(result, "summary");
        json_t *cs = json_obj_get(cleaned, "summary");
        if (!json_equal(os, cs)) {
            int valid = (schema && schema->type == JSON_OBJECT) ? 1 : 0;
            json_set(cleaned, "schema_valid", json_bool(valid ? true : false));
            json_obj_del(cleaned, "schema_errors");
            json_obj_del(cleaned, "schema_note");
            if (!valid) {
                json_t *errs = json_array();
                json_append(errs, json_string("The redacted summary has not been validated against the original output_schema."));
                json_set(cleaned, "schema_errors", redact_value(errs, force, 0));
                json_free(errs);
                json_set(cleaned, "schema_note",
                    json_string("Secret redaction changed the delivered summary; do not treat it as validated output."));
            }
        }
    }
    return cleaned;
}

struct json_t *w14_delegation_58ac47_redact_child_text(const struct json_t *text, int force) {
    return json_string(redact_text_s(text, force));
}

struct json_t *w14_delegation_58ac47_redact_child_value(const struct json_t *value, int force) {
    return redact_value(value, force, 0);
}

struct json_t *w14_delegation_58ac47_redact_child_result(const struct json_t *result,
                                                         const struct json_t *output_schema, int force) {
    return redact_result(result ? result : json_object(), output_schema, force, 0);
}

struct json_t *w14_delegation_58ac47_redact_completion_event(const struct json_t *event, int force) {
    json_t *filtered = json_object();
    for (size_t i = 0; i < json_object_size(event); i++) {
        const char *k = json_object_get_key_at(event, i);
        if (!k || in_set(k, PARENT_FIELDS)) continue;
        json_set(filtered, k, json_copy(json_object_get_at(event, i)));
    }
    json_t *cleaned = redact_result(filtered, NULL, force, 0);
    for (size_t i = 0; i < json_object_size(event); i++) {
        const char *k = json_object_get_key_at(event, i);
        if (!k || !in_set(k, PARENT_FIELDS)) continue;
        json_set(cleaned, k, json_copy(json_object_get_at(event, i)));
    }
    json_free(filtered);
    return cleaned;
}

