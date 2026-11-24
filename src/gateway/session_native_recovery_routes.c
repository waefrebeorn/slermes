/*
 * session_native_recovery_routes.c - C11 port of upstream
 * gateway/session_native_recovery_routes.py (gate w24_sessionnat_5f7f78).
 *
 * PoP: w24_sessionnat_5f7f78_relay_match @ gateway/session_native_recovery_routes.py:verified_restored_relay_source
 * PoP: w24_sessionnat_5f7f78_retry_plan @ gateway/session_native_recovery_routes.py:_retry_recovery
 * PoP: w24_sessionnat_5f7f78_recovered_source @ gateway/session_native_recovery_routes.py:recovered_native_source
 * PoP: w24_sessionnat_5f7f78_constants @ gateway/session_native_recovery_routes.py:_RECOVERY_RETRY_DELAYS
 *
 * What is ported is the part of native-route recovery that is a pure function
 * of values: the envelope test that decides whether a settled Relay lane may be
 * rebuilt from a verified admission (route match + 'relay' provenance, never an
 * unverified stored profile), the verified-over-fallback choice of receiving
 * source, and the bounded retry ladder a late connection's recovery climbs
 * (5s, 30s, 120s, 300s), including the "gave up" end state.
 *
 * Seams, deliberately not ported: authority_persistence / latest_admission (a
 * store read), restore_native_source (a runner call that may raise
 * AuthorityRefusal), asyncio.create_task/…sleep and the runner's background-task
 * set.  Those become the arguments (payload, refused) and the returned plan.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_sessionnat_5f7f78.h"

/* _RECOVERY_RETRY_DELAYS */
static const double SR_RETRY_DELAYS[W24_SESSIONNAT_5F7F78_RETRY_DELAYS] = { 5, 30, 120, 300 };

static int sr_truthy(const json_t *v) {
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

/* Python '==': None == None, True == 1, 1 == 1.0; str never equals a number. */
static int sr_equal(const json_t *a, const json_t *b) {
    int anull = !a || a->type == JSON_NULL;
    int bnull = !b || b->type == JSON_NULL;
    double x, y;

    if (anull || bnull) return anull && bnull;
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL) &&
        (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* 'relay' in provenance: a key of a mapping, a member of a sequence, a
 * substring of a string.  Anything else is the live TypeError seam (null). */
static int sr_has_relay(const json_t *prov, int *error) {
    size_t i, n;

    if (!prov || prov->type == JSON_NULL) return 0;   /* the {} default */
    if (prov->type == JSON_OBJECT) return json_obj_get(prov, "relay") != NULL;
    if (prov->type == JSON_ARRAY) {
        n = json_len(prov);
        for (i = 0; i < n; i++) {
            const json_t *e = json_get(prov, i);
            if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, "relay") == 0) return 1;
        }
        return 0;
    }
    if (prov->type == JSON_STRING) return prov->str_val && strstr(prov->str_val, "relay") != NULL;
    *error = 1;
    return 0;
}

json_t *w24_sessionnat_5f7f78_relay_match(const json_t *envelope, const json_t *session_key) {
    const json_t *prov;
    int error = 0, relay;

    if (!envelope || envelope->type != JSON_OBJECT) return json_null();  /* .get() seam */
    relay = sr_has_relay(json_obj_get(envelope, "provenance"), &error);
    if (error) return json_null();
    if (!relay) return json_bool(0);
    return json_bool(sr_equal(json_obj_get(envelope, "route"), session_key));
}

json_t *w24_sessionnat_5f7f78_retry_plan(const json_t *running, const json_t *attempts) {
    json_t *out = json_object();
    json_t *delays = json_array();
    size_t n = 0, i, have = json_len(attempts);
    int is_running = running == NULL || running->type == JSON_NULL ? 1 : sr_truthy(running);

    /* for delay in _RECOVERY_RETRY_DELAYS: await sleep(delay);
     *     if not runner._running or await _try_recover(...): return */
    for (i = 0; i < W24_SESSIONNAT_5F7F78_RETRY_DELAYS; i++) {
        json_append(delays, json_number(SR_RETRY_DELAYS[i]));
        if (!is_running) break;                 /* checks _running after the sleep */
        n++;                                    /* one _try_recover call */
        if (i < have && sr_truthy(json_get(attempts, i))) break;
    }
    json_set(out, "attempts", json_number((double)n));
    json_set(out, "delays", delays);
    /* The ladder is exhausted exactly when every delay was slept and no
     * attempt recovered: logger.error("... gave up ..."). */
    json_set(out, "gave_up", json_bool(n == W24_SESSIONNAT_5F7F78_RETRY_DELAYS));
    return out;
}

json_t *w24_sessionnat_5f7f78_recovered_source(const json_t *payload, const json_t *refused,
                                               const json_t *fallback) {
    json_t *out;

    /* payload = _latest_native(...); if payload is not None: try restore... */
    if (payload && payload->type != JSON_NULL && !sr_truthy(refused)) {
        out = json_object();
        json_set(out, "from", json_string("verified"));
        json_set(out, "payload", json_copy(payload));
        return out;
    }
    /* except AuthorityRefusal: pass -> runner._restored_source(entry) */
    out = json_object();
    json_set(out, "from", json_string("fallback"));
    json_set(out, "tag", fallback && fallback->type != JSON_NULL ? json_copy(fallback) : json_null());
    return out;
}

json_t *w24_sessionnat_5f7f78_constants(void) {
    json_t *o = json_object();
    json_t *d = json_array();
    size_t i;

    for (i = 0; i < W24_SESSIONNAT_5F7F78_RETRY_DELAYS; i++)
        json_append(d, json_number(SR_RETRY_DELAYS[i]));
    json_set(o, "recovery_retry_delays", d);
    return o;
}
