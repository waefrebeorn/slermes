/*
 * shared_metrics_subscriber.c -- C11 port of
 * hermes_cli/observability/shared_metrics_subscriber.py.
 *
 * PoP: w20_sharedmetr_d8839c_call @ hermes_cli/observability/shared_metrics_subscriber.py:SharedMetricsSubscriber.__call__
 * PoP: w20_sharedmetr_d8839c_persist_kind @ hermes_cli/observability/shared_metrics_subscriber.py:_persist
 *
 * The store, the Relay thread and the contract's projections stay upstream. What
 * is ported is everything the subscriber decides before it writes: which events
 * it accepts (runtime instance filter), which counters it folds into the
 * engagement rollup, which store method each counter routes to, the
 * once-per-install milestone/feature latches, and the commit-ticket count that
 * only advances while every row of the event settled.
 */
#include "hermes_json.h"
#include "noc_w20_sharedmetr_d8839c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SM_COMMIT_TICKET_KEY "hermes.shared_metrics.commit_ticket"
#define SM_RUNTIME_KEY       "hermes.relay.runtime_instance"
#define SM_CLIENT_ACTIVE     "hermes.client.active"
#define SM_FEATURE_USED      "hermes.feature.used"
#define SM_FEATURE_DISABLED  "hermes.feature_disabled.count"
#define SM_INSTALL_SNAPSHOT  "hermes.install.snapshot"

