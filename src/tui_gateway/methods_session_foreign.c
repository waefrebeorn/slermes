/*
 * methods_session_foreign.c - C11 port of upstream
 * tui_gateway/methods_session_foreign.py (gate w24_methodsses_ccfd92).
 *
 * PoP: w24_methodsses_ccfd92_dispatch_list @ tui_gateway/methods_session_foreign.py:_foreign_list
 * PoP: w24_methodsses_ccfd92_dispatch_history @ tui_gateway/methods_session_foreign.py:_foreign_history_request
 * PoP: w24_methodsses_ccfd92_methods @ tui_gateway/methods_session_foreign.py:<@method registrations>
 * PoP: w24_methodsses_ccfd92_constants @ tui_gateway/methods_session_foreign.py:<module constants>
 *
 * The handlers are three lines of decision around a reader, a profile store and
 * a persistence handle.  Those three are the seams and arrive as a behaviour
 * object here; what is ported is everything else the handler owns: the params
 * defaults (offset 0, limit 25), the JSON-RPC envelope, the mapping of
 * ValueError -> -32602 (with str(exc)) and OSError -> -32000 (with the module's
 * fixed text), and the no-store refusal.  Nothing opens a db, a folder or a
 * socket.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_methodsses_ccfd92.h"

#define CODE_INVALID_PARAMS (-32602)
#define CODE_INTERNAL (-32000)

static const char *LIST_OS_ERROR = "Could not read session folders on this backend";
static const char *HISTORY_OS_ERROR = "Could not read this session on the backend";
static const char *DB_UNAVAILABLE = "Session storage is unavailable";

/* _ok(rid, result) */
static json_t *ok_env(const json_t *rid, json_t *result) {
    json_t *o = json_object();
    json_set(o, "jsonrpc", json_string("2.0"));
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "result", result ? result : json_null());
    return o;
}

/* _err(rid, code, msg) */
static json_t *err_env(const json_t *rid, double code, const char *msg) {
    json_t *o = json_object();
    json_t *e = json_object();
    json_set(e, "code", json_number(code));
    json_set(e, "message", json_string(msg ? msg : ""));
    json_set(o, "jsonrpc", json_string("2.0"));
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "error", e);
    return o;
}

static json_t *wrap(json_t *call, json_t *resp) {
    json_t *o = json_object();
    json_set(o, "call", call);
    json_set(o, "response", resp);
    return o;
}

json_t *w24_methodsses_ccfd92_dispatch_list(const json_t *req) {
    const json_t *params = json_obj_get(req, "params");
    const json_t *beh = json_obj_get(req, "behavior");
    const json_t *rid = json_obj_get(req, "rid");
    const json_t *src = json_obj_get(params, "source");
    const json_t *off = json_obj_get(params, "offset");
    const json_t *lim = json_obj_get(params, "limit");
    const char *raise_kind = json_get_str(beh, "raise", "");
    json_t *call = json_array();
    json_t *resp;

    /* list_foreign_sessions(params.get("source"), params.get("offset", 0),
     *                        params.get("limit", 25)) */
    json_append(call, src ? json_copy(src) : json_null());
    json_append(call, off ? json_copy(off) : json_number(0));
    json_append(call, lim ? json_copy(lim) : json_number(25));

    if (!strcmp(raise_kind, "ValueError")) {
        resp = err_env(rid, CODE_INVALID_PARAMS, json_get_str(beh, "message", ""));
    } else if (!strcmp(raise_kind, "OSError")) {
        resp = err_env(rid, CODE_INTERNAL, LIST_OS_ERROR);
    } else {
        const json_t *result = json_obj_get(beh, "result");
        resp = ok_env(rid, result ? json_copy(result) : json_null());
    }
    return wrap(call, resp);
}

json_t *w24_methodsses_ccfd92_dispatch_history(const json_t *req) {
    const json_t *params = json_obj_get(req, "params");
    const json_t *beh = json_obj_get(req, "behavior");
    const json_t *rid = json_obj_get(req, "rid");
    const json_t *id_v = json_obj_get(params, "id");
    const char *profile_name = json_get_str(beh, "profile_name", "");
    const char *raise_kind = json_get_str(beh, "raise", "");
    int importing = json_get_bool(req, "importing", 0);
    int db_available = json_get_bool(beh, "db", 1);
    json_t *call = NULL;
    json_t *resp;

    /* The two context managers are seams; db is None when the store is unavailable. */
    if (!db_available) {
        resp = err_env(rid, CODE_INTERNAL, DB_UNAVAILABLE);
        return wrap(json_null(), resp);
    }
    call = json_array();
    json_append(call, json_string(importing ? "import" : "preview"));
    json_append(call, id_v ? json_copy(id_v) : json_null());
    /* preview_foreign_session(id, db, persistence=...) takes no profile name. */
    json_append(call, importing ? json_string(profile_name) : json_null());

    if (!strcmp(raise_kind, "ValueError")) {
        resp = err_env(rid, CODE_INVALID_PARAMS, json_get_str(beh, "message", ""));
    } else if (!strcmp(raise_kind, "OSError")) {
        resp = err_env(rid, CODE_INTERNAL, HISTORY_OS_ERROR);
    } else {
        const json_t *result = json_obj_get(beh, "result");
        resp = ok_env(rid, result ? json_copy(result) : json_null());
    }
    return wrap(call, resp);
}

json_t *w24_methodsses_ccfd92_methods(void) {
    json_t *o = json_array();
    json_append(o, json_string("session.foreign.import"));
    json_append(o, json_string("session.foreign.list"));
    json_append(o, json_string("session.foreign.preview"));
    return o;
}

json_t *w24_methodsses_ccfd92_constants(void) {
    json_t *o = json_object();
    json_set(o, "list_method", json_string("session.foreign.list"));
    json_set(o, "preview_method", json_string("session.foreign.preview"));
    json_set(o, "import_method", json_string("session.foreign.import"));
    json_set(o, "invalid_params_code", json_number(CODE_INVALID_PARAMS));
    json_set(o, "internal_code", json_number(CODE_INTERNAL));
    json_set(o, "list_os_error", json_string(LIST_OS_ERROR));
    json_set(o, "history_os_error", json_string(HISTORY_OS_ERROR));
    json_set(o, "db_unavailable", json_string(DB_UNAVAILABLE));
    return o;
}
