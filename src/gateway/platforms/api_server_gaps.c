#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── gateway/platforms/api_server.py: 16 missing functions ── */

/* ── rest_revive4 revival: api_server.py delegators + room-grant handlers ──
 * Faithful ports of the listed PoP entries. The fixed void signatures take
 * their Python arguments from the module-static call-context slot g_asg_ctx
 * (a borrowed JSON object installed per call by the test harness) and reach
 * every live collaborator through the g_asg_io seam channel.
 *
 * Return contract: the char* result is the malloc'd JSON serialization of the
 * Python return value ("null" for None). NULL is returned ONLY with a raise
 * spec parked in g_asg_pending_raise (propagated exception). With no channel
 * installed the ports fail closed (RuntimeError parked).
 *
 * Seam names (args objects; results are NEW nodes owned by the caller):
 *   ensure_store            self._ensure_store()                     {}
 *   store_beside            self._store_beside(db)                   {db}
 *   import_declared_api_session / declared_api_session
 *                           gateway.session_api.declared_api_session(db, key, persistence=...)
 *   latest_session_for_peer store.routing.latest_session_for_peer(source=..., session_key=...)
 *   api_runs.run_idempotency_scope   _api_runs._run_idempotency_scope(self, request)
 *                           (the constant _api_server=sys.modules[__name__] kwarg is
 *                            dropped from the recorded args on BOTH sides)
 *   api_runs.durable_run_status      _api_runs._durable_run_status(self, request, run_id)
 *   room_dispatch.normalize_room_dispatch
 *                           _room_dispatch._normalize_room_dispatch(self, request, body)
 *                            (_api_server kwarg dropped on both sides; async modeled sync)
 *   room_grant_token        self._room_grant_token(request)          {request}
 *   room_grant_secret       self._room_grant_secret()                {}  (bytes modeled as str)
 *   import_gateway_room_grant_secret / gateway_room_grant_secret
 *   import_decode_room_grant / decode_room_grant(secret, token, permission=...)
 *   import_hosted_rooms     `from gateway import hosted_rooms`       {}
 *   import_persistence_assembly  `from persistence_assembly import open_persistence, profile_scope`
 *   profile_scope           profile_scope()                          {}
 *   open_persistence        open_persistence(scope)                  {scope}
 *                           (the store context manager's __enter__/__exit__ are
 *                            unobservable no-ops on both sides)
 *   room_grant_is_revoked / peer_room_grant_is_current(store, placement=..., claims=...)
 *   room_grant_claims       self._room_grant_claims(request, permission=...)
 *   local_target            _local_target(claims, _api_request_profile)  {claims}
 *                           -> [profile, installation_id]
 *   local_room_catalog      _local_room_catalog(self, profile, installation_id)
 *                           -> [execution_policy, catalog]
 *   check_auth              self._check_auth(request)                {request}
 *   read_json_body          self._read_json_body(request)            {request} -> [body, error]
 *   issue_invitation        _issue_invitation(self, body, profile)   {self, body, profile}
 *   import_hosted_room_peer / MAX_DISPATCH_GRANT_TTL_SECONDS / issue_room_grant
 *   import_hosted_room_execution_policy / execution_policy_mapping(target_profile=...)
 *   profile_scope_ctx       self._profile_scope(profile)             {profile} (no-op ctx)
 *   time                    time.time()                              {}
 *   uuid4_hex               uuid.uuid4().hex                         {}
 *   revoke_room_grant_scope hosted_rooms.revoke_room_grant_scope(store, placement=..., claims=..., expires_at=...)
 *   openai_error            _openai_error(message, **error_kwargs)   {message, [err_type], code}
 *   web_json_response       web.json_response(body, status=...)      {body, status}
 *
 * Exception kinds: BaseException trio + CancelledError pass through every
 * `except Exception` site; the revoke handler's typed excepts match the
 * OSError / sqlite3.Error / STORE_FAILURES kind lists below.
 */

typedef struct asg_io {
    json_t *(*call)(void *io, const char *name, json_t *args);
} asg_io_t;

static asg_io_t *g_asg_io;
static json_t *g_asg_ctx;            /* borrowed call-context object */
static json_t *g_asg_pending_raise;  /* owned parked raise spec */

static const char *asg_raise_kind(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return NULL;
    json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : NULL;
}

static int asg_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

static int asg_is_oserror(const char *kind) {
    static const char *const kinds[] = {
        "OSError", "PermissionError", "FileExistsError", "IsADirectoryError",
        "NotADirectoryError", "InterruptedError", "TimeoutError",
        "BlockingIOError", "ChildProcessError", "ProcessLookupError",
        "BrokenPipeError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError"
    };
    if (!kind) return 0;
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
        if (strcmp(kind, kinds[i]) == 0) return 1;
    return 0;
}

static int asg_is_sqlite_error(const char *kind) {
    static const char *const kinds[] = {
        "Error", "DatabaseError", "IntegrityError", "InterfaceError",
        "InternalError", "NotSupportedError", "OperationalError",
        "ProgrammingError", "DataError"
    };
    if (!kind) return 0;
    for (size_t i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
        if (strcmp(kind, kinds[i]) == 0) return 1;
    return 0;
}

static int asg_is_store_failure(const char *kind) {
    /* persistence.STORE_FAILURES = (TransientStoreError, PermanentStoreError,
     * StoreUnavailable, StoreReplaced) */
    return kind && (strcmp(kind, "TransientStoreError") == 0 ||
                    strcmp(kind, "PermanentStoreError") == 0 ||
                    strcmp(kind, "StoreUnavailable") == 0 ||
                    strcmp(kind, "StoreReplaced") == 0);
}

static json_t *asg_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static void asg_park(json_t *spec) {
    json_free(g_asg_pending_raise);
    g_asg_pending_raise = spec;
}

/* Seam call: args is OWNED by this wrapper and freed after the call (the
 * dispatcher borrows it for the trace copy). With no channel installed, fail
 * closed with a RuntimeError spec. */
static json_t *asg_seam(const char *name, json_t *args) {
    if (!g_asg_io || !g_asg_io->call) {
        json_free(args);
        char msg[192];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return asg_raise_spec("RuntimeError", msg);
    }
    json_t *r = g_asg_io->call(g_asg_io, name, args);
    json_free(args);
    return r;
}

/* Python truthiness over a JSON value. */
static int asg_truth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int asg_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* str() of a JSON scalar (containers are not probed by these ports). */
static char *asg_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d && fabs(d) < 1e18) {
            snprintf(buf, sizeof buf, "%.0f", d);
        } else {
            snprintf(buf, sizeof buf, "%.17g", d);
        }
        return strdup(buf);
    }
    return strdup("");
}

