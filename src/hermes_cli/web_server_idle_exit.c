/*
 * web_server_idle_exit.c - C11 port of upstream
 * hermes_cli/web_server_idle_exit.py (gate w26_webserveri_efe232).
 *
 * Ported value->value: the liveness decision of an SSH-isolated backend.
 * Proven by the differential oracle gate w26_webserveri_efe232.
 * PoP: w26_webserveri_efe232_tracker @ hermes_cli/web_server_idle_exit.py:IdleClientTracker
 * PoP: w26_webserveri_efe232_session_work_in_flight @ hermes_cli/web_server_idle_exit.py:_session_work_in_flight
 * PoP: w26_webserveri_efe232_should_exit_idle @ hermes_cli/web_server_idle_exit.py:should_exit_idle
 * PoP: w26_webserveri_efe232_turn_in_flight_view @ hermes_cli/web_server_idle_exit.py:turn_in_flight
 * PoP: w26_webserveri_efe232_constants @ hermes_cli/web_server_idle_exit.py:<literals>
 *
 * Seams: time.monotonic, the threading.Lock, the gateway session table, the
 * retirement/delegation/cron ledgers.  What is ported is what those values
 * decide: has the last client been gone long enough, and is a turn provably
 * running (an unreadable probe keeps the process, fail closed).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_webserveri_efe232.h"

/* ---------------------------------------------------------------- helpers */

static int ws_truthy(const json_t *v) {
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

static double ws_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

static long long ws_int(const json_t *v, long long def) {
    double d = ws_num(v, (double)def);
    return (long long)d;
}

/* ---------------------------------------------------------------- entries */

json_t *w26_webserveri_efe232_tracker(const json_t *in) {
    const json_t *ops = json_obj_get(in, "ops");
    double last = ws_num(json_obj_get(in, "start"), 0.0);
    double now = ws_num(json_obj_get(in, "now"), last);
    long long live = 0;
    size_t i, n = (ops && ops->type == JSON_ARRAY) ? json_len(ops) : 0;
    json_t *out = json_object();

    for (i = 0; i < n; i++) {
        const json_t *op = json_get(ops, i);
        const char *name;
        if (!op || op->type != JSON_OBJECT) continue;
        name = json_get_str(op, "op", "");
        if (!strcmp(name, "open")) {
            live += 1;
            last = ws_num(json_obj_get(op, "now"), last);
        } else if (!strcmp(name, "close")) {
            live = live > 0 ? live - 1 : 0;          /* max(0, self._live - 1) */
            last = ws_num(json_obj_get(op, "now"), last);
        }
    }
    json_set(out, "live", json_number((double)live));
    json_set(out, "idle_for", json_number(live ? 0.0 : now - last));
    return out;
}

json_t *w26_webserveri_efe232_session_work_in_flight(const json_t *in) {
    static const char *const KEYS[] = {
        "running", "queued_prompt", "queued_prompts", "_auto_continue_scheduled",
    };
    static const char *const THREADS[] = { "_run_thread", "_agent_build_thread" };
    const json_t *session = json_obj_get(in, "session");
    size_t i;

    if (!session || session->type != JSON_OBJECT) return json_bool(0);
    for (i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++)
        if (ws_truthy(json_obj_get(session, KEYS[i]))) return json_bool(1);
    for (i = 0; i < sizeof(THREADS) / sizeof(THREADS[0]); i++) {
        const json_t *thread = json_obj_get(session, THREADS[i]);
        if (!thread || thread->type == JSON_NULL) continue;   /* `is not None` */
        if (thread->type == JSON_OBJECT && ws_truthy(json_obj_get(thread, "alive")))
            return json_bool(1);
    }
    return json_bool(0);
}

json_t *w26_webserveri_efe232_should_exit_idle(const json_t *in) {
    long long live = ws_int(json_obj_get(in, "live"), 0);
    double last = ws_num(json_obj_get(in, "last_client_at"), 0.0);
    double now = ws_num(json_obj_get(in, "now"), last);
    double grace = ws_num(json_obj_get(in, "grace_s"), W26_WEBSERVERI_EFE232_IDLE_GRACE_S);
    const char *probe = json_get_str(in, "probe", "false");
    double idle = live ? 0.0 : now - last;

    /* `probe() is False` -- True and None both keep the process. */
    return json_bool(idle >= grace && probe && !strcmp(probe, "false"));
}

json_t *w26_webserveri_efe232_turn_in_flight_view(const json_t *in) {
    const json_t *ledger = json_obj_get(in, "ledger");
    if (!ledger || ledger->type == JSON_NULL) return json_null();   /* probe unreadable */
    return json_bool(ws_truthy(ledger));
}

json_t *w26_webserveri_efe232_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_idle_grace_s", json_number(W26_WEBSERVERI_EFE232_IDLE_GRACE_S));
    json_set(o, "tunnel_ws_ping_interval_s", json_number(W26_WEBSERVERI_EFE232_PING_INTERVAL_S));
    json_set(o, "tunnel_ws_ping_timeout_s", json_number(W26_WEBSERVERI_EFE232_PING_TIMEOUT_S));
    return o;
}
