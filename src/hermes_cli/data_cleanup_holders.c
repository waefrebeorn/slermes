/*
 * data_cleanup_holders.c — C11 port of upstream Hermes
 * hermes_cli/data_cleanup_holders.py (gate w26_datacleanu_bcef92).
 *
 * PoP: w26_datacleanu_bcef92_pause_acked @ hermes_cli/data_cleanup_holders.py:pause_acked
 * PoP: w26_datacleanu_bcef92_owner_stop_hint @ hermes_cli/data_cleanup_holders.py:_OWNER_STOP_HINTS
 * PoP: w26_datacleanu_bcef92_drain_timeout @ hermes_cli/data_cleanup_holders.py:_drain_manual_gateway
 * PoP: w26_datacleanu_bcef92_backend_owner_state @ hermes_cli/data_cleanup_holders.py:_refuse_backend_writers
 * PoP: w26_datacleanu_bcef92_cron_owner_state @ hermes_cli/data_cleanup_holders.py:_refuse_cron_writers
 * PoP: w26_datacleanu_bcef92_constants @ hermes_cli/data_cleanup_holders.py:_OWNER_STOP_HINTS
 *
 * The control sockets, the pid probes, the sqlite ledgers and the lock files are
 * seams.  What is ported is the decision each probe's answer feeds: whether the
 * pause was acked by the gateway we identified, what advice the owner deserves,
 * how long the drain deadline is, and what each ledger row means.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_datacleanu_bcef92.h"

static const struct { const char *owner; const char *hint; } CLEANUP_OWNER_HINTS[] = {
    {"desktop", "quit the owning desktop app before retrying"},
    {"external", "stop the gateway through its external supervisor before retrying"},
    {"launchd", "stop its LaunchAgent with launchctl before retrying"},
    {"systemd", "stop its systemd unit with systemctl before retrying"},
};
#define CLEANUP_OWNER_HINT_COUNT (sizeof(CLEANUP_OWNER_HINTS) / sizeof(CLEANUP_OWNER_HINTS[0]))
#define CLEANUP_OWNER_HINT_DEFAULT "stop it through its supervisor before retrying"

/* Python truthiness of a JSON value. */
static int dc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python '==' over the scalar shapes a pid / start-time can hold.  An absent
 * key and an explicit null are the same element (dict.get -> None). */
static int dc_equal(const json_t *a, const json_t *b) {
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* type(x) is int -- bools and floats are not, exactly as Python's identity test. */
static int dc_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && v->num_val == (double)(long long)v->num_val;
}

json_t *w26_datacleanu_bcef92_pause_acked(const json_t *response, const json_t *identity) {
    const json_t *resp_pid, *ident_pid;
    if (!dc_truthy(response)) return json_bool(false);
    if (!identity || identity->type == JSON_NULL) return json_bool(false);
    if (identity->type != JSON_OBJECT || response->type != JSON_OBJECT) return json_bool(false);
    resp_pid = json_obj_get(response, "pid");
    ident_pid = json_obj_get(identity, "pid");
    if (!dc_equal(resp_pid, ident_pid)) return json_bool(false);
    if (!dc_truthy(json_obj_get(response, "pausing")) &&
        !dc_truthy(json_obj_get(response, "already_stopping")))
        return json_bool(false);
    return json_bool(true);
}

json_t *w26_datacleanu_bcef92_owner_stop_hint(const json_t *owner) {
    size_t i;
    const char *name = (owner && owner->type == JSON_STRING) ? owner->str_val : NULL;
    if (!name || !name[0]) return json_string(CLEANUP_OWNER_HINT_DEFAULT);
    for (i = 0; i < CLEANUP_OWNER_HINT_COUNT; i++)
        if (!strcmp(name, CLEANUP_OWNER_HINTS[i].owner)) return json_string(CLEANUP_OWNER_HINTS[i].hint);
    return json_string(CLEANUP_OWNER_HINT_DEFAULT);
}

json_t *w26_datacleanu_bcef92_drain_timeout(const json_t *response) {
    const json_t *raw;
    double drain = 30.0, out;

    raw = (response && response->type == JSON_OBJECT) ? json_obj_get(response, "drain_timeout") : NULL;
    if (raw) {
        if (raw->type == JSON_NUMBER) drain = raw->num_val;
        else if (raw->type == JSON_BOOL) drain = raw->bool_val ? 1.0 : 0.0;
        else drain = 30.0; /* non-numeric: the live float() raises; keep the default */
    }
    out = drain + 5.0;
    if (out < 5.0) out = 5.0;
    if (out > 120.0) out = 120.0;
    return json_number(out);
}

json_t *w26_datacleanu_bcef92_backend_owner_state(const json_t *row, int running) {
    const json_t *purpose, *pid;
    if (!row || row->type != JSON_OBJECT) return json_string("skip");
    purpose = json_obj_get(row, "purpose");
    if (!purpose || purpose->type != JSON_STRING ||
        (strcmp(purpose->str_val ? purpose->str_val : "", "serve") &&
         strcmp(purpose->str_val ? purpose->str_val : "", "dashboard")))
        return json_string("skip");
    pid = json_obj_get(row, "pid");
    if (!dc_is_int(pid) || pid->num_val <= 0.0) return json_string("invalid_pid");
    if (running) return json_string("running");
    return json_string("free");
}

json_t *w26_datacleanu_bcef92_cron_owner_state(const json_t *row, int pid_exists,
                                               const json_t *current_start) {
    const json_t *recorded;
    if (!row || row->type != JSON_OBJECT) return json_string("skip");
    if (dc_truthy(json_obj_get(row, "handoff_pending"))) return json_string("handoff");
    if (!pid_exists) return json_string("skip");
    recorded = json_obj_get(row, "process_started_at");
    if (!current_start || current_start->type == JSON_NULL || !recorded || recorded->type == JSON_NULL)
        return json_string("owns");
    if (dc_equal(current_start, recorded)) return json_string("owns");
    return json_string("free");
}

json_t *w26_datacleanu_bcef92_constants(void) {
    json_t *o = json_object();
    json_set(o, "owner_stop_hint_default", json_string(CLEANUP_OWNER_HINT_DEFAULT));
    {
        json_t *hints = json_object();
        size_t i;
        for (i = 0; i < CLEANUP_OWNER_HINT_COUNT; i++)
            json_set(hints, CLEANUP_OWNER_HINTS[i].owner, json_string(CLEANUP_OWNER_HINTS[i].hint));
        json_set(o, "owner_stop_hints", hints);
    }
    return o;
}