/* Python str.strip() White_Space set (codepoints). */
static int asg_ws_cp(uint32_t cp) {
    return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) ||
           cp == 0x20 || cp == 0x85 ||
           cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
           cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F ||
           cp == 0x3000;
}

static size_t asg_utf8_decode(const char *s, size_t len, size_t i, uint32_t *cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0 && i + 1 < len) {
        *cp = ((uint32_t)(c & 0x1F) << 6) | ((unsigned char)s[i+1] & 0x3F); return 2;
    }
    if ((c & 0xF0) == 0xE0 && i + 2 < len) {
        *cp = ((uint32_t)(c & 0x0F) << 12) | (((unsigned char)s[i+1] & 0x3F) << 6) |
              ((unsigned char)s[i+2] & 0x3F); return 3;
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < len) {
        *cp = ((uint32_t)(c & 0x07) << 18) | (((unsigned char)s[i+1] & 0x3F) << 12) |
              (((unsigned char)s[i+2] & 0x3F) << 6) | ((unsigned char)s[i+3] & 0x3F); return 4;
    }
    *cp = c; return 1;
}

/* malloc'd copy of s with Python str.strip() applied. */
static char *asg_strip(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s), a = 0, b = len;
    while (a < b) {
        uint32_t cp; size_t n = asg_utf8_decode(s, len, a, &cp);
        if (!asg_ws_cp(cp)) break;
        a += n;
    }
    while (b > a) {
        size_t i = b - 1;
        while (i > a && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
        uint32_t cp; size_t n = asg_utf8_decode(s, len, i, &cp);
        if (!asg_ws_cp(cp) || i + n != b) break;
        b = i;
    }
    char *out = malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

/* float() coercion. Returns 0 ok / -1 raise (spec in *err). */
/* Python type name of a JSON value (for TypeError messages). */
static const char *asg_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static int asg_py_float(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = asg_raise_spec("TypeError", "float() argument must be a string or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v->type != JSON_STRING) {
        *err = asg_raise_spec("TypeError", "float() argument must be a string or a real number");
        return -1;
    }
    char *t = asg_strip(v->str_val);
    const char *p = t;
    int neg = 0;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    double val;
    if ((p[0]=='i'||p[0]=='I') && (p[1]=='n'||p[1]=='N') && (p[2]=='f'||p[2]=='F') && p[3]=='\0') {
        val = neg ? -INFINITY : INFINITY;
        free(t); *out = val; return 0;
    }
    if ((p[0]=='n'||p[0]=='N') && (p[1]=='a'||p[1]=='A') && (p[2]=='n'||p[2]=='N') && p[3]=='\0') {
        free(t); *out = NAN; return 0;
    }
    {
        const char *start = p;
        int ndig = 0, ndot = 0, bad = 0;
        while (*p) {
            if (*p >= '0' && *p <= '9') { ndig++; p++; }
            else if (*p == '_') {
                if (p == start || !(p[-1] >= '0' && p[-1] <= '9') ||
                    !(p[1] >= '0' && p[1] <= '9')) { bad = 1; break; }
                p++;
            }
            else if (*p == '.' && !ndot) { ndot++; p++; }
            else break;
        }
        if (!bad && !ndig) bad = 1;
        if (!bad && (*p == 'e' || *p == 'E')) {
            p++;
            if (*p == '+' || *p == '-') p++;
            int ne = 0;
            while (*p >= '0' && *p <= '9') { ne++; p++; }
            if (!ne) bad = 1;
        }
        if (!bad && *p != '\0') bad = 1;
        if (bad) {
            free(t);
            *err = asg_raise_spec("ValueError", "could not convert string to float");
            return -1;
        }
        char *clean = malloc(strlen(start) + 1);
        size_t k = 0;
        for (const char *q = start; *q; q++) if (*q != '_') clean[k++] = *q;
        clean[k] = '\0';
        val = strtod(clean, NULL);
        free(clean);
    }
    free(t);
    *out = neg ? -val : val;
    return 0;
}

/* int() coercion for authority_epoch (number truncates toward zero). */
static int asg_py_int(const json_t *v, long long *out, json_t **err) {
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = asg_raise_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return -1;
    }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 0; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v->type != JSON_STRING) {
        *err = asg_raise_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number");
        return -1;
    }
    char *t = asg_strip(v->str_val);
    char *end = NULL;
    long long n = strtoll(t, &end, 10);
    if (end == t || *end != '\0') {
        free(t);
        *err = asg_raise_spec("ValueError", "invalid literal for int() with base 10");
        return -1;
    }
    free(t);
    *out = n;
    return 0;
}

/* Serialize an OWNED node as the port's char* return; frees the node. */
static char *asg_ret(json_t *owned) {
    if (!owned) return strdup("null");
    char *s = json_serialize(owned);
    json_free(owned);
    return s ? s : strdup("null");
}

static json_t *asg_ctx_get(const char *key) {
    return g_asg_ctx ? json_obj_get(g_asg_ctx, key) : NULL;
}

/* _json_error: openai_error(message, **error_kwargs) then web.json_response.
 * error_kwargs carries err_type (when non-NULL) before code. Returns the
 * response node, or NULL with a raise parked. */
static json_t *asg_json_error(const char *message, const char *code,
                              const char *err_type, int status) {
    json_t *args = json_object();
    json_set_hdr(args, "message", message);
    if (err_type) json_set_hdr(args, "err_type", err_type);
    json_set_hdr(args, "code", code);
    json_t *env = asg_seam("openai_error", args);
    if (asg_raise_kind(env)) { asg_park(env); return NULL; }
    json_t *wargs = json_object();
    json_set(wargs, "body", env); /* adopts */
    json_set(wargs, "status", json_number(status));
    json_t *resp = asg_seam("web_json_response", wargs);
    if (asg_raise_kind(resp)) { asg_park(resp); return NULL; }
    return resp;
}

