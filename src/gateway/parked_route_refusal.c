/*
 * parked_route_refusal.c — C port of upstream Hermes gateway/parked_route_refusal.py.
 *
 * PoP: w16_parkedrout_cc3fed_refused_info @ gateway/parked_route_refusal.py:ParkedRouteRefused
 * PoP: w16_parkedrout_cc3fed_profile_refusal @ gateway/parked_route_refusal.py:ParkedProfileRefusal
 * PoP: w16_parkedrout_cc3fed_notice_due @ gateway/parked_route_refusal.py:_notice_due
 * PoP: w16_parkedrout_cc3fed_constants @ gateway/parked_route_refusal.py:PARKED_ROUTE_NOTICE
 *
 * The notice-dedup window is exercised with an injected monotonic `now` seam; the
 * asyncio/transport send paths are outside this gate.
 */
#include "noc_w16_parkedrout_cc3fed.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}
/* Python str(v) for the scalar values used as notice keys. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

json_t *w16_parkedrout_cc3fed_refused_info(void) {
    json_t *o = json_object();
    json_set(o, "code", json_string("profile_parked"));
    json_set(o, "status", json_number(503.0));
    json_t *h = json_object();
    json_set(h, "Retry-After", json_string("30"));
    json_set(o, "headers", h);
    json_set(o, "message", json_string("The routed profile is not being served right now; retry this delivery"));
    return o;
}

json_t *w16_parkedrout_cc3fed_profile_refusal(const json_t *profile) {
    json_t *o = json_object();
    json_set(o, "reason", json_string("runtime_draining"));
    json_set(o, "field", json_string("profile"));
    json_set(o, "detail", json_string("the routed profile is parked"));
    json_set(o, "profile", json_copy(profile));
    return o;
}

json_t *w16_parkedrout_cc3fed_constants(void) {
    json_t *o = json_object();
    json_set(o, "notice", json_string("The agent is currently unavailable; please resend your message in a moment."));
    json_set(o, "window", json_number(60.0));
    return o;
}

typedef struct { char *p, *pf, *c; double at; } SentEntry;
static int cmp_entry(const SentEntry *a, const SentEntry *b) {
    int r = strcmp(a->p, b->p); if (r) return r;
    r = strcmp(a->pf, b->pf);   if (r) return r;
    return strcmp(a->c, b->c);
}

/* _notice_due(adapter, profile, platform, chat_id) with _monotonic()==now injected. */
json_t *w16_parkedrout_cc3fed_notice_due(const json_t *sent_in, const json_t *now_v,
                                        const json_t *profile, const json_t *platform,
                                        const json_t *chat_id) {
    double now = json_is_number(now_v) ? now_v->num_val : 0.0;
    char *p = py_str(profile), *pf = py_str(platform), *ch = py_str(chat_id);
    SentEntry es[512]; size_t n = 0;
    if (json_is_array(sent_in)) {
        for (size_t i = 0; i < json_len(sent_in) && n < 512; i++) {
            const json_t *row = json_get(sent_in, i);
            if (!json_is_array(row) || json_len(row) < 4) continue;
            es[n].p = py_str(json_get(row, 0));
            es[n].pf = py_str(json_get(row, 1));
            es[n].c = py_str(json_get(row, 2));
            const json_t *at = json_get(row, 3);
            es[n].at = json_is_number(at) ? at->num_val : 0.0;
            n++;
        }
    }
    /* prune every key whose age >= PARKED_NOTICE_WINDOW_S (60.0) */
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        if (now - es[i].at >= 60.0) {
            free(es[i].p); free(es[i].pf); free(es[i].c);
        } else {
            es[w++] = es[i];
        }
    }
    n = w;
    int present = 0;
    for (size_t i = 0; i < n; i++) {
        if (!strcmp(es[i].p, p) && !strcmp(es[i].pf, pf) && !strcmp(es[i].c, ch)) { present = 1; break; }
    }
    int due = !present;
    if (due && n < 512) {
        es[n].p = dupstr(p); es[n].pf = dupstr(pf); es[n].c = dupstr(ch); es[n].at = now; n++;
    }
    for (size_t i = 1; i < n; i++) {
        SentEntry k = es[i]; size_t j = i;
        while (j > 0 && cmp_entry(&es[j - 1], &k) > 0) { es[j] = es[j - 1]; j--; }
        es[j] = k;
    }
    json_t *out = json_object();
    json_set(out, "due", json_bool(due));
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *row = json_array();
        json_append(row, json_string(es[i].p));
        json_append(row, json_string(es[i].pf));
        json_append(row, json_string(es[i].c));
        json_append(row, json_number(es[i].at));
        json_append(arr, row);
    }
    json_set(out, "sent", arr);
    for (size_t i = 0; i < n; i++) { free(es[i].p); free(es[i].pf); free(es[i].c); }
    free(p); free(pf); free(ch);
    return out;
}
