/*
 * credential_pool_admin.c — faithful C11 port of agent/credential_pool_admin.py
 * (gate w14_credential_e7e645).
 *
 * PoP: w14_credential_e7e645_resolve_target @ agent/credential_pool_admin.py:CredentialPoolAdminMixin.resolve_target
 * PoP: w14_credential_e7e645_remove_index_ex @ agent/credential_pool_admin.py:CredentialPoolAdminMixin.remove_index
 *
 * The pool's _entries and _current_id are injected; the real branch logic of
 * target resolution and index removal (including contiguous re-prioritisation
 * and the current-id clearing) is ported faithfully.
 */
#include "noc_w14_credential_e7e645.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t a = 0, b;
    if (!t) return NULL;
    b = strlen(t);
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';
    return t;
}

static char *lower_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    for (size_t i = 0; t && t[i]; ++i) t[i] = (char)tolower((unsigned char)t[i]);
    return t;
}

static const char *entry_str(const json_t *e, const char *key) {
    const json_t *v;
    if (!e || e->type != JSON_OBJECT) return NULL;
    v = json_obj_get(e, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static long long entry_priority(const json_t *e) {
    const json_t *v = (e && e->type == JSON_OBJECT) ? json_obj_get(e, "priority") : NULL;
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : 0;
}

static json_t *result_obj(long long idx, const char *id, const char *error) {
    json_t *o = json_object();
    if (idx < 0) json_set(o, "index", json_null());
    else json_set(o, "index", json_number((double)idx));
    json_set(o, "id", id ? json_string(id) : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    return o;
}

static bool all_digits(const char *s) {
    if (!s || !s[0]) return false;
    for (; *s; ++s) if (!isdigit((unsigned char)*s)) return false;
    return true;
}

json_t *w14_credential_e7e645_resolve_target(const char *target, const json_t *entries) {
    char *raw = strip_dup(target);
    size_t n = (entries && entries->type == JSON_ARRAY) ? entries->c.count : 0;
    json_t *out;
    char buf[1024];

    if (!raw || !raw[0]) {
        free(raw);
        return result_obj(-1, NULL, "No credential target provided.");
    }
    /* exact id match, 1-based */
    for (size_t i = 0; i < n; ++i) {
        const char *cid = entry_str(json_get(entries, i), "id");
        if (cid && !strcmp(cid, raw)) {
            json_t *r = result_obj((long long)i + 1, cid, NULL);
            free(raw);
            return r;
        }
    }
    /* case-insensitive label match */
    {
        char *raw_lower = lower_dup(raw);
        long long match_idx = -1;
        const char *match_id = NULL;
        int matches = 0;
        for (size_t i = 0; i < n; ++i) {
            const json_t *e = json_get(entries, i);
            char *lab = strip_dup(entry_str(e, "label"));
            char *lab_lower = lower_dup(lab);
            if (lab_lower && !strcmp(lab_lower, raw_lower)) {
                matches++;
                match_idx = (long long)i + 1;
                match_id = entry_str(e, "id");
            }
            free(lab); free(lab_lower);
        }
        if (matches == 1) {
            json_t *r = result_obj(match_idx, match_id, NULL);
            free(raw_lower); free(raw);
            return r;
        }
        if (matches > 1) {
            snprintf(buf, sizeof(buf),
                     "Ambiguous credential label \"%s\". Use the numeric index or entry id instead.", raw);
            out = result_obj(-1, NULL, buf);
            free(raw_lower); free(raw);
            return out;
        }
        free(raw_lower);
    }
    if (all_digits(raw)) {
        long long index = strtoll(raw, NULL, 10);
        if (index >= 1 && index <= (long long)n) {
            const char *cid = entry_str(json_get(entries, (size_t)(index - 1)), "id");
            out = result_obj(index, cid, NULL);
        } else {
            snprintf(buf, sizeof(buf), "No credential #%lld.", index);
            out = result_obj(-1, NULL, buf);
        }
        free(raw);
        return out;
    }
    snprintf(buf, sizeof(buf), "No credential matching \"%s\".", raw);
    out = result_obj(-1, NULL, buf);
    free(raw);
    return out;
}

static json_t *entry_projection(const json_t *e, long long priority) {
    json_t *o = json_object();
    const char *id = entry_str(e, "id");
    json_set(o, "id", id ? json_string(id) : json_null());
    json_set(o, "priority", json_number((double)priority));
    return o;
}

json_t *w14_credential_e7e645_remove_index_ex(const json_t *entries, long long index,
                                              const char *current_id) {
    json_t *o = json_object();
    size_t n = (entries && entries->type == JSON_ARRAY) ? entries->c.count : 0;
    json_t *remaining = json_array();
    const char *removed_id = NULL;
    const char *new_current = current_id;

    if (index >= 1 && index <= (long long)n) {
        size_t ri = (size_t)(index - 1);
        long long p = 0;
        removed_id = entry_str(json_get(entries, ri), "id");
        for (size_t i = 0; i < n; ++i) {
            if (i == ri) continue;
            json_append(remaining, entry_projection(json_get(entries, i), p++));
        }
        if (removed_id && current_id && !strcmp(current_id, removed_id)) new_current = NULL;
    } else {
        for (size_t i = 0; i < n; ++i)
            json_append(remaining, entry_projection(json_get(entries, i), entry_priority(json_get(entries, i))));
    }
    json_set(o, "removed", removed_id ? json_string(removed_id) : json_null());
    json_set(o, "remaining", remaining);
    json_set(o, "current_id", new_current ? json_string(new_current) : json_null());
    return o;
}