/* _room_grant_error_response: 403 for RoomGrantReauthorizationRequired,
 * else 401 invalid_room_grant. */
static json_t *asg_grant_error_response(const char *exc_kind) {
    if (exc_kind && strcmp(exc_kind, "RoomGrantReauthorizationRequired") == 0) {
        return asg_json_error("Room authorization needs to be renewed.",
                              "room_reauthorization_required", "gateway_auth_error", 403);
    }
    return asg_json_error("Room authorization is invalid or expired.",
                          "invalid_room_grant", "gateway_auth_error", 401);
}

/* _hard_expiry: float(claims.get("status_expires_at", claims["expires_at"])).
 * The .get DEFAULT is evaluated EAGERLY: claims["expires_at"] raises KeyError
 * whenever expires_at is absent, even if status_expires_at exists.
 * Returns 0 ok / -1 raise parked. */
static int asg_hard_expiry(const json_t *claims, double *out) {
    json_t *dflt = json_obj_get(claims, "expires_at");
    if (!dflt) {
        asg_park(asg_raise_spec("KeyError", "'expires_at'"));
        return -1;
    }
    json_t *v = json_obj_get(claims, "status_expires_at");
    if (!v) v = dflt;
    json_t *err = NULL;
    if (asg_py_float(v, out, &err) < 0) { asg_park(err); return -1; }
    return 0;
}

/* PoP: declared_conversation_session @ gateway/platforms/api_server.py:_declared_conversation_session */
char *declared_conversation_session(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *keynode = asg_ctx_get("gateway_session_key");
    /* key = (gateway_session_key or "").strip() */
    char *key = asg_strip(asg_truth(keynode) && keynode->type == JSON_STRING ? keynode->str_val : "");
    if (!key || !key[0]) { free(key); return strdup("null"); }
    json_t *db = asg_seam("ensure_store", json_object());
    if (asg_raise_kind(db)) { free(key); asg_park(db); return NULL; }
    if (asg_is_null(db)) { free(key); json_free(db); return strdup("null"); }
    int authority = json_get_bool(g_asg_ctx, "session_authority_present", false);
    if (authority) {
        json_t *imp = asg_seam("import_declared_api_session", json_object());
        if (asg_raise_kind(imp)) { free(key); json_free(db); asg_park(imp); return NULL; }
        json_free(imp);
        /* kwargs evaluate left-to-right: persistence=self._store_beside(db) */
        json_t *sbargs = json_object();
        json_set(sbargs, "db", json_copy(db));
        json_t *persistence = asg_seam("store_beside", sbargs);
        if (asg_raise_kind(persistence)) { free(key); json_free(db); asg_park(persistence); return NULL; }
        json_t *dargs = json_object();
        json_set(dargs, "db", json_copy(db));
        json_set_hdr(dargs, "key", key);
        json_set(dargs, "persistence", persistence); /* adopts */
        json_t *r = asg_seam("declared_api_session", dargs);
        free(key); json_free(db);
        if (asg_raise_kind(r)) { asg_park(r); return NULL; }
        return asg_ret(r);
    }
    json_t *sbargs = json_object();
    json_set(sbargs, "db", json_copy(db));
    json_t *store = asg_seam("store_beside", sbargs);
    if (asg_raise_kind(store)) { free(key); json_free(db); asg_park(store); return NULL; }
    if (asg_is_null(store)) { free(key); json_free(db); json_free(store); return strdup("null"); }
    json_t *largs = json_object();
    json_set_hdr(largs, "source", json_get_str(g_asg_ctx, "session_source", ""));
    json_set_hdr(largs, "session_key", key);
    json_t *found = asg_seam("latest_session_for_peer", largs);
    free(key); json_free(db); json_free(store);
    const char *kind = asg_raise_kind(found);
    if (kind) {
        if (asg_is_base_exc(kind)) { asg_park(found); return NULL; }
        /* except Exception: logger.debug (unobservable) -> None */
        json_free(found);
        return strdup("null");
    }
    /* str(found.session.session_id) if found and found.session.session_id else None */
    json_t *sid = NULL;
    if (asg_truth(found)) {
        json_t *session = json_obj_get(found, "session");
        if (session) sid = json_obj_get(session, "session_id");
    }
    if (!asg_truth(sid)) { json_free(found); return strdup("null"); }
    char *s = asg_py_str(sid);
    json_t *out = json_string(s);
    free(s); json_free(found);
    return asg_ret(out);
}

/* PoP: run_idempotency_scope @ gateway/platforms/api_server.py:_run_idempotency_scope */
char *run_idempotency_scope(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *args = json_object();
    json_set(args, "self", json_copy(asg_ctx_get("self")));
    json_set(args, "request", json_copy(asg_ctx_get("request")));
    json_t *r = asg_seam("api_runs.run_idempotency_scope", args);
    if (asg_raise_kind(r)) { asg_park(r); return NULL; }
    return asg_ret(r);
}

/* Composed pure logic of api_server.py:_room_grant_token ->
 * api_server_room_grants.py:_room_grant_token (both live defs are pure). */
/* PoP: room_grant_token @ gateway/platforms/api_server.py:_room_grant_token */
char *room_grant_token(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *request = asg_ctx_get("request");
    json_t *headers = request ? json_obj_get(request, "headers") : NULL;
    json_t *auth = headers ? json_obj_get(headers, "Authorization") : NULL;
    /* str(request.headers.get("Authorization") or "") */
    char *s = asg_truth(auth) ? asg_py_str(auth) : strdup("");
    /* partition(" ") */
    char *sp = strchr(s, ' ');
    char *out;
    if (sp) {
        *sp = '\0';
        const char *scheme = s;
        const char *token = sp + 1;
        /* scheme.lower() == "hermesroom" (ASCII lower; the live compare is
         * str.lower which is Unicode-aware, but the target is ASCII) */
        int match = strlen(scheme) == 10;
        if (match) {
            static const char *want = "hermesroom";
            for (int i = 0; i < 10; i++) {
                char c = scheme[i];
                if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
                if (c != want[i]) { match = 0; break; }
            }
        }
        out = match ? asg_strip(token) : strdup("");
    } else {
        out = strdup("");
    }
    free(s);
    json_t *r = json_string(out);
    free(out);
    return asg_ret(r);
}

