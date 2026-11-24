/*
 * session_group_controls.c -- real C11 port of the pure tables, validators and
 * payload/decision shapers of upstream Hermes
 *   gateway/session_group_controls.py.
 *
 * PoP: w18_sessiongro_0639e1_capability_for_method @ gateway/session_group_controls.py:GROUP_METHODS
 * PoP: w18_sessiongro_0639e1_fields_for_method @ gateway/session_group_controls.py:_FIELDS
 * PoP: w18_sessiongro_0639e1_params_allowed @ gateway/session_group_controls.py:dispatch_group_control
 * PoP: w18_sessiongro_0639e1_refusal @ gateway/session_group_controls.py:_refusal
 * PoP: w18_sessiongro_0639e1_ui_meta_revisions @ gateway/session_group_controls.py:_ui_meta_revisions
 * PoP: w18_sessiongro_0639e1_stat_key @ gateway/session_group_controls.py:_stat_key
 * PoP: w18_sessiongro_0639e1_has_avatar @ gateway/session_group_controls.py:_has_avatar
 * PoP: w18_sessiongro_0639e1_execution_control @ gateway/session_group_controls.py:_execution_control
 *
 * The rest of the module (dispatch_group_control, _group, _disband,
 * _revoke_routes_first, _store, _roster_siblings, _bot_sessions,
 * _read_bot_sessions, _sibling_sessions, _profiles, _named_profile_dirs,
 * _discovered_profile_meta, _lstat_key) is bound to a live authority
 * connection, an open store handle, a running hosted-room service or the
 * filesystem inode clock, so those stay upstream.
 */
#include "hermes_json.h"
#include "noc_w18_sessiongro_0639e1.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *sg0639_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int sg0639_truthy(const json_t *v) {
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

/* `type(x) is int`: a JSON bool is not an int, and only integral numbers are. */
static int sg0639_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* --- GROUP_METHODS: method -> capability (default 'session:read') ---------- */

typedef struct { const char *method; const char *capability; } sg0639_cap_row;

static const sg0639_cap_row CAPABILITY_TABLE[] = {
    {"groups.capabilities",        "session:read"},
    {"groups.list",                "session:read"},
    {"groups.state",               "session:read"},
    {"groups.log",                 "session:read"},
    {"groups.create",              "session:control"},
    {"groups.rename",              "session:control"},
    {"groups.context",             "session:control"},
    {"groups.disband",             "session:control"},
    {"groups.send",                "session:submit"},
    {"groups.attachment.upload",   "session:submit"},
    {"groups.attachment.download", "session:read"},
    {"groups.stop",                "session:control"},
    {"groups.continue",            "session:control"},
    {"groups.retry",               "session:control"},
    {"groups.discard",             "session:control"},
    {"groups.approve",             "session:approve"},
    {"groups.peer.register",       "session:control"},
    {"groups.peer.invite",         "session:operator"},
    {"groups.peer.revoke",         "session:operator"},
    {NULL, NULL}
};

const char *w18_sessiongro_0639e1_capability_for_method(const char *method) {
    int i;
    if (method) {
        for (i = 0; CAPABILITY_TABLE[i].method; i++) {
            if (!strcmp(method, CAPABILITY_TABLE[i].method)) return CAPABILITY_TABLE[i].capability;
        }
    }
    return "session:read";
}

/* --- _FIELDS: method -> accepted parameter names -------------------------- */

static const char *const F_CAPABILITIES[] = {NULL};
static const char *const F_LIST[] = {"limit", "offset", "include_disbanded", NULL};
static const char *const F_STATE[] = {"room_id", "include_disbanded", NULL};
static const char *const F_LOG[] = {"room_id", "since_seq", "limit", "include_disbanded", NULL};
static const char *const F_CREATE[] = {"room_id", "name", "members", NULL};
static const char *const F_RENAME[] = {"room_id", "event_id", "name", NULL};
static const char *const F_CONTEXT[] = {"room_id", "event_id", "purpose", "notes", "routing",
                                        "lead_member_id", NULL};
static const char *const F_DISBAND[] = {"room_id", "cancel_id", NULL};
static const char *const F_SEND[] = {"room_id", "event_id", "payload", NULL};
static const char *const F_UPLOAD[] = {"room_id", "upload_id", "kind", "name", "mime",
                                       "data_base64", NULL};
