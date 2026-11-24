/*
 * runtime_bootstrap_windows.c - C11 port of upstream
 * gateway/runtime_bootstrap_windows.py (gate w34_runtimeboo_7f06de).
 *
 * PoP: w34_runtimeboo_7f06de_classify_refused_owner @ gateway/runtime_bootstrap_windows.py:classify_refused_owner
 * PoP: w34_runtimeboo_7f06de_refused_owner_remedy @ gateway/runtime_bootstrap_windows.py:refused_owner_remedy
 * PoP: w34_runtimeboo_7f06de_judge_pipe_server @ gateway/runtime_bootstrap_windows.py:judge_pipe_server
 * PoP: w34_runtimeboo_7f06de_peer_subject @ gateway/runtime_bootstrap_windows.py:_peer_subject (serving half)
 * PoP: w34_runtimeboo_7f06de_remaining_ms @ gateway/runtime_bootstrap_windows.py:_remaining_ms
 * PoP: w34_runtimeboo_7f06de_accept_outcome @ gateway/runtime_bootstrap_windows.py:_accept (result tail)
 * PoP: w34_runtimeboo_7f06de_pipe_open_action @ gateway/runtime_bootstrap_windows.py:query_runtime_control (open/wait error table)
 * PoP: w34_runtimeboo_7f06de_read_step @ gateway/runtime_bootstrap_windows.py:_read_line (chunk size + size limit)
 * PoP: w34_runtimeboo_7f06de_pipe_dacl @ gateway/runtime_bootstrap_windows.py:NativeControlServer._create_pipe (DACL string)
 * PoP: w34_runtimeboo_7f06de_pipe_create_mode @ gateway/runtime_bootstrap_windows.py:NativeControlServer._create_pipe (open mode)
 * PoP: w34_runtimeboo_7f06de_constants @ gateway/runtime_bootstrap_windows.py:_PIPE_CLIENT_SQOS,_PIPE_INSTANCES
 *
 * Seams (deliberately not ported): every ctypes/Win32 call, the pipe handles
 * and their OVERLAPPED objects, time.monotonic(), the worker threads, the
 * handler, and the disconnect/close cleanup.  Where Python mixes probes with
 * decisions the port starts at the decision boundary: the caller performs the
 * probe (OpenProcess, GetSecurityInfo, GetNamedPipeClientProcessId,
 * WaitForSingleObject, CreateFile, ...) and hands the plain values in here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_runtimeboo_7f06de.h"

#define ADMIN_SID "S-1-5-32-544"
#define REFUSED_OTHER_SESSION "gateway_other_session"
#define REFUSED_ELEVATED "gateway_elevated"

/* Borrowed value for key; NULL when the key is absent. */
static const json_t *rb(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

/* Present and not JSON null (Python's "not None"). */
static const json_t *rn(const json_t *obj, const char *key) {
    const json_t *v = rb(obj, key);
    return v && v->type != JSON_NULL ? v : NULL;
}

/* Python truthiness of a JSON value. */
static int rbool(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* Python int-or-None: 1 when the key holds a number. */
static int rnum_opt(const json_t *obj, const char *key, double *out) {
    const json_t *v = rb(obj, key);
    if (!v || v->type != JSON_NUMBER) return 0;
    *out = v->num_val;
    return 1;
}

static const char *rstr(const json_t *obj, const char *key) {
    const json_t *v = rb(obj, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val ? v->str_val : "";
}

/* f"{value}" for the integer pids and sessions that reach a string. */
static void rnum_text(double v, char *buf, size_t n) {
    if (v == (double)(long long)v && v > -9.0e18 && v < 9.0e18)
        snprintf(buf, n, "%lld", (long long)v);
    else
        snprintf(buf, n, "%.17g", v);
}

/* -- classify_refused_owner ------------------------------------------------ */

/* why: 0 = None, 1 = gateway_other_session, 2 = gateway_elevated.
 * owner_elevated: 0 = False, 1 = True, -1 = None (probe failed). */
static void classify_vals(int has_own, double own_session, int has_owner, double owner_session,
                          int own_elevated, int owner_elevated, int *why) {
    *why = 0;
    if (has_own && has_owner && own_session != owner_session) {
        *why = 1;
        return;
    }
    if (own_elevated) return;
    if (owner_elevated == 1 || (owner_elevated == -1 && has_owner)) *why = 2;
}

static int classify(const json_t *req, int *why) {
    double own_session = 0.0, owner_session = 0.0;
    int has_own = rnum_opt(req, "own_session", &own_session);
    int has_owner = rnum_opt(req, "owner_session", &owner_session);
    const json_t *owner_elevated = rn(req, "owner_elevated");
    int own_elevated = rbool(rn(req, "own_elevated"));
    classify_vals(has_own, own_session, has_owner, owner_session, own_elevated,
                  owner_elevated ? (rbool(owner_elevated) ? 1 : 0) : -1, why);
    return *why;
}

json_t *w34_runtimeboo_7f06de_classify_refused_owner(const json_t *req) {
    int why = 0;
    classify(req, &why);
    if (why == 1) return json_string(REFUSED_OTHER_SESSION);
    if (why == 2) return json_string(REFUSED_ELEVATED);
    return json_null();
}

/* -- refused_owner_remedy -------------------------------------------------- */

json_t *w34_runtimeboo_7f06de_refused_owner_remedy(const json_t *req) {
    const char *reason = rstr(req, "reason");
    double pid = 0.0, owner_session = 0.0;
    char pid_text[64], where[96], *buf;
    size_t need;
    int has_session;

    rnum_opt(req, "pid", &pid);
    rnum_text(pid, pid_text, sizeof pid_text);
    has_session = rnum_opt(req, "owner_session", &owner_session);

    if (reason && !strcmp(reason, REFUSED_ELEVATED)) {
        static const char *const fmt =
            "The running gateway (PID %s) was started as administrator, so this non-elevated "
            "app cannot reach it. Stop it from an administrator prompt (hermes gateway stop), then "
            "start Hermes without elevation";
        need = strlen(fmt) + strlen(pid_text) + 1;
        buf = (char *)malloc(need);
        if (!buf) return json_null();
        snprintf(buf, need, fmt, pid_text);
    } else {
        static const char *const fmt =
            "The running gateway (PID %s) runs in another Windows logon session (%s, e.g. a "
            "boot-time scheduled task), so this app cannot reach it. Stop it (hermes gateway stop from an "
            "administrator prompt) and let only one auto-start task launch the gateway";
        if (has_session) {
            char sess[64];
            rnum_text(owner_session, sess, sizeof sess);
            snprintf(where, sizeof where, "session %s", sess);
        } else {
            snprintf(where, sizeof where, "another session");
        }
        need = strlen(fmt) + strlen(pid_text) + strlen(where) + 1;
        buf = (char *)malloc(need);
        if (!buf) return json_null();
        snprintf(buf, need, fmt, pid_text, where);
    }
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

/* -- judge_pipe_server ----------------------------------------------------- */

/* pipe DACL == ((0, own_sid),) exactly: one ACCESS_ALLOWED ACE naming this user. */
static int dacl_is_private(const json_t *dacl, const char *own_sid) {
    const json_t *only, *kind, *sid;
    if (!dacl || dacl->type == JSON_NULL) return 0;   /* Python: empty tuple */
    if (dacl->type != JSON_ARRAY || json_len(dacl) != 1) return 0;
    only = json_get(dacl, 0);
    if (!only || only->type != JSON_ARRAY || json_len(only) != 2) return 0;
    kind = json_get(only, 0);
    sid = json_get(only, 1);
    if (!kind || kind->type != JSON_NUMBER || kind->num_val != 0.0) return 0;
    if (!sid || sid->type != JSON_STRING) return 0;
    return strcmp(sid->str_val ? sid->str_val : "", own_sid) == 0;
}

static json_t *judge_out(int ok, const char *subject, const char *error,
                         const char *reason, double pid) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok != 0));
    json_set(o, "subject", subject ? json_string(subject) : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    json_set(o, "reason", reason ? json_string(reason) : json_null());
    json_set(o, "pid", json_number(pid));
    return o;
}

json_t *w34_runtimeboo_7f06de_judge_pipe_server(const json_t *req) {
    const char *own_sid = rstr(req, "own_sid");
    const char *process_sid = rstr(req, "process_sid");
    const json_t *pipe = rn(req, "pipe");
    const char *owner = NULL;
    const json_t *dacl = NULL;
    const json_t *elevated_node = rn(req, "server_elevated");
    int elevated_known = 0, server_elevated = 0;
    int own_elevated = rbool(rn(req, "own_elevated"));
    int owner_elevated = 0;
    double pid = 0.0;
    const char *reason = NULL;
    char *subject = NULL;
    int why = 0;

    rnum_opt(req, "pid", &pid);
    if (!own_sid) own_sid = "";
    if (pipe) {
        owner = rstr(pipe, "owner");
        dacl = rb(pipe, "dacl");
    }
    if (elevated_node) {
        elevated_known = 1;
        server_elevated = rbool(elevated_node);
    }

    if (process_sid) {
        if (strcmp(process_sid, own_sid) != 0)
            return judge_out(0, NULL, "named-pipe peer belongs to another user", NULL, pid);
        if (!elevated_known) server_elevated = owner && !strcmp(owner, ADMIN_SID);
        if (own_elevated || !server_elevated) {
            size_t n = strlen(own_sid) + 5;
            json_t *out;
            subject = (char *)malloc(n);
            if (!subject) return json_null();
            snprintf(subject, n, "sid:%s", own_sid);
            out = judge_out(1, subject, NULL, NULL, pid);
            free(subject);
            return out;
        }
        owner_elevated = 1;
    } else {
        int owner_is_own, owner_is_admin;
        if (!pipe)
            return judge_out(0, NULL, "cannot verify bootstrap peer process", NULL, pid);
        owner_is_own = owner && !strcmp(owner, own_sid);
        owner_is_admin = owner && !strcmp(owner, ADMIN_SID);
        /* The ACE shape only filters pipes that are not the gateway's own. */
        if ((!owner_is_own && !owner_is_admin) || !dacl_is_private(dacl, own_sid))
            return judge_out(0, NULL, "named-pipe peer belongs to another user", NULL, pid);
        owner_elevated = owner_is_admin ? 1 : -1;
    }

    {
        double own_session = 0.0, server_session = 0.0;
        int has_own = rnum_opt(req, "own_session", &own_session);
        int has_server = rnum_opt(req, "server_session", &server_session);
        classify_vals(has_own, own_session, has_server, server_session, own_elevated,
                      owner_elevated, &why);
    }
    if (why == 0)
        return judge_out(0, NULL, "cannot verify bootstrap peer process", NULL, pid);

    {
        json_t *remedy_req = json_object();
        json_t *remedy;
        const char *msg;
        json_t *out;
        reason = why == 1 ? REFUSED_OTHER_SESSION : REFUSED_ELEVATED;
        json_set(remedy_req, "reason", json_string(reason));
        json_set(remedy_req, "pid", json_number(pid));
        if (rn(req, "server_session"))
            json_set(remedy_req, "owner_session", json_copy(rn(req, "server_session")));
        else
            json_set(remedy_req, "owner_session", json_null());
        remedy = w34_runtimeboo_7f06de_refused_owner_remedy(remedy_req);
        msg = remedy && remedy->type == JSON_STRING ? remedy->str_val : "";
        out = judge_out(0, NULL, msg, reason, pid);
        json_free(remedy);
        json_free(remedy_req);
        return out;
    }
}

/* -- peer_subject (serving half) ------------------------------------------- */

static json_t *peer_out(int ok, const char *subject, const char *error) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok != 0));
    json_set(o, "subject", subject ? json_string(subject) : json_null());
    json_set(o, "error", error ? json_string(error) : json_null());
    return o;
}