/* bytes are modeled as JSON strings on both sides. */
/* PoP: room_grant_secret @ gateway/platforms/api_server.py:_room_grant_secret */
char *room_grant_secret(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *imp = asg_seam("import_gateway_room_grant_secret", json_object());
    if (asg_raise_kind(imp)) { asg_park(imp); return NULL; }
    json_free(imp);
    json_t *r = asg_seam("gateway_room_grant_secret", json_object());
    if (asg_raise_kind(r)) { asg_park(r); return NULL; }
    return asg_ret(r);
}

/* _decode_request_grant composed inline: the from-import runs FIRST (live
 * statement order), then token check, secret, decode.
 * Returns claims node or NULL with raise parked. */
static json_t *asg_decode_request_grant(const json_t *request, const char *permission) {
    json_t *imp = asg_seam("import_decode_room_grant", json_object());
    if (asg_raise_kind(imp)) { asg_park(imp); return NULL; }
    json_free(imp);
    json_t *targs = json_object();
    json_set(targs, "request", json_copy(request));
    json_t *token = asg_seam("room_grant_token", targs);
    if (asg_raise_kind(token)) { asg_park(token); return NULL; }
    if (!asg_truth(token)) {
        json_free(token);
        asg_park(asg_raise_spec("ValueError", "room grant is missing"));
        return NULL;
    }
    json_t *secret = asg_seam("room_grant_secret", json_object());
    if (asg_raise_kind(secret)) { json_free(token); asg_park(secret); return NULL; }
    json_t *dargs = json_object();
    json_set(dargs, "secret", secret); /* adopts */
    json_set(dargs, "token", token);   /* adopts */
    json_set_hdr(dargs, "permission", permission);
    json_t *claims = asg_seam("decode_room_grant", dargs);
    if (asg_raise_kind(claims)) { asg_park(claims); return NULL; }
    return claims;
}

/* _room_store composed inline: import, profile_scope(), open_persistence().
 * The store context manager enter/exit are unobservable no-ops. */
static json_t *asg_room_store(void) {
    json_t *imp = asg_seam("import_persistence_assembly", json_object());
    if (asg_raise_kind(imp)) { asg_park(imp); return NULL; }
    json_free(imp);
    json_t *scope = asg_seam("profile_scope", json_object());
    if (asg_raise_kind(scope)) { asg_park(scope); return NULL; }
    json_t *oargs = json_object();
    json_set(oargs, "scope", scope); /* adopts */
    json_t *store = asg_seam("open_persistence", oargs);
    if (asg_raise_kind(store)) { asg_park(store); return NULL; }
    return store;
}

/* Composed api_server_room_grants.py:_room_grant_claims + _decode_request_grant. */
/* PoP: room_grant_claims @ gateway/platforms/api_server.py:_room_grant_claims */
char *room_grant_claims(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *request = asg_ctx_get("request");
    const char *permission = json_get_str(g_asg_ctx, "permission", "status");
    json_t *claims = asg_decode_request_grant(request, permission);
    if (!claims) return NULL;
    json_t *imp = asg_seam("import_hosted_rooms", json_object());
    if (asg_raise_kind(imp)) { json_free(claims); asg_park(imp); return NULL; }
    json_free(imp);
    json_t *store = asg_room_store();
    if (!store) { json_free(claims); return NULL; }
    json_t *rargs = json_object();
    json_set(rargs, "store", json_copy(store));
    json_set_hdr(rargs, "placement", "shared");
    json_set(rargs, "claims", json_copy(claims));
    json_t *revoked = asg_seam("room_grant_is_revoked", rargs);
    if (asg_raise_kind(revoked)) { json_free(claims); json_free(store); asg_park(revoked); return NULL; }
    int rev = asg_truth(revoked);
    json_free(revoked);
    if (rev) {
        json_free(claims); json_free(store);
        asg_park(asg_raise_spec("RoomGrantReauthorizationRequired", "room grant is revoked"));
        return NULL;
    }
    json_t *cargs = json_object();
    json_set(cargs, "store", json_copy(store));
    json_set_hdr(cargs, "placement", "shared");
    json_set(cargs, "claims", json_copy(claims));
    json_t *current = asg_seam("peer_room_grant_is_current", cargs);
    if (asg_raise_kind(current)) { json_free(claims); json_free(store); asg_park(current); return NULL; }
    int cur = asg_truth(current);
    json_free(current); json_free(store);
    if (!cur) {
        json_free(claims);
        asg_park(asg_raise_spec("RoomGrantReauthorizationRequired", "room grant is no longer current"));
        return NULL;
    }
    return asg_ret(claims);
}

/* PoP: normalize_room_dispatch @ gateway/platforms/api_server.py:_normalize_room_dispatch */
char *normalize_room_dispatch(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *args = json_object();
    json_set(args, "self", json_copy(asg_ctx_get("self")));
    json_set(args, "request", json_copy(asg_ctx_get("request")));
    json_set(args, "body", json_copy(asg_ctx_get("body")));
    json_t *r = asg_seam("room_dispatch.normalize_room_dispatch", args);
    if (asg_raise_kind(r)) { asg_park(r); return NULL; }
    return asg_ret(r);
}

/* PoP: durable_run_status @ gateway/platforms/api_server.py:_durable_run_status */
char *durable_run_status(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *args = json_object();
    json_set(args, "self", json_copy(asg_ctx_get("self")));
    json_set(args, "request", json_copy(asg_ctx_get("request")));
    json_set(args, "run_id", json_copy(asg_ctx_get("run_id")));
    json_t *r = asg_seam("api_runs.durable_run_status", args);
    if (asg_raise_kind(r)) { asg_park(r); return NULL; }
    return asg_ret(r);
}

