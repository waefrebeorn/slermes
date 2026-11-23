/*
 * sdk_transform_bypass.c — C port of upstream Hermes agent/sdk_transform_bypass.py.
 *
 * PoP: w13_sdktransfo_241aa3_is_plain_json_data @ agent/sdk_transform_bypass.py:_is_plain_json_data
 * PoP: w13_sdktransfo_241aa3_bypass @ agent/sdk_transform_bypass.py:bypass_sdk_request_transform
 * PoP: w13_sdktransfo_241aa3_bypass_chat @ agent/sdk_transform_bypass.py:bypass_chat_sdk_request_transform
 *
 * The escape-hatch environment read is an explicit seam (`env_value` injected
 * from the fixture) rather than mutating the process environment: Windows CRT
 * setenv(name, "", 1) unsets a variable while Python keeps it as "". The SDK
 * gate is likewise injected as `is_openai`.
 */
#include "noc_w13_sdktransfo_241aa3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

static const char *jstr_field(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

int w13_sdktransfo_241aa3_is_plain_json_data(const json_t *v) {
    if (!v) return 1;                       /* None */
    switch (v->type) {
        case JSON_NULL:
        case JSON_BOOL:
        case JSON_NUMBER:
        case JSON_STRING: return 1;         /* str/int/float/bool */
        case JSON_ARRAY:
            for (size_t i = 0; i < json_len(v); i++)
                if (!w13_sdktransfo_241aa3_is_plain_json_data(json_get(v, i))) return 0;
            return 1;
        case JSON_OBJECT:
            for (size_t i = 0; i < json_object_size(v); i++) {
                const char *k = json_object_get_key_at(v, i);
                if (!k) return 0;            /* non-str key */
                if (!w13_sdktransfo_241aa3_is_plain_json_data(json_object_get_at(v, i))) return 0;
            }
            return 1;
        default: return 0;
    }
}

/* os.environ.get(ENV, "").strip().lower() in {"1","true","yes","on"} */
static int env_truthy(const char *value) {
    if (!value) return 0;
    const char *p = value;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    const char *words[] = {"1", "true", "yes", "on"};
    for (size_t i = 0; i < 4; i++) {
        size_t w = strlen(words[i]);
        if (n == w) {
            int eq = 1;
            for (size_t j = 0; j < n; j++)
                if ((char)tolower((unsigned char)p[j]) != words[i][j]) { eq = 0; break; }
            if (eq) return 1;
        }
    }
    return 0;
}

/* Collect {f: kwargs[f] for f in fields if container(f) and plain(f)}. */
static json_t *collect_moved(const json_t *kwargs, const json_t *fields) {
    json_t *moved = json_object();
    if (!json_is_object(kwargs)) return moved;
    size_t nf = json_is_array(fields) ? json_len(fields) : 2;
    const char *defs[2] = {"input", "tools"};
    for (size_t i = 0; i < nf; i++) {
        const char *f = json_is_array(fields) ? jstr_field(json_get(fields, i)) : defs[i];
        if (!f || !*f) continue;
        json_t *v = json_obj_get(kwargs, f);
        if (!v || (v->type != JSON_OBJECT && v->type != JSON_ARRAY)) continue;
        if (!w13_sdktransfo_241aa3_is_plain_json_data(v)) continue;
        json_set(moved, f, json_copy(v));
    }
    return moved;
}

struct json_t *w13_sdktransfo_241aa3_bypass(const json_t *request_kwargs,
                                           const json_t *fields, int keep_slots,
                                           const char *env_value) {
    if (env_truthy(env_value)) return json_copy(request_kwargs);
    json_t *moved = collect_moved(request_kwargs, fields);
    if (json_object_size(moved) == 0) { json_free(moved); return json_copy(request_kwargs); }

    json_t *bypassed = json_object();
    if (json_is_object(request_kwargs)) {
        for (size_t i = 0; i < json_object_size(request_kwargs); i++) {
            const char *k = json_object_get_key_at(request_kwargs, i);
            json_t *v = json_object_get_at(request_kwargs, i);
            if (!k) continue;
            int in_moved = json_obj_get(moved, k) != NULL;
            if (in_moved && !keep_slots) continue;
            json_set(bypassed, k, (in_moved && keep_slots) ? json_array() : json_copy(v));
        }
    }
    json_t *extra = json_obj_get(bypassed, "extra_body");
    json_t *merged = json_object();
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(extra); i++)
            json_set(merged, json_object_get_key_at(extra, i),
                     json_copy(json_object_get_at(extra, i)));
    }
    /* {**merged, **moved_without_explicit_keys}: explicit extra_body wins. */
    for (size_t i = 0; i < json_object_size(moved); i++) {
        const char *k = json_object_get_key_at(moved, i);
        if (k && !json_has(merged, k))
            json_set(merged, k, json_copy(json_object_get_at(moved, i)));
    }
    json_set(bypassed, "extra_body", merged);
    json_free(moved);
    return bypassed;
}

struct json_t *w13_sdktransfo_241aa3_bypass_chat(const json_t *request_kwargs,
                                                int is_openai, const char *env_value) {
    if (!is_openai) return json_copy(request_kwargs);
    json_t *chat_fields = json_array();
    json_append(chat_fields, json_string("messages"));
    json_append(chat_fields, json_string("tools"));
    json_t *out = w13_sdktransfo_241aa3_bypass(request_kwargs, chat_fields, 1, env_value);
    json_free(chat_fields);
    return out;
}
