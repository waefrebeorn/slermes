/*
 * web_server_idle_proof.c -- real C11 port of
 * hermes_cli/web_server_idle_proof.py (gate w21_webserveri_184aef): the idle
 * proof a Desktop-pooled `hermes serve` child is retired on, and the
 * human-input probe it sums.
 *
 * PoP: w21_webserveri_184aef_idle_proof @ hermes_cli/web_server_idle_proof.py:idle_proof
 * PoP: w21_webserveri_184aef_pending_human_input @ hermes_cli/web_server_idle_proof.py:pending_human_input
 *
 * The probes are upstream arguments (`turn_probe=lambda: busy_ledger`,
 * `input_probe=pending_human_input`); the port takes the value a probe
 * answered with, so the session/cron/request/approval ledgers stay upstream
 * as the database seam they are. An indeterminate probe is never idle.
 */
#include "hermes_json.h"
#include "noc_w21_webserveri_184aef.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a decoded JSON value; NULL (absent) reads as None. */
static int wsp184_truthy(const json_t *v) {
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

/* `turn is None`: absent or a JSON null. */
static int wsp184_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

json_t *w21_webserveri_184aef_idle_proof(const json_t *turn, const json_t *pending) {
    json_t *out;
    if (wsp184_is_none(turn)) {
        out = json_object();
        json_set(out, "idle", json_null());
        json_set(out, "reason", json_string("turn_probe_unavailable"));
        return out;
    }
    if (wsp184_truthy(turn)) {
        out = json_object();
        json_set(out, "idle", json_bool(0));
        json_set(out, "reason", json_string("turn_in_flight"));
        /* `turn if isinstance(turn, str) else None` */
        if (turn->type == JSON_STRING) {
            json_set(out, "detail", json_string(turn->str_val ? turn->str_val : ""));
        } else {
            json_set(out, "detail", json_null());
        }
        return out;
    }
    if (wsp184_is_none(pending)) {
        out = json_object();
        json_set(out, "idle", json_null());
        json_set(out, "reason", json_string("input_probe_unavailable"));
        return out;
    }
    if (wsp184_truthy(pending)) {
        out = json_object();
        json_set(out, "idle", json_bool(0));
        json_set(out, "reason", json_string("awaiting_human_input"));
        return out;
    }
    out = json_object();
    json_set(out, "idle", json_bool(1));
    json_set(out, "reason", json_null());
    return out;
}

/* `int(value)`: a JSON number floors toward zero, as int() does. */
static long long wsp184_ll(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return 0;
    return (long long)v->num_val;
}

json_t *w21_webserveri_184aef_pending_human_input(const json_t *server_requests,
                                                  const json_t *approvals,
                                                  int unavailable) {
    long long total;
    if (unavailable) return json_null();
    total = wsp184_ll(server_requests) + wsp184_ll(approvals);
    return json_number((double)total);
}