json_t *w34_runtimeboo_7f06de_peer_subject(const json_t *req) {
    const char *client_sid = rstr(req, "client_sid");
    const char *pid_sid = rstr(req, "pid_sid");
    const char *own_sid = rstr(req, "own_sid");
    int client_pid = rbool(rn(req, "client_pid"));
    const char *sid = client_sid;

    if (!own_sid) own_sid = "";
    if (!sid) {
        if (!client_pid) return peer_out(0, NULL, "cannot identify named-pipe peer");
        if (!pid_sid) return peer_out(0, NULL, "cannot verify bootstrap peer process");
        sid = pid_sid;
    }
    if (strcmp(sid, own_sid) != 0)
        return peer_out(0, NULL, "named-pipe peer belongs to another user");
    {
        size_t n = strlen(sid) + 5;
        char *subject = (char *)malloc(n);
        json_t *out;
        if (!subject) return json_null();
        snprintf(subject, n, "sid:%s", sid);
        out = peer_out(1, subject, NULL);
        free(subject);
        return out;
    }
}

/* -- _remaining_ms --------------------------------------------------------- */

json_t *w34_runtimeboo_7f06de_remaining_ms(const json_t *req) {
    double remaining = 0.0;
    long long ms;
    json_t *out = json_object();

    rnum_opt(req, "remaining", &remaining);
    if (remaining <= 0.0) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "ms", json_null());
        json_set(out, "error", json_string("runtime control deadline exceeded"));
        return out;
    }
    ms = (long long)(remaining * 1000.0);   /* Python int(): truncation */
    if (ms < 1) ms = 1;
    json_set(out, "ok", json_bool(true));
    json_set(out, "ms", json_number((double)ms));
    json_set(out, "error", json_null());
    return out;
}