static int sm_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static int sm_list_has(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (i = 0; i < arr->c.count; i++) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

const char *w20_sharedmetr_d8839c_persist_kind(const char *metric_name) {
    if (!metric_name) return "counter";
    if (!strcmp(metric_name, SM_FEATURE_USED)) return "noop";
    if (!strcmp(metric_name, SM_FEATURE_DISABLED)) return "once_per_day";
    if (!strcmp(metric_name, SM_CLIENT_ACTIVE)) return "client_active";
    if (!strcmp(metric_name, SM_INSTALL_SNAPSHOT)) return "install_snapshot";
    return "counter";
}

typedef struct {
    json_t *persisted;
    json_t *milestones;
    json_t *features;
    json_t *tickets;
    const json_t *req;
    int saved;
    int active;
    int stopped;
} sm_state;

/* the fixture's answer for milestones_for(metric, dimensions) */
static json_t *sm_for(const json_t *req, const char *which, const char *metric) {
    json_t *table = json_obj_get(req, which);
    if (!table || table->type != JSON_OBJECT) return NULL;
    return json_obj_get(table, metric);
}

/* One row per store call, shaped by what that call is handed: record_counter
 * takes (metric, dimensions, amount), once_per_day and install_snapshot drop the
 * amount, record_client_active takes only the resource, feature_used writes
 * nothing at all. */
static void sm_record(sm_state *st, const char *metric, const json_t *dims, double amount) {
    const char *kind = w20_sharedmetr_d8839c_persist_kind(metric);
    json_t *row = json_object();
    if (!strcmp(kind, "noop")) { json_free(row); return; }
    json_set(row, "kind", json_string(kind));
    if (!strcmp(kind, "counter")) {
        json_set(row, "metric", json_string(metric));
        json_set(row, "dimensions", dims ? json_copy(dims) : json_object());
        json_set(row, "amount", json_number(amount));
    } else if (!strcmp(kind, "once_per_day")) {
        json_set(row, "metric", json_string(metric));
        json_set(row, "dimensions", dims ? json_copy(dims) : json_object());
    } else if (!strcmp(kind, "install_snapshot")) {
        json_set(row, "dimensions", dims ? json_copy(dims) : json_object());
    }
    json_append(st->persisted, row);
}

/* _record_milestones: latch each reached milestone once per install. 0 on a
 * store error (the caller's `except Exception` turns that into saved=False). */
static int sm_record_milestones(sm_state *st, const char *metric) {
    json_t *reached = sm_for(st->req, "milestones_for", metric);
    json_t *done = json_obj_get(st->req, "milestones_done");
    size_t i;
    int any = 0;
    if (!reached || reached->type != JSON_ARRAY) return 1;
    for (i = 0; i < reached->c.count; i++) {
        json_t *m = reached->c.items[i];
        if (!m || m->type != JSON_STRING) continue;
        if (sm_list_has(done, m->str_val)) continue;
        any = 1;
    }
    if (!any) return 1;
    if (json_get_bool(st->req, "milestone_error", 0)) return 0;
    for (i = 0; i < reached->c.count; i++) {
        json_t *m = reached->c.items[i];
        if (!m || m->type != JSON_STRING) continue;
        if (sm_list_has(done, m->str_val)) continue;
        json_append(st->milestones, json_string(m->str_val));
    }
    return 1;
}

/* _record_features: a failed latch is logged and retried on a later use. */
static void sm_record_features(sm_state *st, const char *metric) {
    json_t *reached = sm_for(st->req, "features_for", metric);
    json_t *done = json_obj_get(st->req, "features_done");
    size_t i;
    if (!reached || reached->type != JSON_ARRAY) return;
    if (json_get_bool(st->req, "feature_error", 0)) return;
    for (i = 0; i < reached->c.count; i++) {
        json_t *f = reached->c.items[i];
        if (!f || f->type != JSON_STRING) continue;
        if (sm_list_has(done, f->str_val)) continue;
        json_append(st->features, json_string(f->str_val));
    }
}

json_t *w20_sharedmetr_d8839c_call(const json_t *req) {
    json_t *event = json_obj_get(req, "event");
    json_t *metadata = event ? json_obj_get(event, "metadata") : NULL;
    json_t *runtime = json_obj_get(req, "runtime_id");
    json_t *classified = json_obj_get(req, "classified");
    json_t *engagement = json_obj_get(req, "engagement");
    json_t *ticket = NULL;
    sm_state st;
    json_t *value, *out, *eng_out = NULL;
    const char *take = json_get_str(req, "take", NULL);
    json_t *taken = NULL;
    size_t i;
    int filtered = 0;

    memset(&st, 0, sizeof(st));
    st.req = req;
    st.persisted = json_array();
    st.milestones = json_array();
    st.features = json_array();
    st.tickets = json_object();
    st.active = json_get_bool(req, "active", 1);
    st.saved = 1;

    if (runtime && runtime->type != JSON_NULL) {
        const char *rid = runtime->type == JSON_STRING ? runtime->str_val : NULL;
        json_t *got = (metadata && metadata->type == JSON_OBJECT)
                          ? json_obj_get(metadata, SM_RUNTIME_KEY) : NULL;
        if (!rid || !got || got->type != JSON_STRING || strcmp(got->str_val, rid)) {
            filtered = 1;
            goto done;
        }
    }
    ticket = (metadata && metadata->type == JSON_OBJECT)
                 ? json_obj_get(metadata, SM_COMMIT_TICKET_KEY) : NULL;

    /* _record_engagement: a turn start/end surface or an attended turn's route. */
    if (engagement && engagement->type == JSON_OBJECT) {
        json_t *surface = json_obj_get(engagement, "surface");
        json_t *route = json_obj_get(engagement, "route");
        if (sm_truthy(surface) || sm_truthy(route)) {
            if (st.active) {
                eng_out = json_object();
                json_set(eng_out, "surface", surface ? json_copy(surface) : json_null());
                json_set(eng_out, "route", route ? json_copy(route) : json_null());
            }
        }
    }

    for (i = 0; classified && classified->type == JSON_ARRAY && i < classified->c.count; i++) {
        json_t *triple = classified->c.items[i];
        const char *metric;
        json_t *dims;
        double amount;
        int triple_ok = 1;
        if (!triple || triple->type != JSON_ARRAY || triple->c.count < 3) continue;
        metric = (triple->c.items[0] && triple->c.items[0]->type == JSON_STRING)
                     ? triple->c.items[0]->str_val : "";
        dims = triple->c.items[1];
        amount = (triple->c.items[2] && triple->c.items[2]->type == JSON_NUMBER)
                     ? triple->c.items[2]->num_val : 0.0;
        if (!st.active) {                 /* `if not self._active: return` */
            st.stopped = 1;
            goto done;
        }
        if (sm_list_has(json_obj_get(req, "store_errors"), metric)) {
            st.saved = 0;
            continue;
        }
        sm_record(&st, metric, dims, amount);
        if (!sm_record_milestones(&st, metric)) triple_ok = 0;
        if (triple_ok) sm_record_features(&st, metric);
        if (!triple_ok) st.saved = 0;
    }

    if (sm_truthy(ticket) && st.saved && st.active) {
        const char *key = (ticket->type == JSON_STRING) ? ticket->str_val : "";
        double n = json_get_num(st.tickets, key, 0.0);
        json_set(st.tickets, key, json_number(n + 1));
    }

done:
    /* take_saved(ticket): how many settled events rode that ticket, then forget it. */
    if (take && take[0]) {
        taken = json_number(json_get_num(st.tickets, take, 0.0));
        json_obj_del(st.tickets, take);
    }
    value = json_object();
    json_set(value, "filtered", json_bool(filtered ? 1 : 0));
    json_set(value, "engagement", eng_out ? eng_out : json_null());
    json_set(value, "persisted", st.persisted);
    json_set(value, "milestones", st.milestones);
    json_set(value, "features", st.features);
    json_set(value, "tickets", st.tickets);
    json_set(value, "active", json_bool(st.active ? 1 : 0));
    json_set(value, "stopped", json_bool(st.stopped ? 1 : 0));
    json_set(value, "taken", taken ? taken : json_null());
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    return out;
}