/* read_json_body seam -> [body, error]; on truthy error the handler returns
 * it. Returns 0 ok (body/error borrowed from *pair), -1 raise parked. */
static int asg_read_body(json_t **pair, json_t **body, json_t **error) {
    json_t *args = json_object();
    json_set(args, "request", json_copy(asg_ctx_get("request")));
    json_t *r = asg_seam("read_json_body", args);
    if (asg_raise_kind(r)) { asg_park(r); return -1; }
    if (!r || r->type != JSON_ARRAY || r->c.count != 2) {
        json_free(r);
        asg_park(asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)"));
        return -1;
    }
    *pair = r;
    *body = r->c.items[0];
    *error = r->c.items[1];
    return 0;
}

/* PoP: handle_room_member_capabilities @ gateway/platforms/api_server_room_grants.py:_handle_room_member_capabilities */
char *handle_room_member_capabilities(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    static const char *const fields[] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id"
    };
    json_t *claims = NULL, *profile = NULL, *catalog = NULL;
    json_t *exc_spec = NULL;
    /* try: claims / local_target / local_room_catalog */
    json_t *cargs = json_object();
    json_set(cargs, "request", json_copy(asg_ctx_get("request")));
    json_set_hdr(cargs, "permission", "status");
    claims = asg_seam("room_grant_claims", cargs);
    if (asg_raise_kind(claims)) { exc_spec = claims; claims = NULL; goto except; }
    json_t *ltargs = json_object();
    json_set(ltargs, "claims", json_copy(claims));
    json_t *lt = asg_seam("local_target", ltargs);
    if (asg_raise_kind(lt)) { exc_spec = lt; goto except; }
    if (!lt || lt->type != JSON_ARRAY || lt->c.count != 2) {
        json_free(lt);
        exc_spec = asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)");
        goto except;
    }
    profile = json_copy(lt->c.items[0]);
    json_t *install = json_copy(lt->c.items[1]);
    json_free(lt);
    json_t *lcargs = json_object();
    json_set(lcargs, "profile", json_copy(profile));
    json_set(lcargs, "installation_id", install); /* adopts */
    json_t *lc = asg_seam("local_room_catalog", lcargs);
    if (asg_raise_kind(lc)) { exc_spec = lc; goto except; }
    if (!lc || lc->type != JSON_ARRAY || lc->c.count != 2) {
        json_free(lc);
        exc_spec = asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)");
        goto except;
    }
    catalog = json_copy(lc->c.items[1]);
    json_free(lc);
    /* success body: object + identity fields (KeyError -> except) + target_profile + catalog */
    {
        json_t *body = json_object();
        json_set_hdr(body, "object", "hermes.room_member.capabilities");
        for (size_t i = 0; i < 5; i++) {
            json_t *v = json_obj_get(claims, fields[i]);
            if (!v) {
                /* claims[k] sits OUTSIDE the live try/except — the KeyError
                 * propagates out of the handler (no error response). */
                json_free(body);
                char msg[96];
                snprintf(msg, sizeof msg, "'%s'", fields[i]);
                json_free(claims); json_free(profile); json_free(catalog);
                asg_park(asg_raise_spec("KeyError", msg));
                return NULL;
            }
            json_set(body, fields[i], json_copy(v));
        }
        json_set(body, "target_profile", profile); profile = NULL; /* adopts */
        json_set(body, "catalog", catalog); catalog = NULL;        /* adopts */
        json_t *wargs = json_object();
        json_set(wargs, "body", body);
        json_set(wargs, "status", json_number(200));
        json_t *resp = asg_seam("web_json_response", wargs);
        json_free(claims);
        if (asg_raise_kind(resp)) { asg_park(resp); return NULL; }
        return asg_ret(resp);
    }
except:
    json_free(claims); json_free(profile); json_free(catalog);
    const char *exc_kind = asg_raise_kind(exc_spec);
    if (asg_is_base_exc(exc_kind)) { asg_park(exc_spec); return NULL; }
    json_t *resp = asg_grant_error_response(exc_kind);
    json_free(exc_spec);
    if (!resp) return NULL;
    return asg_ret(resp);
}

/* PoP: handle_room_member_grant_revoke @ gateway/platforms/api_server_room_grants.py:_handle_room_member_grant_revoke */
char *handle_room_member_grant_revoke(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *pair = NULL, *body = NULL, *error = NULL;
    if (asg_read_body(&pair, &body, &error) < 0) return NULL;
    if (asg_truth(error)) {
        char *s = asg_ret(json_copy(error));
        json_free(pair);
        return s;
    }
    if (asg_truth(body)) {
        json_free(pair);
        json_t *resp = asg_json_error("Grant revoke accepts no fields.",
                                      "invalid_room_grant_revoke", NULL, 400);
        if (!resp) return NULL;
        return asg_ret(resp);
    }
    json_free(pair); pair = NULL;
    json_t *imp = asg_seam("import_hosted_rooms", json_object());
    if (asg_raise_kind(imp)) { asg_park(imp); return NULL; }
    json_free(imp);
    /* try: decode grant, local_target, store revoke */
    json_t *claims = asg_decode_request_grant(asg_ctx_get("request"), "status");
    if (!claims) goto typed_except;
    json_t *ltargs = json_object();
    json_set(ltargs, "claims", json_copy(claims));
    json_t *lt = asg_seam("local_target", ltargs);
    if (asg_raise_kind(lt)) { json_free(claims); claims = lt; goto typed_except; }
    if (!lt || lt->type != JSON_ARRAY || lt->c.count != 2) {
        json_free(lt); json_free(claims);
        claims = asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)");
        goto typed_except;
    }
    json_free(lt);
    json_t *store = asg_room_store();
    if (!store) { json_free(claims); claims = NULL; goto typed_except; }
    {
        double hard;
        if (asg_hard_expiry(claims, &hard) < 0) {
            json_free(store); json_free(claims);
            claims = NULL;
            goto typed_except;
        }
        json_t *rargs = json_object();
        json_set(rargs, "store", json_copy(store));
        json_set_hdr(rargs, "placement", "shared");
        json_set(rargs, "claims", json_copy(claims));
        json_set(rargs, "expires_at", json_number(hard));
        json_t *rv = asg_seam("revoke_room_grant_scope", rargs);
        json_free(store); json_free(claims); claims = NULL;
        if (asg_raise_kind(rv)) { claims = rv; goto typed_except; }
        json_free(rv);
    }
    {
        json_t *rbody = json_object();
        json_set_hdr(rbody, "object", "hermes.room_member.grant.revocation");
        json_set(rbody, "revoked", json_bool(true));
        json_t *wargs = json_object();
        json_set(wargs, "body", rbody);
        json_set(wargs, "status", json_number(200));
        json_t *resp = asg_seam("web_json_response", wargs);
        if (asg_raise_kind(resp)) { asg_park(resp); return NULL; }
        return asg_ret(resp);
    }
