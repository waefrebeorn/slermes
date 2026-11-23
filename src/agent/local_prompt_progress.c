/*
 * local_prompt_progress.c — faithful C11 port of agent/local_prompt_progress.py.
 * Gate w14_localpromp_2504b2.
 *
 * PoP: w14_localpromp_2504b2_with_prompt_progress @ agent/local_prompt_progress.py:with_prompt_progress
 * PoP: w14_localpromp_2504b2_prompt_progress @ agent/local_prompt_progress.py:prompt_progress
 * PoP: w14_localpromp_2504b2_prompt_progress_percent @ agent/local_prompt_progress.py:prompt_progress_percent
 *
 * Thread-local hook state and PromptReadingClock.tick (wall-clock dependent) are
 * intentionally not ported here; the deterministic decision/pure helpers are.
 */
#include "noc_w14_localpromp_2504b2.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
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

/* Python int(x) for JSON scalar x. 0 on failure to parse (fixtures avoid overflow). */
static int py_int(const json_t *v, int64_t *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        *out = (int64_t)v->num_val;  /* C truncates toward zero, like Python int(float) */
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *s = p;
        if (*s == '+' || *s == '-') s++;
        if (!isdigit((unsigned char)*s)) return 0;
        while (isdigit((unsigned char)*s)) s++;
        const char *e = s;
        while (*e && isspace((unsigned char)*e)) e++;
        if (*e != '\0') return 0;  /* Python int() rejects trailing junk / decimals */
        errno = 0;
        char *end = NULL;
        long long n = strtoll(p, &end, 10);
        if (end != e || errno == ERANGE) return 0;
        *out = (int64_t)n;
        return 1;
    }
    return 0;  /* int(list/dict) raises TypeError */
}

/* Python round(x): nearest integer, ties to even. */
static int64_t py_round_i(double x) {
    double fl = floor(x);
    double frac = x - fl;
    int64_t r = (int64_t)fl;
    if (frac > 0.5) return r + 1;
    if (frac < 0.5) return r;
    return ((r & 1) == 0) ? r : r + 1;
}

struct json_t *w14_localpromp_2504b2_with_prompt_progress(const struct json_t *kwargs) {
    json_t *out = json_object();
    if (kwargs && kwargs->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(kwargs); i++) {
            const char *k = json_object_get_key_at(kwargs, i);
            json_t *v = json_object_get_at(kwargs, i);
            if (k) json_set(out, k, json_copy(v));
        }
    }
    json_t *merged = json_object();
    const json_t *extra = kwargs ? json_obj_get(kwargs, "extra_body") : NULL;
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            json_t *v = json_object_get_at(extra, i);
            if (k) json_set(merged, k, json_copy(v));
        }
    }
    if (!json_has(merged, "return_progress"))
        json_set(merged, "return_progress", json_bool(true));
    json_set(out, "extra_body", merged);  /* replaces the copy; frees the old value */
    return out;
}

struct json_t *w14_localpromp_2504b2_prompt_progress(const struct json_t *chunk) {
    const json_t *progress = chunk ? json_obj_get(chunk, "prompt_progress") : NULL;
    if (!progress || progress->type != JSON_OBJECT) return json_null();
    const json_t *choices = chunk ? json_obj_get(chunk, "choices") : NULL;
    static const char *const CONTENT_FIELDS[] = {
        "content", "tool_calls", "reasoning_content", "reasoning", "refusal", NULL
    };
    if (choices && choices->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(choices); i++) {
            const json_t *choice = json_get(choices, i);
            const json_t *finish = (choice && choice->type == JSON_OBJECT)
                ? json_obj_get(choice, "finish_reason") : NULL;
            if (py_truthy(finish)) return json_null();
            const json_t *delta = (choice && choice->type == JSON_OBJECT)
                ? json_obj_get(choice, "delta") : NULL;
            for (int f = 0; CONTENT_FIELDS[f]; f++) {
                const json_t *value = (delta && delta->type == JSON_OBJECT)
                    ? json_obj_get(delta, CONTENT_FIELDS[f]) : NULL;
                if (py_truthy(value)) return json_null();
            }
        }
    }
    return json_copy((json_t *)progress);
}

struct json_t *w14_localpromp_2504b2_prompt_progress_percent(const struct json_t *progress) {
    int64_t total = 0, processed = 0;
    const json_t *tv = progress ? json_obj_get(progress, "total") : NULL;
    const json_t *pv = progress ? json_obj_get(progress, "processed") : NULL;
    if (py_truthy(tv)) { if (!py_int(tv, &total)) return json_null(); }
    if (py_truthy(pv)) { if (!py_int(pv, &processed)) return json_null(); }
    if (total <= 0) return json_null();
    double x = (double)processed / (double)total * 100.0;
    int64_t r = py_round_i(x);
    if (r < 0) r = 0;
    if (r > 100) r = 100;
    return json_number((double)r);
}
