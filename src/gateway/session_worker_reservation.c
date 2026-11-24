/*
 * session_worker_reservation.c - C11 port of upstream
 * gateway/session_worker_reservation.py (gate w22_sessionwor_277372).
 *
 * PoP: w22_sessionwor_277372_verify @ gateway/session_worker_reservation.py:verify_worker_identity
 * PoP: w22_sessionwor_277372_owner_subject @ gateway/session_worker_reservation.py:owner_worker_subject
 * PoP: w22_sessionwor_277372_reserve @ gateway/session_worker_reservation.py:reserve_admission_worker
 * PoP: w22_sessionwor_277372_lose @ gateway/session_worker_reservation.py:lose_admission_worker
 * PoP: w22_sessionwor_277372_hops @ gateway/session_worker_reservation.py:MAX_LAUNCHER_HOPS
 *
 * The process table is a seam: the caller supplies the facts psutil would have
 * returned (ppid, status, running, ancestors, birth) and this file owns only the
 * decision ladder, its refusal order and the exact refusal tokens.
 */
#include <stdlib.h>
#include <string.h>
#include "noc_w22_sessionwor_277372.h"

int w22_sessionwor_277372_hops(void) { return W22_SESSIONWOR_277372_MAX_HOPS; }

/* Python's type(x) is int: a JSON number with no fractional part. */
static int sw_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    return (double)(long long)d == d;
}

static int sw_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static json_t *sw_refused(const char *reason) {
    json_t *o = json_object();
    json_set(o, "status", json_string("refused"));
    json_set(o, "reason", json_string(reason));
    return o;
}

static json_t *sw_verified(const json_t *pid, const json_t *birth) {
    json_t *o = json_object();
    json_set(o, "status", json_string("verified"));
    json_set(o, "pid", json_number(pid ? pid->num_val : 0.0));
    if (birth) json_set(o, "birth", json_number(birth->num_val));
    else json_set(o, "birth", json_null());
    return o;
}

json_t *w22_sessionwor_277372_verify(const json_t *world) {
    const json_t *hello, *h_type, *h_pid, *h_birth, *h_anc, *handle_ppid;
    const json_t *worker_birth, *parents, *process_pid;
    int handle_zombie, worker_zombie, worker_running, psutil_error, process_live;
    double owner_pid;

    if (!world || world->type != JSON_OBJECT) return sw_refused("invalid_worker_frame");

    /* 1. the Popen handle must be live. */
    process_live = json_get_bool(world, "process_live", 1) ? 1 : 0;
    if (!process_live) return sw_refused("worker_not_live");

    /* 2. the hello frame shape. */
    hello = json_obj_get(world, "hello");
    h_type = hello ? json_obj_get(hello, "type") : NULL;
    h_pid = hello ? json_obj_get(hello, "pid") : NULL;
    h_birth = hello ? json_obj_get(hello, "birth") : NULL;
    h_anc = hello ? json_obj_get(hello, "ancestors") : NULL;
    if (!hello || hello->type != JSON_OBJECT) return sw_refused("invalid_worker_frame");
    if (!h_type || h_type->type != JSON_STRING || strcmp(h_type->str_val ? h_type->str_val : "", "hello") != 0)
        return sw_refused("invalid_worker_frame");
    if (!sw_is_int(h_pid)) return sw_refused("invalid_worker_frame");
    if (!sw_is_num(h_birth)) return sw_refused("invalid_worker_frame");
    if (!h_anc || h_anc->type != JSON_ARRAY) return sw_refused("invalid_worker_frame");
    {
        size_t i, n = json_len(h_anc);
        for (i = 0; i < n; i++)
            if (!sw_is_int(json_get(h_anc, i))) return sw_refused("invalid_worker_frame");
    }

    /* 3. the worker is never the owner and never a non-pid. */
    owner_pid = json_get_num(world, "owner_pid", -1.0);
    if (h_pid->num_val <= 0.0) return sw_refused("permission_denied");
    if (owner_pid >= 0.0 && h_pid->num_val == owner_pid) return sw_refused("permission_denied");

    /* 4. the process-table proof (psutil.Error becomes worker_not_live). */
    psutil_error = json_get_bool(world, "psutil_error", 0) ? 1 : 0;
    process_pid = json_obj_get(world, "process_pid");
    if (psutil_error) return sw_refused("worker_not_live");
    if (!sw_is_num(process_pid)) return sw_refused("worker_not_live");

    handle_ppid = json_obj_get(world, "handle_ppid");
    if (!sw_is_num(handle_ppid) || !(handle_ppid->num_val == owner_pid))
        return sw_refused("permission_denied");
    handle_zombie = json_get_bool(world, "handle_zombie", 0) ? 1 : 0;
    if (handle_zombie) return sw_refused("permission_denied");

    worker_birth = json_obj_get(world, "worker_birth");
    if (!sw_is_num(worker_birth)) return sw_refused("permission_denied");
    if (!(worker_birth->num_val == h_birth->num_val)) return sw_refused("permission_denied");
    worker_running = json_get_bool(world, "worker_running", 1) ? 1 : 0;
    if (!worker_running) return sw_refused("permission_denied");
    worker_zombie = json_get_bool(world, "worker_zombie", 0) ? 1 : 0;
    if (worker_zombie) return sw_refused("permission_denied");

    /* 5. the handle must be the worker itself or one of its launcher hops. */
    parents = json_obj_get(world, "parents");
    if (h_pid->num_val != process_pid->num_val) {
        size_t i, n = (parents && parents->type == JSON_ARRAY) ? json_len(parents) : 0;
        int found = 0;
        if (n > (size_t)W22_SESSIONWOR_277372_MAX_HOPS) n = (size_t)W22_SESSIONWOR_277372_MAX_HOPS;
        for (i = 0; i < n; i++) {
            json_t *e = json_get(parents, i);
            if (sw_is_int(e) && e->num_val == process_pid->num_val) { found = 1; break; }
        }
        if (!found) return sw_refused("permission_denied");
    }
    return sw_verified(h_pid, worker_birth);
}