/* -- _accept decision tail ------------------------------------------------- */

json_t *w34_runtimeboo_7f06de_accept_outcome(const json_t *req) {
    double error = 0.0;
    json_t *out = json_object();

    rnum_opt(req, "error", &error);
    if (error == (double)W34_RUNTIMEBOO_7F06DE_ERROR_OPERATION_ABORTED) {
        json_set(out, "connected", json_bool(false));
        json_set(out, "error", json_null());
        json_set(out, "code", json_number(error));
        return out;
    }
    if (error != 0.0) {
        json_set(out, "connected", json_bool(false));
        json_set(out, "error", json_string("runtime pipe accept failed"));
        json_set(out, "code", json_number(error));
        return out;
    }
    json_set(out, "connected", json_bool(true));
    json_set(out, "error", json_null());
    json_set(out, "code", json_null());
    return out;
}

/* -- query_runtime_control open/wait error table --------------------------- */

json_t *w34_runtimeboo_7f06de_pipe_open_action(const json_t *req) {
    const char *phase = rstr(req, "phase");
    double code = 0.0;
    json_t *out = json_object();

    rnum_opt(req, "code", &code);
    if (phase && !strcmp(phase, "wait")) {
        if (code == (double)W34_RUNTIMEBOO_7F06DE_ERROR_SEM_TIMEOUT) {
            json_set(out, "action", json_string("fail"));
            json_set(out, "error", json_string("runtime control deadline exceeded"));
        } else {
            json_set(out, "action", json_string("raise"));
            json_set(out, "error", json_null());
        }
        return out;
    }
    if (code == 2.0) {
        json_set(out, "action", json_string("fail"));
        json_set(out, "error", json_string("runtime control pipe absent"));
    } else if (code == 5.0) {
        json_set(out, "action", json_string("fail"));
        json_set(out, "error", json_string("runtime control pipe inaccessible"));
    } else if (code == (double)W34_RUNTIMEBOO_7F06DE_ERROR_PIPE_BUSY) {
        json_set(out, "action", json_string("wait"));
        json_set(out, "error", json_null());
    } else {
        json_set(out, "action", json_string("raise"));
        json_set(out, "error", json_null());
    }
    return out;
}

