/*
 * run_profile_home.c — C home for upstream Hermes gateway/run_profile_home.py.
 *
 * PoP: w10_runprofile_b87e2d_served_names @ gateway/run_profile_home.py:_served_names
 * PoP: w10_runprofile_b87e2d_resolve_source @ gateway/run_profile_home.py:resolve_profile_home_for_source
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_runprofile_b87e2d.h"

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

/* Mirrors: {name for name, _home in _multiplex_profile_homes(config)}.
 * The fixture supplies the (name, home) list as "served"; the oracle sorts the set. */
json_t *w10_runprofile_b87e2d_served_names(const json_t *in) {
    const json_t *served = json_obj_get(in, "served");
    const char **items = NULL;
    size_t n = 0;
    if (served && served->type == JSON_ARRAY) {
        size_t m = json_len(served);
        if (m) items = (const char **)calloc(m, sizeof(char *));
        for (size_t i = 0; items && i < m; i++) {
            const json_t *e = json_get(served, i);
            if (!e || e->type != JSON_STRING || !e->str_val) continue;
            int dup = 0;
            for (size_t j = 0; j < n; j++) {
                if (!strcmp(items[j], e->str_val)) { dup = 1; break; }
            }
            if (!dup) items[n++] = e->str_val;
        }
    }
    if (n > 1) qsort(items, n, sizeof(char *), cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_string(items[i]));
    free(items);
    return out;
}

/* Mirrors resolve_profile_home_for_source, with every imported collaborator
 * injected through the fixture (identity, profile lookup, served set, home). */
json_t *w10_runprofile_b87e2d_resolve_source(const json_t *in) {
    const json_t *identity_home = json_obj_get(in, "identity_home");
    if (identity_home && identity_home->type == JSON_STRING && identity_home->str_val) {
        json_t *o = json_object();
        json_set(o, "home", json_copy(identity_home));   /* identity.runtime_home */
        return o;
    }

    int multiplexed = json_get_bool(in, "multiplexed", 0);
    const char *sp = json_get_str(in, "source_profile", "");
    const char *s = sp, *e = sp + strlen(sp);
    while (s < e && isspace((unsigned char)*s)) s++;   /* (source.profile or "").strip() */
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t slen = (size_t)(e - s);
    char *trimmed = (char *)malloc(slen + 1);
    if (trimmed) memcpy(trimmed, s, slen);
    if (trimmed) trimmed[slen] = '\0';

    const char *name = (trimmed && slen) ? trimmed : json_get_str(in, "derived_name", "");
    int explicit = (name && *name) ? 1 : 0;
    if (!explicit) {
        const char *active = json_get_str(in, "active_name", "default");
        name = (active && *active) ? active : "default";
    }

    const json_t *profiles = json_obj_get(in, "profiles");
    const json_t *pinfo = profiles ? json_obj_get(profiles, name) : NULL;
    int exists = pinfo ? json_get_bool(pinfo, "exists", 0) : 0;
    const char *dir = pinfo ? json_get_str(pinfo, "dir", "/none") : "/none";
    const char *norm = pinfo ? json_get_str(pinfo, "norm", name) : name;

    int reject = 0;
    if (explicit && multiplexed) {
        int served_has = 0;
        const json_t *served = json_obj_get(in, "served");
        if (served && served->type == JSON_ARRAY) {
            size_t m = json_len(served);
            for (size_t i = 0; i < m; i++) {
                const json_t *v = json_get(served, i);
                if (v && v->type == JSON_STRING && v->str_val && norm &&
                    !strcmp(v->str_val, norm)) { served_has = 1; break; }
            }
        }
        if (!exists || !served_has) reject = 1;       /* ProfileRouteRejected */
    }

    json_t *out = json_object();
    if (reject) {
        json_set(out, "reject", json_string(explicit ? name : ""));
    } else if (explicit && !exists) {
        json_set(out, "home", json_string(json_get_str(in, "home", "/global")));
    } else {
        json_set(out, "home", json_string(dir ? dir : ""));
    }
    free(trimmed);
    return out;
}
