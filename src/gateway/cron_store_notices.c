/*
 * cron_store_notices.c — C home for upstream Hermes gateway/cron_store_notices.py.
 * PoP: w13_cronstore_1b947c_notice_facts @ gateway/cron_store_notices.py:install_cron_store_notices
 *
 * The loop-only state machine that decides which degraded-store transition is
 * announced, when a "recovered" notice is deferred, and which stale notices are
 * dropped/cancelled.  Delivery itself (profile scope, i18n, home-channel send)
 * stays in Python; its result is a recorded action seam.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_cronstore_1b947c.h"

#define CS_MAX_STORES 64
#define CS_MAX_ACTIONS 128

static const char *cs_announced[CS_MAX_STORES];
static int cs_nannounced;
static const char *cs_pending[CS_MAX_STORES];
static int cs_pending_active[CS_MAX_STORES];
static char *cs_actions[CS_MAX_ACTIONS];
static int cs_nactions;

static void cs_reset(void) {
    cs_nannounced = 0;
    memset(cs_pending_active, 0, sizeof(cs_pending_active));
    cs_nactions = 0;
}
static int cs_in(const char *arr[], int n, const char *s) {
    for (int i = 0; i < n; i++) if (arr[i] && !strcmp(arr[i], s)) return 1;
    return 0;
}
static void cs_ann_add(const char *s) { if (!cs_in(cs_announced, cs_nannounced, s) && cs_nannounced < CS_MAX_STORES) cs_announced[cs_nannounced++] = s; }
static void cs_ann_del(const char *s) {
    for (int i = 0; i < cs_nannounced; i++) if (!strcmp(cs_announced[i], s)) {
        for (int j = i; j + 1 < cs_nannounced; j++) cs_announced[j] = cs_announced[j + 1];
        cs_nannounced--;
        return;
    }
}
static int cs_pend_find(const char *s) {
    for (int i = 0; i < CS_MAX_STORES; i++)
        if (cs_pending_active[i] && cs_pending[i] && !strcmp(cs_pending[i], s)) return i;
    return -1;
}
static void cs_pend_add(const char *s) {
    for (int i = 0; i < CS_MAX_STORES; i++) if (!cs_pending_active[i]) {
        cs_pending[i] = s; cs_pending_active[i] = 1; return;
    }
}
static void cs_act(const char *kind, const char *event, const char *store) {
    if (cs_nactions >= CS_MAX_ACTIONS) return;
    char b[1024];
    if (event) snprintf(b, sizeof(b), "%s:%s:%s", kind, event, store);
    else snprintf(b, sizeof(b), "%s:%s", kind, store);
    size_t n = strlen(b);
    char *o = (char *)malloc(n + 1);
    memcpy(o, b, n + 1);
    cs_actions[cs_nactions++] = o;
}

/* deliver() re-check + send seam. */
static void cs_deliver(const char *event, const char *store, int degraded) {
    if (!strcmp(event, "unwritable") && !degraded) {
        cs_ann_del(store);
        int pi = cs_pend_find(store);
        if (pi >= 0) { cs_act("cancel", NULL, store); cs_pending_active[pi] = 0; }
        return;
    }
    if (!strcmp(event, "recovered")) {
        if (degraded || !cs_in(cs_announced, cs_nannounced, store)) return;
        cs_ann_del(store);
    }
    cs_act("send", event, store);
}
/* send_recovered(): pop the timer, then deliver() decides whether to send. */
static void cs_send_recovered(const char *store, int degraded) {
    int pi = cs_pend_find(store);
    if (pi >= 0) cs_pending_active[pi] = 0; /* pop, no cancel action */
    if (cs_in(cs_announced, cs_nannounced, store)) cs_deliver("recovered", store, degraded);
}
/* on_loop(): the installed transition listener's decision tree. */
static void cs_on_loop(const char *event, const char *store, int degraded) {
    if (!strcmp(event, "unwritable")) {
        int pi = cs_pend_find(store);
        if (pi >= 0) { cs_act("cancel", NULL, store); cs_pending_active[pi] = 0; }
        else if (!cs_in(cs_announced, cs_nannounced, store)) {
            cs_ann_add(store);
            cs_deliver("unwritable", store, degraded);
        }
    } else if (cs_in(cs_announced, cs_nannounced, store) && cs_pend_find(store) < 0) {
        cs_pend_add(store);
        cs_act("schedule", NULL, store);
    }
}
static int cs_degraded(json_t *degraded, const char *store) {
    json_t *v = json_obj_get(degraded, store);
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}

json_t *w13_cronstore_1b947c_notice_facts(json_t *boot, json_t *degraded, json_t *events) {
    cs_reset();
    size_t bn = (boot && boot->type == JSON_ARRAY) ? boot->c.count : 0;
    for (size_t i = 0; i < bn; i++) {
        const char *s = (boot->c.items[i] && boot->c.items[i]->type == JSON_STRING) ? boot->c.items[i]->str_val : NULL;
        if (s) cs_on_loop("unwritable", s, cs_degraded(degraded, s));
    }
    size_t en = (events && events->type == JSON_ARRAY) ? events->c.count : 0;
    for (size_t i = 0; i < en; i++) {
        json_t *e = events->c.items[i];
        const char *kind = (e && e->type == JSON_OBJECT) ? json_get_str(e, "kind", "") : "";
        const char *store = (e && e->type == JSON_OBJECT) ? json_get_str(e, "store", "") : "";
        if (!strcmp(kind, "fire")) cs_send_recovered(store, cs_degraded(degraded, store));
        else cs_on_loop(kind, store, cs_degraded(degraded, store));
    }
    json_t *out = json_array();
    for (int i = 0; i < cs_nactions; i++) {
        json_append(out, json_string(cs_actions[i]));
        free(cs_actions[i]);
    }
    cs_nactions = 0;
    return out;
}
