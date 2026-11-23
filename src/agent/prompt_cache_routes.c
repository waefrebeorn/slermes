/*
 * prompt_cache_routes.c — C home for upstream Hermes agent/prompt_cache_routes.py.
 *
 * Faithful C11 ports of the module's pure helpers: finite-number checks,
 * well-formedness, bounded gap/session push, distinct-session counting, the
 * routine-hit short circuit, learned lifetime and the /context display word.
 *
 * PoP: w11_promptcach_850c92_finite @ agent/prompt_cache_routes.py:_finite
 * PoP: w11_promptcach_850c92_numbers @ agent/prompt_cache_routes.py:_numbers
 * PoP: w11_promptcach_850c92_well_formed @ agent/prompt_cache_routes.py:_well_formed
 * PoP: w11_promptcach_850c92_push @ agent/prompt_cache_routes.py:_push
 * PoP: w11_promptcach_850c92_sessions @ agent/prompt_cache_routes.py:_sessions
 * PoP: w11_promptcach_850c92_routine_hit @ agent/prompt_cache_routes.py:_routine_hit
 * PoP: w11_promptcach_850c92_learned_lifetime @ agent/prompt_cache_routes.py:learned_lifetime
 * PoP: w11_promptcach_850c92_display_rule @ agent/prompt_cache_routes.py:display_rule
 */
#include "noc_w11_promptcach_850c92.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_SAMPLES 2
#define REWRITE_SHARE 0.5
#define GAPS_KEPT 8

static int finite_num(json_t *v) {
    return json_is_number(v) && isfinite(v->num_val) ? 1 : 0;
}

static int truthy(json_t *v) {
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

/* Python round(x, 1) — printf's round-half-to-even matches it. */
static double py_round1(double x) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f", x);
    return strtod(buf, NULL);
}

/* Distinct sessions among entry[field] samples that `keep` marks true. */
static long long sessions_count(const json_t *entry, const char *field, const json_t *keep) {
    char name[160];
    snprintf(name, sizeof(name), "%s_sessions", field);
    json_t *values = json_obj_get(entry, field);
    json_t *sids = json_obj_get(entry, name);
    size_t nv = json_is_array(values) ? json_len(values) : 0;
    if (!nv) return 0;
    size_t ns = json_is_array(sids) ? json_len(sids) : 0;
    const char **seen = (const char **)calloc(nv, sizeof(*seen));
    if (!seen) return 0;
    size_t nseen = 0;
    for (size_t i = 0; i < nv; i++) {
        if (keep && json_is_array(keep) && i < json_len(keep) && !truthy(json_get(keep, i))) continue;
        const char *sid = "";
        if (ns >= nv) sid = json_string_value(json_get(sids, ns - nv + i));
        else if (i >= nv - ns) sid = json_string_value(json_get(sids, i - (nv - ns)));
        if (!sid) sid = "";
        int found = 0;
        for (size_t j = 0; j < nseen; j++) if (!strcmp(seen[j], sid)) { found = 1; break; }
        if (!found) seen[nseen++] = sid;
    }
    free(seen);
    return (long long)nseen;
}

/* PoP: w11_promptcach_850c92_finite @ agent/prompt_cache_routes.py:_finite */
json_t *w11_promptcach_850c92_finite(json_t *a) {
    return json_bool(finite_num(json_obj_get(a, "value")) ? true : false);
}

/* PoP: w11_promptcach_850c92_numbers @ agent/prompt_cache_routes.py:_numbers */
json_t *w11_promptcach_850c92_numbers(json_t *a) {
    json_t *v = json_obj_get(a, "value");
    if (!json_is_array(v)) return json_bool(false);
    for (size_t i = 0; i < json_len(v); i++)
        if (!finite_num(json_get(v, i))) return json_bool(false);
    return json_bool(true);
}

/* PoP: w11_promptcach_850c92_well_formed @ agent/prompt_cache_routes.py:_well_formed */
json_t *w11_promptcach_850c92_well_formed(json_t *a) {
    json_t *e = json_obj_get(a, "entry");
    if (!json_is_object(e)) return json_bool(false);
    if (!finite_num(json_obj_get(e, "updated"))) return json_bool(false);
    const char *counters[] = {"reads", "attempts", "write_on_hit", "samples", "longest_hit"};
    for (int i = 0; i < 5; i++) {
        json_t *v = json_obj_get(e, counters[i]);
        if (v && !finite_num(v)) return json_bool(false);   /* missing -> default 0 */
    }
    const char *gaps[] = {"hit_gaps", "miss_gaps", "write_on_hit_gaps"};
    for (int i = 0; i < 3; i++) {
        json_t *v = json_obj_get(e, gaps[i]);
        if (!v) continue;
        if (!json_is_array(v)) return json_bool(false);
        for (size_t j = 0; j < json_len(v); j++)
            if (!finite_num(json_get(v, j))) return json_bool(false);
    }
    const char *sessions[] = {"miss_gaps_sessions", "write_on_hit_gaps_sessions"};
    for (int i = 0; i < 2; i++) {
        json_t *v = json_obj_get(e, sessions[i]);
        if (!v) continue;
        if (!json_is_array(v)) return json_bool(false);
        for (size_t j = 0; j < json_len(v); j++)
            if (!json_is_string(json_get(v, j))) return json_bool(false);
    }
    return json_bool(true);
}

