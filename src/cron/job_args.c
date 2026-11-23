/*
 * job_args.c — faithful C11 port of the pure helpers in cron/job_args.py.
 *
 * PoP: w5_jobargs_45f9c5_split_monitor_arg @ cron/job_args.py:_split_monitor_arg
 * PoP: w5_jobargs_45f9c5_repeat_display @ cron/job_args.py:_repeat_display
 * PoP: w5_jobargs_45f9c5_clean_str_list @ cron/job_args.py:_clean_str_list
 * PoP: w5_jobargs_45f9c5_canonical_skills @ cron/job_args.py:_canonical_skills
 * PoP: w5_jobargs_45f9c5_normalize_optional @ cron/job_args.py:_normalize_optional_job_value
 * PoP: w5_jobargs_45f9c5_normalize_deliver @ cron/job_args.py:_normalize_deliver_param
 * PoP: w5_jobargs_45f9c5_apply_continuity @ cron/job_args.py:_apply_continuity
 * PoP: w5_jobargs_45f9c5_base_url_refused @ cron/job_args.py:_base_url_refused
 */
#include "noc_w5_jobargs_45f9c5.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

static char *w5j_dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *w5j_strip(const char *s) {
    if (!s) return w5j_dupn("", 0);
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return w5j_dupn(s + a, b - a);
}

static int w5j_truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Python str() for JSON scalars (fixtures avoid container reprs). */
static char *w5j_py_str(const struct json_t *v) {
    if (!v || v->type == JSON_NULL) return w5j_dupn("None", 4);
    if (v->type == JSON_STRING) return w5j_dupn(v->str_val ? v->str_val : "", strlen(v->str_val ? v->str_val : ""));
    if (v->type == JSON_BOOL) return w5j_dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
    if (v->type == JSON_NUMBER) {
        char buf[64];
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(buf, sizeof(buf), "%.*g", prec, v->num_val);
            if (strtod(buf, NULL) == v->num_val) break;
        }
        return w5j_dupn(buf, strlen(buf));
    }
    return w5j_dupn("", 0);
}

/* Python repr() of a string. */
static char *w5j_py_repr_str(const char *s) {
    if (!s) s = "";
    int dq = (strchr(s, '\'') && !strchr(s, '"'));
    char q = dq ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 5 + 3);
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == (unsigned char)q) { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            static const char *H = "0123456789abcdef";
            out[k++] = '\\'; out[k++] = 'x'; out[k++] = H[c >> 4]; out[k++] = H[c & 15];
        } else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

static struct json_t *w5j_string_or_null(char *s) {
    if (!s || !*s) { free(s); return json_null(); }
    struct json_t *r = json_string(s);
    free(s);
    return r;
}

struct json_t *w5_jobargs_45f9c5_split_monitor_arg(const struct json_t *monitor,
                                                   const struct json_t *monitor_script,
                                                   const struct json_t *monitor_url) {
    struct json_t *out = json_array();
    if (!monitor || monitor->type == JSON_NULL) {
        json_append(out, monitor_script ? json_copy(monitor_script) : json_null());
        json_append(out, monitor_url ? json_copy(monitor_url) : json_null());
        return out;
    }
    const char *raw = (monitor->type == JSON_STRING) ? monitor->str_val : "";
    char *value = w5j_strip(raw);
    if (!*value) {
        json_append(out, json_string(""));
        json_append(out, json_string(""));
    } else if (!strncasecmp(value, "http://", 7) || !strncasecmp(value, "https://", 8)) {
        json_append(out, json_string(""));
        json_append(out, json_string(value));
    } else {
        json_append(out, json_string(value));
        json_append(out, json_string(""));
    }
    free(value);
    return out;
}