static const char *const F_DOWNLOAD[] = {"room_id", "event_id", "attachment_id", NULL};
static const char *const F_STOP[] = {"room_id", "cancel_id", NULL};
static const char *const F_CONTINUE[] = {"room_id", "discussion_event_id", NULL};
static const char *const F_RETRY[] = {"room_id", "member_id", "task_id", "execution_generation",
                                      NULL};
static const char *const F_APPROVE[] = {"room_id", "member_id", "task_id", "execution_generation",
                                        "choice", "request_id", NULL};
static const char *const F_PEER_REGISTER[] = {"room_id", "member_id", "target_url",
                                              "target_profile", "grant", "catalog", NULL};
static const char *const F_PEER_INVITE[] = {"room_id", "home_install_id", "authority_gateway_id",
                                            "authority_epoch", "member_id", "ttl_seconds",
                                            "status_ttl_seconds", NULL};
static const char *const F_PEER_REVOKE[] = {"grant", NULL};
static const char *const F_PROFILES[] = {"include_sessions", NULL};

typedef struct { const char *method; const char *const *fields; } sg0639_field_row;

static const sg0639_field_row FIELD_TABLE[] = {
    {"groups.capabilities",        F_CAPABILITIES},
    {"groups.list",                F_LIST},
    {"groups.state",               F_STATE},
    {"groups.log",                 F_LOG},
    {"groups.create",              F_CREATE},
    {"groups.rename",              F_RENAME},
    {"groups.context",             F_CONTEXT},
    {"groups.disband",             F_DISBAND},
    {"groups.send",                F_SEND},
    {"groups.attachment.upload",   F_UPLOAD},
    {"groups.attachment.download", F_DOWNLOAD},
    {"groups.stop",                F_STOP},
    {"groups.continue",            F_CONTINUE},
    {"groups.retry",               F_RETRY},
    {"groups.discard",             F_RETRY},
    {"groups.approve",             F_APPROVE},
    {"groups.peer.register",       F_PEER_REGISTER},
    {"groups.peer.invite",         F_PEER_INVITE},
    {"groups.peer.revoke",         F_PEER_REVOKE},
    {"profiles.list",              F_PROFILES},
    {NULL, NULL}
};

/* NULL for an unknown method; a (possibly empty) list for a known one. */
static const char *const *sg0639_fields(const char *method) {
    int i;
    if (!method) return NULL;
    for (i = 0; FIELD_TABLE[i].method; i++) {
        if (!strcmp(method, FIELD_TABLE[i].method)) return FIELD_TABLE[i].fields;
    }
    return NULL;
}

static int sg0639_in_fields(const char *key, const char *const *fields) {
    int i;
    for (i = 0; fields[i]; i++) {
        if (!strcmp(key, fields[i])) return 1;
    }
    return 0;
}

static int sg0639_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

json_t *w18_sessiongro_0639e1_fields_for_method(const char *method) {
    const char *const *fields = sg0639_fields(method);
    const char *sorted[8];
    json_t *arr;
    int n = 0, i;
    if (!fields) return json_null();
    for (i = 0; fields[i]; i++) {
        if (n < (int)(sizeof(sorted) / sizeof(sorted[0]))) sorted[n++] = fields[i];
    }
    qsort(sorted, (size_t)n, sizeof(sorted[0]), sg0639_cmp_key);
    arr = json_array();
    for (i = 0; i < n; i++) json_append(arr, json_string(sorted[i]));
    return arr;
}

int w18_sessiongro_0639e1_params_allowed(const char *method, const json_t *params) {
    const char *const *fields = sg0639_fields(method);
    size_t i;
    if (!fields) return 0;                                 /* method not in _FIELDS */
    if (!params || params->type != JSON_OBJECT) return 0;  /* not a dict */
    for (i = 0; i < params->c.count; i++) {
        const char *key = params->c.keys[i];
        if (!key) continue;
        if (!strcmp(key, "profile")) continue;             /* _FIELDS[method] | {'profile'} */
        if (!sg0639_in_fields(key, fields)) return 0;
    }
    return 1;
}