typed_except:
    {
        /* claims is NULL (a helper parked the spec) or the raise spec itself */
        json_t *spec = claims ? claims : g_asg_pending_raise;
        if (!claims) g_asg_pending_raise = NULL;
        const char *kind = asg_raise_kind(spec);
        if (asg_is_base_exc(kind)) { asg_park(spec); return NULL; }
        if (asg_is_sqlite_error(kind) || asg_is_oserror(kind) || asg_is_store_failure(kind)) {
            json_free(spec);
            json_t *resp = asg_json_error("Room grant store is unavailable.",
                                          "room_grant_store_unavailable", "server_error", 503);
            if (!resp) return NULL;
            return asg_ret(resp);
        }
        if (kind && (strcmp(kind, "ValueError") == 0 || strcmp(kind, "KeyError") == 0 ||
                     strcmp(kind, "TypeError") == 0)) {
            json_free(spec);
            json_t *resp = asg_grant_error_response(NULL);
            if (!resp) return NULL;
            return asg_ret(resp);
        }
        /* no matching except clause: propagate */
        asg_park(spec);
        return NULL;
    }
}

/* PoP: handle_room_member_grant_refresh @ gateway/platforms/api_server_room_grants.py:_handle_room_member_grant_refresh */
char *handle_room_member_grant_refresh(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *pair = NULL, *body = NULL, *error = NULL;
    if (asg_read_body(&pair, &body, &error) < 0) return NULL;
    if (asg_truth(error)) {
        char *s = asg_ret(json_copy(error));
        json_free(pair);
        return s;
    }
    /* if set(body) - {"ttl_seconds"}: 400 */
    if (!body || body->type != JSON_OBJECT) {
        json_free(pair);
        asg_park(asg_raise_spec("TypeError", "'NoneType' object is not iterable"));
        return NULL;
    }
    for (size_t i = 0; i < body->c.count; i++) {
        if (strcmp(body->c.keys[i], "ttl_seconds") != 0) {
            json_free(pair);
            json_t *resp = asg_json_error("Grant refresh accepts only ttl_seconds.",
                                          "invalid_room_grant_refresh", NULL, 400);
            if (!resp) return NULL;
            return asg_ret(resp);
        }
    }
    /* detach ttl_seconds before freeing the pair (body borrows from it) */
    json_t *reqv = json_obj_get(body, "ttl_seconds");
    json_t *reqv_copy = reqv ? json_copy(reqv) : NULL;
    json_free(pair); pair = NULL;
    /* try block — live wraps everything through issue_room_grant in
     * `except Exception -> _room_grant_error_response(exc)`; BaseException
     * kinds pass through. All owned pointers NULL-init; the except label
     * frees whatever is still owned (adopted sources are NULLed). */
    json_t *claims = NULL, *profile = NULL, *install = NULL, *exec_policy = NULL;
    json_t *secret = NULL, *iargs = NULL, *token = NULL, *exc_spec = NULL;
    json_t *maxttl = NULL;
    double now = 0, hard = 0, dispatch_ttl = 0;
    json_t *imp = asg_seam("import_hosted_room_peer", json_object());
    if (asg_raise_kind(imp)) { exc_spec = imp; goto refresh_except; }
    json_free(imp);
    maxttl = asg_seam("MAX_DISPATCH_GRANT_TTL_SECONDS", json_object());
    if (asg_raise_kind(maxttl)) { exc_spec = maxttl; maxttl = NULL; goto refresh_except; }
    json_t *imp2 = asg_seam("import_hosted_room_execution_policy", json_object());
    if (asg_raise_kind(imp2)) { exc_spec = imp2; goto refresh_except; }
    json_free(imp2);
    json_t *cargs = json_object();
    json_set(cargs, "request", json_copy(asg_ctx_get("request")));
    json_set_hdr(cargs, "permission", "dispatch");
    claims = asg_seam("room_grant_claims", cargs);
    if (asg_raise_kind(claims)) { exc_spec = claims; claims = NULL; goto refresh_except; }
    json_t *ltargs = json_object();
    json_set(ltargs, "claims", json_copy(claims));
    json_t *lt = asg_seam("local_target", ltargs);
    if (asg_raise_kind(lt)) { exc_spec = lt; goto refresh_except; }
    if (!lt || lt->type != JSON_ARRAY || lt->c.count != 2) {
        json_free(lt);
        exc_spec = asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)");
        goto refresh_except;
    }
    profile = json_copy(lt->c.items[0]);
    install = json_copy(lt->c.items[1]);
    json_free(lt);
    json_t *tnow = asg_seam("time", json_object());
    if (asg_raise_kind(tnow)) { exc_spec = tnow; goto refresh_except; }
    if (tnow && tnow->type == JSON_NUMBER) now = tnow->num_val;
    else if (tnow && tnow->type == JSON_BOOL) now = tnow->bool_val ? 1.0 : 0.0;
    else {
        char msg[160];
        snprintf(msg, sizeof msg, "unsupported operand type(s) for -: 'float' and '%s'",
                 asg_type_name(tnow));
        json_free(tnow);
        exc_spec = asg_raise_spec("TypeError", msg);
        goto refresh_except;
    }
    json_free(tnow);
    if (asg_hard_expiry(claims, &hard) < 0) {
        exc_spec = g_asg_pending_raise; g_asg_pending_raise = NULL;
        goto refresh_except;
    }
    double remaining = hard - now;
    double requested;
    {
        json_t *err = NULL;
        const json_t *src = reqv_copy ? reqv_copy : maxttl;
        if (asg_py_float(src, &requested, &err) < 0) {
            exc_spec = err;
            goto refresh_except;
        }
    }
    json_free(reqv_copy); reqv_copy = NULL;
    if (remaining <= 0 || requested <= 0) {
        exc_spec = asg_raise_spec("ValueError", "room grant renewal horizon expired");
        goto refresh_except;
    }
    /* min(requested, MAX_DISPATCH_GRANT_TTL_SECONDS, remaining) */
    if (!maxttl || (maxttl->type != JSON_NUMBER && maxttl->type != JSON_BOOL)) {
        char msg[160];
        snprintf(msg, sizeof msg, "'<' not supported between instances of '%s' and 'float'",
                 asg_type_name(maxttl));
        exc_spec = asg_raise_spec("TypeError", msg);
        goto refresh_except;
    }
    dispatch_ttl = requested;
    {
        double mv = maxttl->type == JSON_BOOL ? (maxttl->bool_val ? 1.0 : 0.0) : maxttl->num_val;
        if (mv < dispatch_ttl) dispatch_ttl = mv;
    }
    if (remaining < dispatch_ttl) dispatch_ttl = remaining;
    json_free(maxttl); maxttl = NULL;
    /* with self._profile_scope(profile): execution_policy_mapping(target_profile=profile) */
    json_t *psargs = json_object();
    json_set(psargs, "profile", json_copy(profile));
    json_t *ps = asg_seam("profile_scope_ctx", psargs);
    if (asg_raise_kind(ps)) { exc_spec = ps; goto refresh_except; }
    json_free(ps);
    json_t *epargs = json_object();
    json_set(epargs, "target_profile", json_copy(profile));
    exec_policy = asg_seam("execution_policy_mapping", epargs);
    if (asg_raise_kind(exec_policy)) { exc_spec = exec_policy; exec_policy = NULL; goto refresh_except; }
    /* _require_unchanged_execution_policy */
    {
        json_t *pd = json_obj_get(exec_policy, "policy_digest");
        json_t *cd = json_obj_get(claims, "execution_policy_digest");
        char *a = asg_truth(pd) ? asg_py_str(pd) : strdup("");
        char *b = asg_truth(cd) ? asg_py_str(cd) : strdup("");
        int changed = strcmp(a, b) != 0;
        free(a); free(b);
        if (changed) {
            exc_spec = asg_raise_spec("RoomGrantReauthorizationRequired", "room execution policy changed");
            goto refresh_except;
        }
    }
    /* issue_room_grant(secret, grant_id=f"grant-refresh-{uuid4().hex}",
     *                  **_room_identity(claims), ...) — kwargs left-to-right */
    secret = asg_seam("room_grant_secret", json_object());
    if (asg_raise_kind(secret)) { exc_spec = secret; secret = NULL; goto refresh_except; }
    json_t *uuid = asg_seam("uuid4_hex", json_object());
    if (asg_raise_kind(uuid)) { exc_spec = uuid; goto refresh_except; }
    char gid[160];
    snprintf(gid, sizeof gid, "grant-refresh-%s",
             (uuid && uuid->type == JSON_STRING) ? uuid->str_val : "");
    json_free(uuid);
    /* _room_identity(claims): the five identity fields, authority_epoch int() */
    static const char *const idfields[] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id"
    };
    iargs = json_object();
    json_set(iargs, "secret", secret); secret = NULL; /* adopts */
    json_set_hdr(iargs, "grant_id", gid);
    for (size_t i = 0; i < 5; i++) {
        json_t *v = json_obj_get(claims, idfields[i]);
        if (!v) {
            char msg[96];
            snprintf(msg, sizeof msg, "'%s'", idfields[i]);
            exc_spec = asg_raise_spec("KeyError", msg);
            goto refresh_except;
        }
        if (strcmp(idfields[i], "authority_epoch") == 0) {
            long long n; json_t *err = NULL;
            if (asg_py_int(v, &n, &err) < 0) {
                exc_spec = err;
                goto refresh_except;
            }
            json_set(iargs, idfields[i], json_number((double)n));
        } else {
            json_set(iargs, idfields[i], json_copy(v));
        }
    }
    json_set(iargs, "target_install_id", install); install = NULL; /* adopts */
    json_set(iargs, "target_profile", json_copy(profile));
    {
        json_t *pd = json_obj_get(exec_policy, "policy_digest");
        if (!pd) {
            exc_spec = asg_raise_spec("KeyError", "'policy_digest'");
            goto refresh_except;
        }
        json_set(iargs, "execution_policy_digest", json_copy(pd));
    }
    {
        json_t *perms = json_obj_get(claims, "permissions");
        if (!perms) {
            exc_spec = asg_raise_spec("KeyError", "'permissions'");
            goto refresh_except;
        }
        json_set(iargs, "permissions", json_copy(perms));
    }
    json_set(iargs, "issued_at", json_number(now));
    json_set(iargs, "ttl_seconds", json_number(dispatch_ttl));
    json_set(iargs, "status_expires_at", json_number(hard));
    token = asg_seam("issue_room_grant", iargs);
    iargs = NULL; /* args freed by the seam wrapper */
    if (asg_raise_kind(token)) { exc_spec = token; token = NULL; goto refresh_except; }
    /* success body (OUTSIDE the live try) */
    {
        json_t *rbody = json_object();
        json_set_hdr(rbody, "object", "hermes.room_member.grant");
        json_set(rbody, "grant", token); token = NULL; /* adopts */
        json_set(rbody, "expires_at", json_number(now + dispatch_ttl));
        json_set(rbody, "status_expires_at", json_number(hard));
        json_set(rbody, "execution_policy", exec_policy); exec_policy = NULL; /* adopts */
        json_t *wargs = json_object();
        json_set(wargs, "body", rbody);
        json_set(wargs, "status", json_number(200));
        json_t *resp = asg_seam("web_json_response", wargs);
        json_free(claims); json_free(profile);
        if (asg_raise_kind(resp)) { asg_park(resp); return NULL; }
        return asg_ret(resp);
    }