struct json_t *w5_jobargs_45f9c5_repeat_display(const struct json_t *job) {
    const struct json_t *rep = job ? json_obj_get(job, "repeat") : NULL;
    if (!rep || rep->type != JSON_OBJECT || json_object_size(rep) == 0) rep = NULL;
    const struct json_t *times = rep ? json_obj_get(rep, "times") : NULL;
    const struct json_t *completed = rep ? json_obj_get(rep, "completed") : NULL;
    if (!times || times->type == JSON_NULL) return json_string("forever");
    int is_one = (times->type == JSON_NUMBER && times->num_val == 1.0);
    if (is_one) {
        int zero = (!completed || completed->type == JSON_NULL ||
                    (completed->type == JSON_NUMBER && completed->num_val == 0.0));
        return json_string(zero ? "once" : "1/1");
    }
    char *ts = w5j_py_str(times);
    if (w5j_truthy(completed)) {
        char *cs = w5j_py_str(completed);
        size_t len = strlen(cs) + strlen(ts) + 2;
        char *out = (char *)malloc(len);
        snprintf(out, len, "%s/%s", cs, ts);
        free(cs); free(ts);
        struct json_t *r = json_string(out);
        free(out);
        return r;
    }
    size_t len = strlen(ts) + 8;
    char *out = (char *)malloc(len);
    snprintf(out, len, "%s times", ts);
    free(ts);
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

struct json_t *w5_jobargs_45f9c5_clean_str_list(const struct json_t *items) {
    struct json_t *out = json_array();
    if (!items || items->type == JSON_NULL) return out;
    size_t n = 0, i = 0;
    const struct json_t *single = NULL;
    if (items->type == JSON_STRING) { single = items; n = 1; }
    else if (items->type == JSON_ARRAY) { n = json_len(items); }
    else return out;
    for (; i < n; i++) {
        const struct json_t *v = single ? single : json_get(items, i);
        char *raw = w5j_py_str(v);
        char *s = w5j_strip(raw);
        free(raw);
        if (*s) json_append(out, json_string(s));
        free(s);
    }
    return out;
}

struct json_t *w5_jobargs_45f9c5_canonical_skills(const struct json_t *skill,
                                                  const struct json_t *skills) {
    struct json_t *source = NULL;
    struct json_t *tmp = NULL;
    if (!skills || skills->type == JSON_NULL) {
        if (w5j_truthy(skill)) { tmp = json_array(); json_append(tmp, json_copy(skill)); source = tmp; }
    } else if (skills->type == JSON_STRING) {
        tmp = json_array(); json_append(tmp, json_copy(skills)); source = tmp;
    } else {
        source = (struct json_t *)skills;
    }
    struct json_t *out = json_array();
    size_t count = source ? json_len(source) : 0;
    for (size_t i = 0; i < count; i++) {
        const struct json_t *item = json_get(source, i);
        /* generator is `item or ""`: a falsy entry drops out, not "None". */
        char *raw = w5j_truthy(item) ? w5j_py_str(item) : w5j_dupn("", 0);
        char *s = w5j_strip(raw);
        free(raw);
        if (!*s) { free(s); continue; }
        int seen = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            const struct json_t *o = json_get(out, j);
            if (o && o->type == JSON_STRING && !strcmp(o->str_val, s)) { seen = 1; break; }
        }
        if (!seen) json_append(out, json_string(s));
        free(s);
    }
    json_free(tmp);
    return out;
}

struct json_t *w5_jobargs_45f9c5_normalize_optional(const struct json_t *value,
                                                    int strip_trailing_slash) {
    if (!value || value->type == JSON_NULL) return json_null();
    char *raw = w5j_py_str(value);
    char *text = w5j_strip(raw);
    free(raw);
    if (strip_trailing_slash) {
        size_t n = strlen(text);
        while (n > 0 && text[n - 1] == '/') text[--n] = '\0';
    }
    return w5j_string_or_null(text);
}

struct json_t *w5_jobargs_45f9c5_normalize_deliver(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    if (value->type == JSON_ARRAY) {
        struct json_t *cleaned = w5_jobargs_45f9c5_clean_str_list(value);
        size_t len = 1;
        for (size_t i = 0; i < json_len(cleaned); i++) {
            const struct json_t *v = json_get(cleaned, i);
            len += (v && v->type == JSON_STRING) ? strlen(v->str_val) : 0;
            if (i) len++;
        }
        char *joined = (char *)malloc(len + 1);
        joined[0] = '\0';
        for (size_t i = 0; i < json_len(cleaned); i++) {
            const struct json_t *v = json_get(cleaned, i);
            if (i) strcat(joined, ",");
            if (v && v->type == JSON_STRING) strcat(joined, v->str_val);
        }
        json_free(cleaned);
        return w5j_string_or_null(joined);
    }
    char *raw = w5j_py_str(value);
    char *text = w5j_strip(raw);
    free(raw);
    return w5j_string_or_null(text);
}

struct json_t *w5_jobargs_45f9c5_apply_continuity(const struct json_t *context_from,
                                                  int continuity) {
    struct json_t *refs = w5_jobargs_45f9c5_clean_str_list(context_from);
    int has_self = 0;
    for (size_t i = 0; i < json_len(refs); i++) {
        const struct json_t *v = json_get(refs, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
        if (!strcasecmp(s, "self")) { has_self = 1; break; }
    }
    if (continuity && !has_self) {
        json_append(refs, json_string("self"));
    } else if (!continuity && has_self) {
        size_t w = 0;
        for (size_t i = 0; i < json_len(refs); i++) {
            struct json_t *v = json_get(refs, i);
            const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
            if (!strcasecmp(s, "self")) { json_free(v); continue; }
            if (w != i) refs->c.items[w] = v;
            w++;
        }
        refs->c.count = w;
    }
    if (json_len(refs) == 0) { json_free(refs); return json_null(); }
    return refs;
}

struct json_t *w5_jobargs_45f9c5_base_url_refused(const char *bu, const char *prov, const char *why) {
    char *rb = w5j_py_repr_str(bu ? bu : "");
    char *rp = w5j_py_repr_str(prov ? prov : "");
    size_t len = strlen("base_url ") + strlen(rb) + strlen(" is not allowed for provider ") +
                 strlen(rp) + strlen(". ") + strlen(why ? why : "") + 1;
    char *out = (char *)malloc(len);
    snprintf(out, len, "base_url %s is not allowed for provider %s. %s",
             rb, rp, why ? why : "");
    free(rb); free(rp);
    struct json_t *r = json_string(out);
    free(out);
    return r;
}

