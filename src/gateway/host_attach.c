/*
 * host_attach.c — C home for upstream Hermes gateway/host_attach.py.
 * Ported behavior: _served_from_identity, HostGateway.serves and
 * standalone_rescan_message. The probe / poll / lock lifecycle stays in Python.
 * PoP: w6_hostattach_served_from_identity @ gateway/host_attach.py:_served_from_identity
 */
#include "noc_w6_hostattach_55be4c.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (out) memcpy(out, s, n + 1);
    return out;
}

static void push_name(char ***arr, size_t *n, size_t *cap, char *name) {
    if (*n + 1 >= *cap) {
        size_t nc = *cap ? *cap * 2 : 8;
        char **tmp = (char **)realloc(*arr, nc * sizeof(char *));
        if (!tmp) { free(name); return; }
        *arr = tmp;
        *cap = nc;
    }
    (*arr)[(*n)++] = name ? name : dup_str("");
}

/* Seam for profile_name_for_home: gateway.status._profile_name_for_home is
 * stubbed by the oracle as Path(home).name or "default". */
static char *profile_name_for_home(const char *home) {
    const char *cur, *end;
    size_t len, i;
    char *out;
    if (!home || !*home) return dup_str("default");
    len = strlen(home);
    while (len > 0 && home[len - 1] == '/') len--; /* Path drops trailing / */
    if (len == 0) return dup_str("default");       /* "/" has no name */
    end = home + len;
    cur = home;
    for (i = 0; i < len; i++)
        if (home[i] == '/') cur = home + i + 1;
    if (cur >= end) return dup_str("default");
    out = (char *)malloc((size_t)(end - cur) + 1);
    if (!out) return dup_str("default");
    memcpy(out, cur, (size_t)(end - cur));
    out[end - cur] = '\0';
    return out;
}

/* str(value) for the JSON scalar values that can name a served profile. */
static char *json_as_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dup_str("None");
    if (v->type == JSON_STRING) return dup_str(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dup_str(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        return dup_str(buf);
    }
    return dup_str("");
}

char **w6_hostattach_served_from_identity(const json_t *identity, size_t *out_n) {
    char **arr = NULL;
    size_t n = 0, cap = 0, i;
    json_t *served = identity ? json_obj_get((json_t *)identity, "served_profiles") : NULL;

    if (served && served->type == JSON_ARRAY && served->c.count > 0) {
        for (i = 0; i < served->c.count; i++) {
            json_t *e = served->c.items[i];
            if (e && e->type == JSON_OBJECT) {
                json_t *h = json_obj_get((json_t *)e, "home");
                if (!h || h->type == JSON_NULL ||
                    (h->type == JSON_STRING && !(h->str_val && h->str_val[0])))
                    h = json_obj_get((json_t *)e, "profile_id");
                if (h && ((h->type == JSON_STRING && h->str_val && h->str_val[0]) ||
                          h->type == JSON_NUMBER)) {
                    char *s = json_as_pystr(h);
                    char *name = profile_name_for_home(s);
                    free(s);
                    push_name(&arr, &n, &cap, name);
                } else {
                    push_name(&arr, &n, &cap, dup_str("default"));
                }
            } else {
                push_name(&arr, &n, &cap, json_as_pystr(e));
            }
        }
    } else {
        json_t *pf = identity ? json_obj_get((json_t *)identity, "profile") : NULL;
        if (pf && pf->type != JSON_NULL &&
            !(pf->type == JSON_STRING && !(pf->str_val && pf->str_val[0])) &&
            !(pf->type == JSON_NUMBER && pf->num_val == 0.0) &&
            !(pf->type == JSON_BOOL && !pf->bool_val))
            push_name(&arr, &n, &cap, json_as_pystr(pf));
        else
            push_name(&arr, &n, &cap, dup_str("default"));
    }
    if (!arr) arr = (char **)malloc(sizeof(char *));
    if (arr) arr[n] = NULL;
    if (out_n) *out_n = n;
    return arr;
}

void w6_hostattach_free_strings(char **arr, size_t n) {
    size_t i;
    if (!arr) return;
    for (i = 0; i < n; i++) free(arr[i]);
    free(arr);
}

/* _normalize seam: (name or "default").strip().lower() */
static char *normalize_profile(const char *name) {
    size_t len, start, end, i;
    char *out;
    if (!name || !*name) name = "default";
    len = strlen(name);
    start = 0;
    while (start < len && isspace((unsigned char)name[start])) start++;
    end = len;
    while (end > start && isspace((unsigned char)name[end - 1])) end--;
    out = (char *)malloc(end - start + 1);
    if (!out) return dup_str("");
    for (i = start; i < end; i++)
        out[i - start] = (char)tolower((unsigned char)name[i]);
    out[end - start] = '\0';
    return out;
}

int w6_hostattach_serves(int served_known, char **profiles, size_t n,
                         const char *wanted) {
    char *want;
    size_t i;
    int found = 0;
    if (!served_known) return 0;
    want = normalize_profile(wanted);
    for (i = 0; i < n && !found; i++) {
        char *p = normalize_profile(profiles[i]);
        if (p && want && !strcmp(p, want)) found = 1;
        free(p);
    }
    free(want);
    return found ? 1 : 0;
}

void w6_hostattach_standalone_rescan_message(const char *profile,
                                             char *out, size_t cap) {
    if (!cap) return;
    snprintf(out, cap,
             "The host gateway still serves profile '%s'; gateway.standalone is not live yet. "
             "Wait for the host gateway to rescan (<=30s), or send the rescan-profiles control verb "
             "to the host gateway before starting this profile's gateway.",
             profile ? profile : "");
}