json_t *w18_sessiongro_0639e1_refusal(const char *reason, const json_t *admitted) {
    json_t *out = json_object();
    json_set(out, "reason", json_string(reason ? reason : ""));
    /* `admitted is False` -- only a genuine boolean False rides along. */
    if (admitted && admitted->type == JSON_BOOL && !admitted->bool_val) {
        json_set(out, "admitted", json_bool(0));
    }
    return out;
}

json_t *w18_sessiongro_0639e1_ui_meta_revisions(const json_t *meta) {
    json_t *revisions, *out = json_object();
    size_t i;
    if (!meta || meta->type != JSON_OBJECT) return out;
    revisions = json_obj_get(meta, "_ui_meta_revisions");
    if (!revisions || revisions->type != JSON_OBJECT) return out;
    for (i = 0; i < revisions->c.count; i++) {
        const char *key = revisions->c.keys[i];
        json_t *v = revisions->c.items[i];
        double d;
        if (!key || !sg0639_is_py_int(v)) continue;        /* type(v) is int */
        d = v->num_val;
        if (d < 0.0) d = 0.0;                              /* max(0, v) */
        json_set(out, key, json_number(d));
    }
    return out;
}

json_t *w18_sessiongro_0639e1_stat_key(const char *mtime_ns, const char *ctime_ns,
                                       const char *size, const char *ino) {
    const char *in[4];
    json_t *arr = json_array();
    int i;
    char buf[32];
    in[0] = mtime_ns;
    in[1] = ctime_ns;
    in[2] = size;
    in[3] = ino;
    for (i = 0; i < 4; i++) {
        long long v = in[i] ? strtoll(in[i], NULL, 10) : 0;
        snprintf(buf, sizeof(buf), "%lld", v);
        json_append(arr, json_string(buf));
    }
    return arr;
}

int w18_sessiongro_0639e1_has_avatar(const char *home) {
    static const char *const EXTS[] = {"png", "jpg", "webp", NULL};
    char path[4096];
    int i;
    if (!home) return 0;
    for (i = 0; EXTS[i]; i++) {
        struct stat st;
        int n = snprintf(path, sizeof(path), "%s/assets/avatar.%s", home, EXTS[i]);
        if (n <= 0 || n >= (int)sizeof(path)) continue;
        if (stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG) return 1;
    }
    return 0;
}

/* --- _execution_control --------------------------------------------------- */

static json_t *sg0639_wrap_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *sg0639_wrap_refused(const char *reason) {
    json_t *out = json_object();
    json_set(out, "status", json_string("refused"));
    json_set(out, "reason", json_string(reason ? reason : ""));
    json_set(out, "admitted", json_null());
    return out;
}

static json_t *sg0639_wrap_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "status", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