/* -- _read_line body ------------------------------------------------------- */

json_t *w34_runtimeboo_7f06de_read_step(const json_t *req) {
    double maximum = 0.0, before = 0.0, count = 0.0;
    long long have, room, chunk;
    json_t *out = json_object();

    rnum_opt(req, "maximum", &maximum);
    rnum_opt(req, "before", &before);
    rnum_opt(req, "count", &count);
    json_set(out, "error", json_null());
    json_set(out, "chunk", json_null());
    if (count == 0.0) {
        /* No bytes: the peer closed the pipe before sending a response. */
        json_set(out, "error", json_string("runtime pipe closed before response"));
        return out;
    }
    have = (long long)before + (long long)count;
    if (have > (long long)maximum) {
        json_set(out, "error", json_string("runtime control message too large"));
        return out;
    }
    room = (long long)maximum + 1 - have;
    chunk = room < (long long)W34_RUNTIMEBOO_7F06DE_READ_CHUNK ? room
                                                              : (long long)W34_RUNTIMEBOO_7F06DE_READ_CHUNK;
    json_set(out, "chunk", json_number((double)chunk));
    return out;
}

/* -- _create_pipe DACL ----------------------------------------------------- */

json_t *w34_runtimeboo_7f06de_pipe_dacl(const json_t *req) {
    const char *sid = rstr(req, "sid");
    char *buf;
    size_t need;
    json_t *out;

    if (!sid) sid = "";
    need = strlen("D:P(A;;GA;;;)") + strlen(sid) + 1;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need, "D:P(A;;GA;;;%s)", sid);
    out = json_string(buf);
    free(buf);
    return out;
}

/* -- _create_pipe open mode ------------------------------------------------ */

json_t *w34_runtimeboo_7f06de_pipe_create_mode(const json_t *req) {
    unsigned mode = W34_RUNTIMEBOO_7F06DE_PIPE_ACCESS_DUPLEX |
                    W34_RUNTIMEBOO_7F06DE_FILE_FLAG_OVERLAPPED;
    if (rbool(rn(req, "first"))) mode |= W34_RUNTIMEBOO_7F06DE_FIRST_PIPE_INSTANCE;
    return json_number((double)mode);
}

/* -- module constants ------------------------------------------------------ */

json_t *w34_runtimeboo_7f06de_constants(void) {
    const double sqos = 0x00100000 | 0x00010000;       /* _PIPE_CLIENT_SQOS */
    const double instances = 8;                        /* _PIPE_INSTANCES */
    json_t *o = json_object();
    json_set(o, "sqos", json_number(sqos));
    json_set(o, "pipe_instances", json_number(instances));
    json_set(o, "client_open_flags",
             json_number((double)(W34_RUNTIMEBOO_7F06DE_FILE_FLAG_OVERLAPPED | 0x00110000u)));
    return o;
}
