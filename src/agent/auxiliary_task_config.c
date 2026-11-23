/*
 * auxiliary_task_config.c — faithful C11 port of agent/auxiliary_task_config.py
 * (gate w14_auxiliaryt_39927b). Every exported symbol is prefixed with the gate.
 *
 * PoP: w14_auxiliaryt_39927b_authoritative_auxiliary_task @ agent/auxiliary_task_config.py:authoritative_auxiliary_task
 * PoP: w14_auxiliaryt_39927b_core_rooted @ agent/auxiliary_task_config.py:_core_rooted
 * PoP: w14_auxiliaryt_39927b_layer_over_inherited @ agent/auxiliary_task_config.py:_layer_over_inherited
 */
#include "noc_w14_auxiliaryt_39927b.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* dupstr — never `dup` (POSIX collision). */
static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static bool py_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Python str() for the JSON types used in config coercion fields. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == 0.0) return dupstr("0");
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return dupstr(buf);
    }
    return dupstr(v->type == JSON_ARRAY ? "[]" : "{}");
}

static char *py_or_empty(const json_t *v) {
    return py_truthy(v) ? py_str(v) : dupstr("");
}

/* ASCII str.strip(); the NUL is written at the *new* length. */
static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t n, a = 0, b;
    if (!t) return NULL;
    n = strlen(t); b = n;
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';
    return t;
}

static void lower_ascii(char *s) {
    for (; s && *s; ++s) *s = (char)tolower((unsigned char)*s);
}

static bool is_aux_route_key(const char *k) {
    static const char *keys[] = {
        "provider", "model", "base_url", "api_key", "api_mode",
        "key_env", "api_key_env", "reasoning_effort", NULL
    };
    if (!k) return false;
    for (int i = 0; keys[i]; ++i) if (!strcmp(k, keys[i])) return true;
    return false;
}

/* AuthoritativeAuxiliaryTask(str(task or "")) — a str subclass, observed as str. */
char *w14_auxiliaryt_39927b_authoritative_auxiliary_task(const char *task) {
    return dupstr(task ? task : "");
}

/* _core_rooted(base, entries): True when base resolves to a core block. */
bool w14_auxiliaryt_39927b_core_rooted(const char *base, const json_t *entries) {
    char **seen = NULL;
    size_t nseen = 0, cap = 0;
    const char *cur = base;
    bool result;

    if (!cur || !entries || entries->type != JSON_OBJECT) return true; /* None/other not in seen */
    for (;;) {
        json_t *entry = json_obj_get(entries, cur);
        bool already = false;
        json_t *inh;
        if (!entry) break; /* base not in entries */
        for (size_t i = 0; i < nseen; ++i) {
            if (!strcmp(seen[i], cur)) { already = true; break; }
        }
        if (already) break; /* base in seen */
        if (nseen == cap) {
            cap = cap ? cap * 2 : 8;
            seen = (char **)realloc(seen, cap * sizeof(char *));
            if (!seen) return false;
        }
        seen[nseen++] = dupstr(cur);
        inh = (entry->type == JSON_OBJECT) ? json_obj_get(entry, "inherit_from") : NULL;
        if (!py_truthy(inh)) { /* `if not base: return False` */
            for (size_t i = 0; i < nseen; ++i) free(seen[i]);
            free(seen);
            return false;
        }
        if (inh->type != JSON_STRING || !inh->str_val) { /* unsupported key type: not rooted */
            for (size_t i = 0; i < nseen; ++i) free(seen[i]);
            free(seen);
            return false;
        }
        cur = inh->str_val;
    }
    result = true; /* `return base not in seen` */
    for (size_t i = 0; i < nseen; ++i) {
        if (cur && !strcmp(seen[i], cur)) { result = false; break; }
    }
    for (size_t i = 0; i < nseen; ++i) free(seen[i]);
    free(seen);
    return result;
}

/* _layer_over_inherited: merge a task's own block over its inherited base. */
json_t *w14_auxiliaryt_39927b_layer_over_inherited(const json_t *inherited, const json_t *user) {
    json_t *merged = json_object();
    const json_t *u = (user && user->type == JSON_OBJECT) ? user : NULL;
    const json_t *inh = (inherited && inherited->type == JSON_OBJECT) ? inherited : NULL;
    bool pinned = false;

    if (u) {
        char *provider = py_or_empty(json_obj_get(u, "provider"));
        char *model = py_or_empty(json_obj_get(u, "model"));
        char *base_url = py_or_empty(json_obj_get(u, "base_url"));
        char *ps = strip_dup(provider);
        char *ms = strip_dup(model);
        char *bs = strip_dup(base_url);
        lower_ascii(ps);
        pinned = (ps[0] != '\0' && strcmp(ps, "auto") != 0) || ms[0] != '\0' || bs[0] != '\0';
        free(provider); free(model); free(base_url);
        free(ps); free(ms); free(bs);
    }

    if (inh) {
        for (size_t i = 0; i < json_object_size(inh); ++i) {
            const char *k = json_object_get_key_at(inh, i);
            json_t *v = json_object_get_at(inh, i);
            if (pinned && is_aux_route_key(k)) continue;
            json_set(merged, k, json_copy(v));
        }
    }
    if (u) {
        for (size_t i = 0; i < json_object_size(u); ++i) {
            const char *k = json_object_get_key_at(u, i);
            json_t *v = json_object_get_at(u, i);
            bool empty_str = (v && v->type == JSON_STRING && v->str_val && v->str_val[0] == '\0');
            if (empty_str && !(pinned && is_aux_route_key(k))) continue;
            if (k && !strcmp(k, "provider") && !pinned) continue;
            json_set(merged, k, json_copy(v));
        }
    }
    return merged;
}