/* str(value or '') for the wire scalar shapes this field can carry. */
static char *sg0639_str_or_empty(const json_t *v) {
    char buf[64];
    if (!sg0639_truthy(v)) return sg0639_dup("");
    switch (v->type) {
    case JSON_STRING: return sg0639_dup(v->str_val);
    case JSON_BOOL:   return sg0639_dup("True");
    case JSON_NUMBER:
        if (sg0639_is_py_int(v)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return sg0639_dup(buf);
    default:
        return sg0639_dup("");
    }
}

static json_t *sg0639_service_error(const json_t *service) {
    json_t *err = json_obj_get(service, "error");
    const char *type, *message;
    if (!err || err->type != JSON_OBJECT) return NULL;
    type = json_get_str(err, "type", "RuntimeError");
    message = json_get_str(err, "message", "");
    return sg0639_wrap_raised(type, message);
}

static json_t *sg0639_attempt_control(const char *method, const json_t *params,
                                      const json_t *service) {
    static const char *const IDENTITY_FIELDS[] = {"room_id", "task_id", "thread_id", "turn_id"};
    static const char *const TASK_FIELDS[] = {"status", "execution_generation",
                                              "cancel_generation"};
    json_t *gen, *task, *identity, *receipt, *value, *raised;
    int i;

    gen = json_obj_get(params, "execution_generation");
    if (!sg0639_is_py_int(gen) || gen->num_val < 1.0) return sg0639_wrap_refused("invalid_params");
    if (!sg0639_truthy(json_obj_get(params, "member_id"))) return sg0639_wrap_refused("invalid_params");
    if (!sg0639_truthy(json_obj_get(params, "task_id"))) return sg0639_wrap_refused("invalid_params");

    raised = sg0639_service_error(service);
    if (raised) return raised;

    task = json_obj_get(service, "task");
    identity = json_obj_get(task, "identity");
    receipt = json_object();
    for (i = 0; i < 4; i++) {
        json_t *v = json_obj_get(identity, IDENTITY_FIELDS[i]);
        json_set(receipt, IDENTITY_FIELDS[i], v ? json_copy(v) : json_null());
    }
    for (i = 0; i < 3; i++) {
        json_t *v = json_obj_get(task, TASK_FIELDS[i]);
        json_set(receipt, TASK_FIELDS[i], v ? json_copy(v) : json_null());
    }
    value = json_object();
    json_set(value, !strcmp(method, "groups.discard") ? "discarded" : "retried", json_bool(1));
    json_set(value, "task", receipt);
    return sg0639_wrap_ok(value);
}

static json_t *sg0639_approve(const json_t *params, const json_t *service) {
    json_t *gen, *choice, *request_id, *value, *raised;
    const char *request_text;

    gen = json_obj_get(params, "execution_generation");
    if (!sg0639_is_py_int(gen) || gen->num_val < 1.0) return sg0639_wrap_refused("invalid_params");
    choice = json_obj_get(params, "choice");
    if (!choice || choice->type != JSON_STRING ||
        (strcmp(choice->str_val, "once") && strcmp(choice->str_val, "deny"))) {
        return sg0639_wrap_refused("invalid_params");
    }
    request_id = json_obj_get(params, "request_id");
    if (!request_id || request_id->type != JSON_STRING) return sg0639_wrap_refused("invalid_params");
    request_text = request_id->str_val;
    if (!request_text || !request_text[0]) return sg0639_wrap_refused("invalid_params");

    raised = sg0639_service_error(service);
    if (raised) {
        /* `except RuntimeError` with this exact text re-raises AuthorityRefusal(str(exc)). */
        json_t *err = json_obj_get(service, "error");
        const char *type = json_get_str(err, "type", "RuntimeError");
        const char *message = json_get_str(err, "message", "");
        if (!strcmp(type, "RuntimeError") && !strcmp(message, "room approval is no longer pending")) {
            json_free(raised);
            return sg0639_wrap_refused(message);
        }
        return raised;
    }
    value = json_object();
    json_set(value, "approved", json_bool(1));
    {
        json_t *result = json_obj_get(service, "result");
        json_set(value, "result", result ? json_copy(result) : json_null());
    }
    return sg0639_wrap_ok(value);
}

static json_t *sg0639_continue(const json_t *params, const json_t *service) {
    char *did = sg0639_str_or_empty(json_obj_get(params, "discussion_event_id"));
    json_t *raised = sg0639_service_error(service);
    json_t *value, *event;
    if (raised) {
        free(did);
        return raised;
    }
    event = json_obj_get(service, "event");
    value = json_object();
    json_set(value, "continued", json_bool(1));
    json_set(value, "event", event ? json_copy(event) : json_null());
    /* `str(params.get('discussion_event_id') or '')` is what the service is
     * handed; the Python return carries only the event, so nothing else is set. */
    free(did);
    return sg0639_wrap_ok(value);
}

json_t *w18_sessiongro_0639e1_execution_control(const char *method, const json_t *params,
                                                const json_t *service) {
    if (!method) return sg0639_wrap_raised("KeyError", "");
    if (!params || params->type != JSON_OBJECT) params = NULL;
    if (!service || service->type != JSON_OBJECT) service = NULL;

    if (!strcmp(method, "groups.retry") || !strcmp(method, "groups.discard")) {
        return sg0639_attempt_control(method, params, service);
    }
    if (!strcmp(method, "groups.approve")) {
        return sg0639_approve(params, service);
    }
    if (!strcmp(method, "groups.continue")) {
        return sg0639_continue(params, service);
    }
    /* groups.send needs rooms.user_event_id(); groups.stop mints uuid4 when no
     * cancel_id is supplied. Both stay upstream (handler not ported). */
    {
        json_t *out = json_object();
        json_set(out, "status", json_string("unsupported"));
        json_set(out, "method", json_string(method));
        return out;
    }
}
