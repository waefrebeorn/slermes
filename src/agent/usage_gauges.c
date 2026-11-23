/*
 * usage_gauges.c — C port of agent/usage_gauges.py (gate w6_usagegauges_1dd126).
 * PoP: w6_usagegauges_1dd126_paused_for_payment @ agent/usage_gauges.py:paused_for_payment
 * PoP: w6_usagegauges_1dd126_parse_gauges @ agent/usage_gauges.py:parse_gauges
 * PoP: w6_usagegauges_1dd126_gauge_title @ agent/usage_gauges.py:gauge_title
 * PoP: w6_usagegauges_1dd126_gauge_limit_name @ agent/usage_gauges.py:gauge_limit_name
 * PoP: w6_usagegauges_1dd126_gauge_short @ agent/usage_gauges.py:gauge_short
 * PoP: w6_usagegauges_1dd126_all_scope @ agent/usage_gauges.py:all_scope
 * PoP: w6_usagegauges_1dd126_top_tier_gauge @ agent/usage_gauges.py:top_tier_gauge
 * PoP: w6_usagegauges_1dd126_binding_gauge @ agent/usage_gauges.py:binding_gauge
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "noc_w6_usagegauges_1dd126.h"

#define BP_FULL 10000
#define RESET_MAX_AHEAD_MS (366LL * 86400000LL)

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }

/* Python str.strip() (ASCII whitespace is enough for the wire data). */
static char *strip_ws(const char *s) {
    if (!s) return NULL;
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    size_t n = (size_t)(b - a);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, a, n);
    r[n] = '\0';
    return r;
}

static struct json_t *gauge_obj(const char *id, const char *scope, const char *label,
                                long long bp, int has_reset, long long reset) {
    struct json_t *o = json_object();
    json_set(o, "id", json_string(id));
    json_set(o, "scope", json_string(scope));
    json_set(o, "label", json_string(label));
    json_set(o, "used_bp", json_number((double)bp));
    json_set(o, "resets_at_ms", has_reset ? json_number((double)reset) : json_null());
    json_set(o, "full", json_bool(bp >= BP_FULL));
    double frac = (double)bp / (double)BP_FULL;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    json_set(o, "used_fraction", json_number(frac));
    return o;
}

int w6_usagegauges_1dd126_paused_for_payment(const char *reason) {
    return reason && !strcmp(reason, "payment_past_due") ? 1 : 0;
}

struct json_t *w6_usagegauges_1dd126_parse_gauges(const struct json_t *value) {
    if (!value) return NULL;
    const struct json_t *src = value;
    struct json_t *parsed = NULL;
    if (value->type == JSON_STRING) {
        parsed = json_parse(value->str_val, NULL);
        if (!parsed) return NULL;
        src = parsed;
    }
    if (src->type != JSON_ARRAY) { json_free(parsed); return NULL; }

    long long limit_ms = (long long)time(NULL) * 1000 + RESET_MAX_AHEAD_MS;
    struct json_t *out = json_array();
    char **seen = NULL;
    size_t nseen = 0;

    for (size_t i = 0; i < json_len(src); i++) {
        const struct json_t *it = json_get(src, i);
        if (!it || it->type != JSON_OBJECT) continue;
        const struct json_t *jid = json_obj_get(it, "id");
        const struct json_t *jsc = json_obj_get(it, "scope");
        const struct json_t *jbp = json_obj_get(it, "used_bp");
        if (!jid || jid->type != JSON_STRING || !jid->str_val || !jid->str_val[0]) continue;
        const char *gid = jid->str_val;
        int dup = 0;
        for (size_t k = 0; k < nseen; k++) {
            if (!strcmp(seen[k], gid)) { dup = 1; break; }
        }
        if (dup) continue;
        if (!jsc || jsc->type != JSON_STRING || !jsc->str_val) continue;
        const char *scope = jsc->str_val;
        if (strcmp(scope, "all") && strcmp(scope, "top_tier")) continue;
        if (!jbp || jbp->type != JSON_NUMBER) continue; /* JSON_BOOL is a distinct type */
        double bpd = jbp->num_val;
        if (bpd != (double)(long long)bpd) continue;
        long long bp = (long long)bpd;
        if (bp < 0 || bp > BP_FULL) continue;

        int has_reset = 0;
        long long reset = 0;
        const struct json_t *jr = json_obj_get(it, "resets_at_ms");
        if (jr && jr->type == JSON_NUMBER && jr->num_val == (double)(long long)jr->num_val) {
            long long rv = (long long)jr->num_val;
            if (rv > 0 && rv <= limit_ms) { has_reset = 1; reset = rv; }
        }

        char *label = dupstr(gid);
        const struct json_t *jl = json_obj_get(it, "label");
        if (jl && jl->type == JSON_STRING && jl->str_val) {
            char *st = strip_ws(jl->str_val);
            if (st && st[0]) { free(label); label = st; }
            else free(st);
        }

        char **ns = (char **)realloc(seen, (nseen + 1) * sizeof(char *));
        if (ns) { seen = ns; seen[nseen++] = dupstr(gid); }
        json_append(out, gauge_obj(gid, scope, label, bp, has_reset, reset));
        free(label);
    }
    for (size_t k = 0; k < nseen; k++) free(seen[k]);
    free(seen);
    json_free(parsed);
    if (json_len(out) == 0) { json_free(out); return NULL; }
    return out;
}