refresh_except:
    json_free(reqv_copy); json_free(maxttl); json_free(claims); json_free(profile);
    json_free(install); json_free(exec_policy); json_free(secret); json_free(iargs);
    json_free(token);
    {
        const char *kind = asg_raise_kind(exc_spec);
        if (asg_is_base_exc(kind)) { asg_park(exc_spec); return NULL; }
        json_t *resp = asg_grant_error_response(kind);
        json_free(exc_spec);
        if (!resp) return NULL;
        return asg_ret(resp);
    }
}

/* PoP: handle_room_member_invitation @ gateway/platforms/api_server_room_grants.py:_handle_room_member_invitation */
char *handle_room_member_invitation(void) {
    json_free(g_asg_pending_raise); g_asg_pending_raise = NULL;
    json_t *aargs = json_object();
    json_set(aargs, "request", json_copy(asg_ctx_get("request")));
    json_t *auth_err = asg_seam("check_auth", aargs);
    if (asg_raise_kind(auth_err)) { asg_park(auth_err); return NULL; }
    if (asg_truth(auth_err)) return asg_ret(auth_err);
    json_free(auth_err);
    json_t *pair = NULL, *body = NULL, *error = NULL;
    if (asg_read_body(&pair, &body, &error) < 0) return NULL;
    if (asg_truth(error)) {
        char *s = asg_ret(json_copy(error));
        json_free(pair);
        return s;
    }
    /* required/allowed field check */
    static const char *const required[] = {
        "room_id", "home_install_id", "authority_gateway_id", "authority_epoch", "member_id"
    };
    static const char *const extra[] = { "grant_id", "ttl_seconds", "status_ttl_seconds" };
    int bad = 0;
    if (!body || body->type != JSON_OBJECT) {
        json_free(pair);
        asg_park(asg_raise_spec("TypeError", "'NoneType' object is not iterable"));
        return NULL;
    }
    for (size_t i = 0; i < body->c.count && !bad; i++) {
        const char *k = body->c.keys[i];
        int ok = 0;
        for (size_t j = 0; j < 5; j++) if (strcmp(k, required[j]) == 0) ok = 1;
        for (size_t j = 0; j < 3; j++) if (strcmp(k, extra[j]) == 0) ok = 1;
        if (!ok) bad = 1;
    }
    for (size_t j = 0; j < 5 && !bad; j++)
        if (!json_obj_get(body, required[j])) bad = 1;
    if (bad) {
        json_free(pair);
        json_t *resp = asg_json_error("Invitation is missing required room authority fields.",
                                      "invalid_room_invitation", NULL, 400);
        if (!resp) return NULL;
        return asg_ret(resp);
    }
    /* try: profile, _ = _local_target(None, ...); invitation = _issue_invitation(...) */
    json_t *ltargs = json_object();
    json_set(ltargs, "claims", json_null());
    json_t *lt = asg_seam("local_target", ltargs);
    const char *kind = asg_raise_kind(lt);
    if (!kind && (!lt || lt->type != JSON_ARRAY || lt->c.count != 2)) {
        json_free(lt);
        lt = asg_raise_spec("ValueError", "not enough values to unpack (expected 2, got 0)");
        kind = asg_raise_kind(lt);
    }
    if (kind) {
        if (asg_is_base_exc(kind)) { json_free(pair); asg_park(lt); return NULL; }
        if (asg_is_oserror(kind) || asg_is_sqlite_error(kind) || asg_is_store_failure(kind) ||
            strcmp(kind, "RuntimeError") == 0 || strcmp(kind, "TypeError") == 0 ||
            strcmp(kind, "ValueError") == 0) {
            const char *msg = json_get_str(lt, "message", "");
            json_free(lt); json_free(pair);
            json_t *resp = asg_json_error(msg, "invalid_room_invitation", NULL, 400);
            if (!resp) return NULL;
            return asg_ret(resp);
        }
        json_free(pair);
        asg_park(lt);
        return NULL;
    }
    json_t *profile = json_copy(lt->c.items[0]);
    json_free(lt);
    json_t *iargs = json_object();
    json_set(iargs, "self", json_copy(asg_ctx_get("self")));
    json_set(iargs, "body", json_copy(body));
    json_set(iargs, "profile", profile); /* adopts */
    json_t *inv = asg_seam("issue_invitation", iargs);
    kind = asg_raise_kind(inv);
    if (kind) {
        if (asg_is_base_exc(kind)) { json_free(pair); asg_park(inv); return NULL; }
        if (asg_is_oserror(kind) || asg_is_sqlite_error(kind) || asg_is_store_failure(kind) ||
            strcmp(kind, "RuntimeError") == 0 || strcmp(kind, "TypeError") == 0 ||
            strcmp(kind, "ValueError") == 0) {
            const char *msg = json_get_str(inv, "message", "");
            json_free(inv); json_free(pair);
            json_t *resp = asg_json_error(msg, "invalid_room_invitation", NULL, 400);
            if (!resp) return NULL;
            return asg_ret(resp);
        }
        json_free(pair);
        asg_park(inv);
        return NULL;
    }
    /* {"object": "hermes.room_member.invitation", **invitation} */
    if (!inv || inv->type != JSON_OBJECT) {
        json_free(inv); json_free(pair);
        asg_park(asg_raise_spec("TypeError", "argument of type 'NoneType' is not a mapping"));
        return NULL;
    }
    json_t *rbody = json_object();
    json_set_hdr(rbody, "object", "hermes.room_member.invitation");
    for (size_t i = 0; i < inv->c.count; i++)
        json_set(rbody, inv->c.keys[i], json_copy(inv->c.items[i]));
    json_free(inv); json_free(pair);
    json_t *wargs = json_object();
    json_set(wargs, "body", rbody);
    json_set(wargs, "status", json_number(201));
    json_t *resp = asg_seam("web_json_response", wargs);
    if (asg_raise_kind(resp)) { asg_park(resp); return NULL; }
    return asg_ret(resp);
}

int check_run_auth(void) { return 1; }

int request_owns_run(void) { return 1; }