/* PoP: w11_promptcach_850c92_push @ agent/prompt_cache_routes.py:_push */
json_t *w11_promptcach_850c92_push(json_t *a) {
    json_t *src = json_obj_get(a, "entry");
    json_t *out = json_is_object(src) ? json_copy(src) : json_object();
    const char *field = json_get_str(a, "field", "hit_gaps");
    json_t *gap = json_obj_get(a, "gap");
    double g = json_is_number(gap) ? gap->num_val : 0.0;
    const char *sid = json_get_str(a, "session_id", "");

    json_t *arr = json_array();
    json_t *old = json_obj_get(out, field);
    if (json_is_array(old))
        for (size_t i = 0; i < json_len(old); i++) json_append(arr, json_copy(json_get(old, i)));
    json_append(arr, json_number(py_round1(g)));
    while (json_len(arr) > GAPS_KEPT) json_array_remove(arr, 0);
    json_set(out, field, arr);   /* frees previous value */

    if (strcmp(field, "hit_gaps") != 0) {
        char name[160];
        snprintf(name, sizeof(name), "%s_sessions", field);
        json_t *arr2 = json_array();
        json_t *old2 = json_obj_get(out, name);
        if (json_is_array(old2))
            for (size_t i = 0; i < json_len(old2); i++) json_append(arr2, json_copy(json_get(old2, i)));
        json_append(arr2, json_string(sid));
        while (json_len(arr2) > GAPS_KEPT) json_array_remove(arr2, 0);
        json_set(out, name, arr2);
    }
    return out;
}

/* PoP: w11_promptcach_850c92_sessions @ agent/prompt_cache_routes.py:_sessions */
json_t *w11_promptcach_850c92_sessions(json_t *a) {
    return json_number((double)sessions_count(
        json_obj_get(a, "entry"), json_get_str(a, "field", ""), json_obj_get(a, "keep")));
}

/* PoP: w11_promptcach_850c92_routine_hit @ agent/prompt_cache_routes.py:_routine_hit */
json_t *w11_promptcach_850c92_routine_hit(json_t *a) {
    json_t *e = json_obj_get(a, "entry");
    if ((long long)json_get_num(e, "reads", 0) < MIN_SAMPLES) return json_bool(false);
    double read = json_get_num(a, "read", 0), write = json_get_num(a, "write", 0);
    if (!(write < REWRITE_SHARE * read)) return json_bool(false);
    json_t *gap = json_obj_get(a, "gap_s");
    if (gap && !json_is_null(gap)) {
        double g = json_is_number(gap) ? gap->num_val : 0.0;
        json_t *hg = json_obj_get(e, "hit_gaps");
        double mx = 0.0;
        if (json_is_array(hg) && json_len(hg)) {
            mx = json_get(hg, 0)->num_val;
            for (size_t i = 1; i < json_len(hg); i++) {
                double x = json_get(hg, i)->num_val;
                if (x > mx) mx = x;
            }
        }
        if (!(g <= mx)) return json_bool(false);
    }
    double now = json_get_num(a, "now", 0);
    double updated = json_get_num(e, "updated", 0);
    if (!(now - updated < 24.0 * 3600.0)) return json_bool(false);
    return json_bool(true);
}

/* PoP: w11_promptcach_850c92_learned_lifetime @ agent/prompt_cache_routes.py:learned_lifetime */
json_t *w11_promptcach_850c92_learned_lifetime(json_t *a) {
    json_t *e = json_obj_get(a, "entry");
    double longest = json_get_num(e, "longest_hit", 0);
    json_t *hg = json_obj_get(e, "hit_gaps");
    if (json_is_array(hg)) for (size_t i = 0; i < json_len(hg); i++) {
        double x = json_get(hg, i)->num_val;
        if (x > longest) longest = x;
    }
    json_t *gaps = json_obj_get(e, "miss_gaps");
    size_t n = json_is_array(gaps) ? json_len(gaps) : 0;
    if (!n) return json_null();
    int *keep = (int *)calloc(n, sizeof(*keep));
    if (!keep) return json_null();
    int ne = 0;
    double mn = 0.0;
    double tier = json_get_num(a, "tier_s", 0);
    for (size_t i = 0; i < n; i++) {
        double g = json_get(gaps, i)->num_val;
        keep[i] = (longest < g && g < tier) ? 1 : 0;
        if (keep[i]) { if (!ne || g < mn) mn = g; ne++; }
    }
    int enough = ne >= MIN_SAMPLES;
    if (enough) {
        json_t *kj = json_array();
        for (size_t i = 0; i < n; i++) json_append(kj, json_bool(keep[i] ? true : false));
        if (sessions_count(e, "miss_gaps", kj) < MIN_SAMPLES) enough = 0;
        json_free(kj);
    }
    free(keep);
    if (!enough) return json_null();
    return json_number(mn);
}

/* PoP: w11_promptcach_850c92_display_rule @ agent/prompt_cache_routes.py:display_rule */
json_t *w11_promptcach_850c92_display_rule(json_t *a) {
    json_t *r = json_obj_get(a, "rule");
    if (!json_is_string(r) || !r->str_val || !r->str_val[0]) return json_null();
    if (!strncmp(r->str_val, "lifetime:", 9)) return json_string("short-lived");
    return json_string(r->str_val);
}