char *w22_sessionwor_277372_owner_subject(const char *os_name, const char *uid,
                                          const char *sid) {
    const char *tail = NULL;
    const char *prefix;
    size_t pl, tl;
    char *out;
    if (os_name && strcmp(os_name, "nt") == 0) {
        prefix = "sid:";
        tail = sid;
    } else {
        prefix = "uid:";
        tail = uid;
    }
    if (!tail) tail = "None";
    pl = strlen(prefix);
    tl = strlen(tail);
    out = (char *)malloc(pl + tl + 1);
    if (!out) return NULL;
    memcpy(out, prefix, pl);
    memcpy(out + pl, tail, tl + 1);
    return out;
}

json_t *w22_sessionwor_277372_reserve(const json_t *req) {
    static const char *keys[] = {"epoch", "admission_id", "principal_id", "profile_id",
                                 "pid", "birth", "secret", "owner", "worker_subject"};
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        json_t *v = req && req->type == JSON_OBJECT ? json_obj_get(req, keys[i]) : NULL;
        json_set(out, keys[i], v ? json_copy(v) : json_null());
    }
    return out;
}

json_t *w22_sessionwor_277372_lose(const json_t *req) {
    json_t *row = req && req->type == JSON_OBJECT ? json_obj_get(req, "row") : NULL;
    json_t *scope = req && req->type == JSON_OBJECT ? json_obj_get(req, "scope") : NULL;
    json_t *out = json_object();
    json_t *v;
    v = scope ? json_obj_get(scope, "epoch") : NULL;
    json_set(out, "epoch", v ? json_copy(v) : json_null());
    v = row ? json_obj_get(row, "admission_id") : NULL;
    json_set(out, "admission_id", v ? json_copy(v) : json_null());
    v = scope ? json_obj_get(scope, "execution_id") : NULL;
    json_set(out, "execution_id", v ? json_copy(v) : json_null());
    v = scope ? json_obj_get(scope, "generation") : NULL;
    json_set(out, "generation", v ? json_copy(v) : json_null());
    return out;
}