/* Hermes's words for the ids it knows: (title, "for", limit name, short). */
static const char * const *words_for(const char *id) {
    static const char *const day[]    = { "Today", "today", "daily", "today" };
    static const char *const week[]   = { "This week", "this week", "weekly", "week" };
    static const char *const month[]  = { "This month", "this month", "monthly", "month" };
    static const char *const top[]    = { "Large models (monthly)", "your monthly large-model usage",
                                          "monthly large-model", "large models (monthly)" };
    if (!id) return NULL;
    if (!strcmp(id, "day")) return day;
    if (!strcmp(id, "week")) return week;
    if (!strcmp(id, "month")) return month;
    if (!strcmp(id, "top_tier")) return top;
    return NULL;
}
static char *word_or_label(const char *id, const char *label, int idx) {
    const char *const *w = words_for(id);
    return dupstr(w ? w[idx] : (label ? label : ""));
}
char *w6_usagegauges_1dd126_gauge_title(const char *id, const char *label) {
    return word_or_label(id, label, 0);
}
char *w6_usagegauges_1dd126_gauge_limit_name(const char *id, const char *label) {
    return word_or_label(id, label, 2);
}
char *w6_usagegauges_1dd126_gauge_short(const char *id, const char *label) {
    return word_or_label(id, label, 3);
}

static const char *scope_of(const struct json_t *g) {
    const struct json_t *s = g ? json_obj_get(g, "scope") : NULL;
    return (s && s->type == JSON_STRING) ? s->str_val : NULL;
}
struct json_t *w6_usagegauges_1dd126_all_scope(const struct json_t *gauges) {
    struct json_t *out = json_array();
    if (!gauges) return out;
    for (size_t i = 0; i < json_len(gauges); i++) {
        const struct json_t *g = json_get(gauges, i);
        const char *s = scope_of(g);
        if (s && !strcmp(s, "all")) json_append(out, json_copy(g));
    }
    return out;
}
struct json_t *w6_usagegauges_1dd126_top_tier_gauge(const struct json_t *gauges) {
    if (!gauges) return NULL;
    for (size_t i = 0; i < json_len(gauges); i++) {
        const struct json_t *g = json_get(gauges, i);
        const char *s = scope_of(g);
        if (s && !strcmp(s, "top_tier")) return json_copy(g);
    }
    return NULL;
}
struct json_t *w6_usagegauges_1dd126_binding_gauge(const struct json_t *gauges) {
    if (!gauges) return NULL;
    const struct json_t *best = NULL;
    double best_key = 0;
    int any = 0;
    /* Pass 1: full all-scope gauges, keyed by resets_at_ms or 0. */
    for (size_t i = 0; i < json_len(gauges); i++) {
        const struct json_t *g = json_get(gauges, i);
        const char *s = scope_of(g);
        if (!s || strcmp(s, "all")) continue;
        const struct json_t *jf = json_obj_get(g, "full");
        if (!jf || !jf->bool_val) continue;
        double key = 0;
        const struct json_t *jr = json_obj_get(g, "resets_at_ms");
        if (jr && jr->type == JSON_NUMBER) key = jr->num_val;
        if (!any || key > best_key) { any = 1; best = g; best_key = key; }
    }
    if (any) return json_copy(best);
    /* Pass 2: no full gauge -> the fullest, keyed by used_bp (ties -> first). */
    best = NULL;
    best_key = 0;
    for (size_t i = 0; i < json_len(gauges); i++) {
        const struct json_t *g = json_get(gauges, i);
        const char *s = scope_of(g);
        if (!s || strcmp(s, "all")) continue;
        double key = 0;
        const struct json_t *jb = json_obj_get(g, "used_bp");
        if (jb && jb->type == JSON_NUMBER) key = jb->num_val;
        if (!best || key > best_key) { best = g; best_key = key; }
    }
    return best ? json_copy(best) : NULL;
}
