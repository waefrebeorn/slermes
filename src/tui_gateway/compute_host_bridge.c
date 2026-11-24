/*
 * compute_host_bridge.c — faithful C11 port of the value core of
 * tui_gateway/compute_host_bridge.py.  Gate w34_computehos_e233d0.
 *
 * Ported: the turn-isolation predicates, the registry-lease vouch that rides
 * with a turn.start frame, the turn.start payload itself, the session_key /
 * history_version adoption a host frame causes, the mirrored-metadata and
 * open-request predicates, the control frame's defaults, the compress RPC wait
 * budget and the ack->error envelope.
 *
 * Seams (stay in the caller, never here): the HostSupervisor child process and
 * its sockets, write_json / _emit fan-out, the _sessions table and history
 * locks, config.yaml reads (_load_cfg / _load_dashboard_process_isolation_config),
 * cwd + platform-source + transport-auth resolution (session_workdir /
 * session_transports), the registry lease transfer, and the vault-redaction
 * registry behind _scrub_preview_vault_reply.
 *
 * PoP: w34_computehos_e233d0_turn_isolation_enabled @ tui_gateway/compute_host_bridge.py:_turn_isolation_enabled
 * PoP: w34_computehos_e233d0_session_uses_compute_host @ tui_gateway/compute_host_bridge.py:_session_uses_compute_host
 * PoP: w34_computehos_e233d0_active_session_lease_vouch @ tui_gateway/compute_host_bridge.py:_active_session_lease_vouch
 * PoP: w34_computehos_e233d0_metadata_mirror @ tui_gateway/compute_host_bridge.py:_metadata_mirror
 * PoP: w34_computehos_e233d0_open_request_matches @ tui_gateway/compute_host_bridge.py:_open_request_matches
 * PoP: w34_computehos_e233d0_adopt_frame_meta @ tui_gateway/compute_host_bridge.py:_compute_host_adopt_frame_meta
 * PoP: w34_computehos_e233d0_control_frame @ tui_gateway/compute_host_bridge.py:_send_compute_host_control
 * PoP: w34_computehos_e233d0_compress_wait_seconds @ tui_gateway/compute_host_bridge.py:_compute_host_compress_wait_seconds
 * PoP: w34_computehos_e233d0_ack_error @ tui_gateway/compute_host_bridge.py:_compute_host_ack_error
 * PoP: w34_computehos_e233d0_turn_frame @ tui_gateway/compute_host_bridge.py:_compute_host_turn_frame
 */
#include "hermes_json.h"
#include "noc_w34_computehos_e233d0.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Python value semantics ------------------------------------------------ */

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *dupn(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n && s) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* str(value) for the scalar shapes a session record carries. */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v) return dupn("None", 4);
    switch (v->type) {
        case JSON_NULL:   return dupn("None", 4);
        case JSON_STRING: return dupn(v->str_val ? v->str_val : "", v->str_val ? strlen(v->str_val) : 0);
        case JSON_BOOL:   return dupn(v->bool_val ? "True" : "False", v->bool_val ? 4 : 5);
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupn(buf, strlen(buf));
        default: return dupn("", 0);
    }
}

/* str(value or "") — the `x.get(k) or ""` shape. */
static char *py_str_or_empty(const json_t *v) {
    return py_truthy(v) ? py_str(v) : dupn("", 0);
}

static char *py_strip(const char *s) {
    size_t n = s ? strlen(s) : 0, a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\v'||s[a]=='\f')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\v'||s[b-1]=='\f')) b--;
    return dupn(s ? s + a : "", s ? b - a : 0);
}

/* float(value) with ValueError/TypeError folded to `def` (the live code wraps
 * these in contextlib.suppress). */
static double py_float(const json_t *v, double def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) return def;
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        return (end && *end == '\0') ? d : def;
    }
    return def;
}

/* int(value) with ValueError/TypeError folded to `def`. */
static double py_int(const json_t *v, double def) {
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_NUMBER) return (double)(long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        double d = strtod(s, &end);
        if (end == s) return def;
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        if (!end || *end != '\0' || d != (double)(long long)d) return def;
        return (double)(long long)d;
    }
    return def;
}

static int py_eq(const json_t *a, const json_t *b);

static int py_eq_obj(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_OBJECT || b->type != JSON_OBJECT) return 0;
    if (a->c.count != b->c.count) return 0;
    for (size_t i = 0; i < a->c.count; i++) {
        const json_t *bv = json_obj_get(b, a->c.keys[i]);
        if (!bv) return 0;
        if (!py_eq(a->c.items[i], bv)) return 0;
    }
    return 1;
}

/* Python == over JSON scalars and containers (absent reads as None). */
static int py_eq(const json_t *a, const json_t *b) {
    int an = (!a || a->type == JSON_NULL), bn = (!b || b->type == JSON_NULL);
    if (an || bn) return an && bn;
    int a_num = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    int b_num = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (a_num || b_num) {
        if (!(a_num && b_num)) return 0;
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING || b->type == JSON_STRING) {
        if (!(a->type == JSON_STRING && b->type == JSON_STRING)) return 0;
        return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    }
    if (a->type == JSON_ARRAY || b->type == JSON_ARRAY) {
        if (!(a->type == JSON_ARRAY && b->type == JSON_ARRAY)) return 0;
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!py_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    return py_eq_obj(a, b);
}

/* Copy of a session value, or a JSON null node when the key is absent. */
static json_t *jget_or_null(const json_t *obj, const char *key) {
    const json_t *v = obj ? json_obj_get(obj, key) : NULL;
    return v ? json_copy(v) : json_null();
}

static json_t *jarray_or_empty(const json_t *v) {
    return (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array();
}

/* --- ported symbols ------------------------------------------------------- */

int w34_computehos_e233d0_turn_isolation_enabled(const json_t *cfg, int child_env_is_1) {
    /* HERMES_COMPUTE_HOST_CHILD == "1" is the env seam: the child never isolates. */
    if (child_env_is_1) return 0;
    /* A falsy cfg means a config.yaml read (seam): no config -> not isolated. */
    if (!py_truthy(cfg) || cfg->type != JSON_OBJECT) return 0;
    return py_truthy(json_obj_get(cfg, "turn_isolation"));
}

int w34_computehos_e233d0_session_uses_compute_host(const json_t *session, const json_t *cfg,
                                                   int child_env_is_1) {
    if (!w34_computehos_e233d0_turn_isolation_enabled(cfg, child_env_is_1)) return 0;
    if (!session || session->type != JSON_OBJECT) return 0;
    if (py_truthy(json_obj_get(session, "_compute_host_active"))) return 1;
    const json_t *agent = json_obj_get(session, "agent");
    const json_t *ready = json_obj_get(session, "agent_ready");
    /* Lazy session: AIAgent never built in-process, but marked ready. */
    return (agent == NULL || agent->type == JSON_NULL) && ready != NULL && ready->type != JSON_NULL;
}

json_t *w34_computehos_e233d0_active_session_lease_vouch(const json_t *session) {
    const json_t *lease = (session && session->type == JSON_OBJECT)
                              ? json_obj_get(session, "active_session_lease") : NULL;
    if (!lease || lease->type == JSON_NULL) return NULL;
    if (lease->type != JSON_OBJECT) return NULL;
    if (py_truthy(json_obj_get(lease, "released"))) return NULL;
    if (!py_truthy(json_obj_get(lease, "enabled"))) return NULL;
    const json_t *lsid = json_obj_get(lease, "session_id");
    if (!lsid) return NULL;  /* getattr(lease, "session_id") would raise */
    char *lsid_s = py_str(lsid);
    char *skey_s = py_str_or_empty(json_obj_get(session, "session_key"));
    if (!lsid_s || !skey_s || strcmp(lsid_s, skey_s) != 0) {  /* qualified, not a bare bool */
        free(lsid_s); free(skey_s);
        return NULL;
    }
    json_t *out = json_object();
    free(skey_s);
    if (!out) { free(lsid_s); return NULL; }
    char *lid_s = py_str(json_obj_get(lease, "lease_id"));
    json_set(out, "lease_id", json_string(lid_s ? lid_s : ""));
    json_set(out, "session_id", json_string(lsid_s));
    free(lid_s); free(lsid_s);
    return out;
}

json_t *w34_computehos_e233d0_metadata_mirror(const json_t *session) {
    const json_t *mirror = (session && session->type == JSON_OBJECT)
                               ? json_obj_get(session, "_metadata_mirror") : NULL;
    if (mirror && mirror->type == JSON_OBJECT) return json_copy(mirror);
    return json_object();
}

int w34_computehos_e233d0_open_request_matches(const json_t *session, const json_t *request_id) {
    const json_t *mirrored = (session && session->type == JSON_OBJECT)
                                 ? json_obj_get(session, "_compute_host_open_request") : NULL;
    if (!mirrored || mirrored->type != JSON_OBJECT) return 0;
    return py_eq(json_obj_get(mirrored, "id"), request_id);
}

json_t *w34_computehos_e233d0_adopt_frame_meta(const json_t *session, const json_t *frame) {
    char *cur = py_str_or_empty(session && session->type == JSON_OBJECT
                                    ? json_obj_get(session, "session_key") : NULL);
    char *new_key = py_str_or_empty(frame && frame->type == JSON_OBJECT
                                        ? json_obj_get(frame, "session_key") : NULL);
    int rotated = 0;
    const char *final_key = cur ? cur : "";
    /* A rotated key means the child compressed A->B; the REAL lease re-anchors
     * in the registry (seam), never claimed from the child pid. */
    if (new_key && new_key[0] && (!cur || strcmp(new_key, cur) != 0)) {
        rotated = 1;
        final_key = new_key;
    }
    const json_t *hv = (frame && frame->type == JSON_OBJECT)
                           ? json_obj_get(frame, "history_version") : NULL;
    json_t *hv_out;
    if (hv && hv->type != JSON_NULL) {
        double cur_v = py_int(session && session->type == JSON_OBJECT
                                  ? json_obj_get(session, "history_version") : NULL, 0.0);
        double frame_v = py_int(py_truthy(hv) ? hv : NULL, 0.0);  /* int(frame[hv] or 0) */
        hv_out = json_number(cur_v > frame_v ? cur_v : frame_v);
    } else {
        hv_out = jget_or_null(session && session->type == JSON_OBJECT ? session : NULL,
                              "history_version");
    }
    json_t *out = json_object();
    if (out) {
        json_set(out, "session_key", json_string(final_key));
        json_set(out, "history_version", hv_out);
        json_set(out, "lease_rotated", json_bool(rotated));
    } else {
        json_free(hv_out);
    }
    free(cur); free(new_key);
    return out;
}

json_t *w34_computehos_e233d0_control_frame(const json_t *payload, const json_t *command) {
    json_t *out = json_object();
    if (!out) return NULL;
    /* dict(payload or {}) */
    if (payload && payload->type == JSON_OBJECT) {
        for (size_t i = 0; i < payload->c.count; i++)
            json_set(out, payload->c.keys[i], json_copy(payload->c.items[i]));
    }
    /* setdefault: an explicit null keeps its slot. */
    if (!json_obj_get(out, "type")) json_set(out, "type", json_string("control"));
    if (!json_obj_get(out, "command")) json_set(out, "command", command ? json_copy(command) : json_string(""));
    return out;
}

double w34_computehos_e233d0_compress_wait_seconds(const json_t *cfg) {
    /* agent.conversation_compression.resolve_context_compression_timeouts:
     * (idle, ceiling) with defaults 120/600, explicit overrides, and the ceiling
     * clamped up to the idle window when idle > 0.  The auxiliary-compression
     * timeout floor is a config seam (see header); fixtures keep the ceiling at
     * or above it. */
    double idle = 120.0, ceiling = 600.0;
    const json_t *comp = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "compression") : NULL;
    if (comp && comp->type == JSON_OBJECT) {
        const json_t *ct = json_obj_get(comp, "context_timeout_seconds");
        if (ct && ct->type != JSON_NULL) idle = py_float(ct, idle);
        const json_t *cc = json_obj_get(comp, "context_total_ceiling_seconds");
        if (cc && cc->type != JSON_NULL) {
            double v = py_float(cc, ceiling);
            if (v > 0) ceiling = v;
        }
    }
    if (idle > 0 && idle > ceiling) ceiling = idle;
    double wait = ceiling + 30.0;
    if (wait < 120.0) wait = 120.0;
    if (wait > W34_COMPUTEHOS_E233D0_COMPRESS_WAIT_CAP_SECS) wait = W34_COMPUTEHOS_E233D0_COMPRESS_WAIT_CAP_SECS;
    return wait;
}

json_t *w34_computehos_e233d0_ack_error(const json_t *rid, const json_t *ack, int code,
                                        const char *fallback) {
    if (!ack || ack->type != JSON_OBJECT) return NULL;
    const char *type = json_get_str(ack, "type", "");
    if (strcmp(type, "control.error") != 0 && strcmp(type, "error") != 0) return NULL;
    const json_t *msg = json_obj_get(ack, "message");
    char *text = py_truthy(msg) ? py_str(msg) : dupn(fallback ? fallback : "", fallback ? strlen(fallback) : 0);
    json_t *error = json_object();
    json_t *out = json_object();
    if (!error || !out) { json_free(error); json_free(out); free(text); return NULL; }
    json_set(error, "code", json_number((double)code));
    json_set(error, "message", json_string(text ? text : ""));
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "id", rid ? json_copy(rid) : json_null());
    json_set(out, "error", error);
    free(text);
    return out;
}

json_t *w34_computehos_e233d0_turn_frame(const json_t *rid, const json_t *sid,
                                        const json_t *session, const json_t *text,
                                        const json_t *image_paths,
                                        const json_t *queued_prompt_generation,
                                        const json_t *display_kind,
                                        const json_t *display_metadata) {
    if (!session || session->type != JSON_OBJECT) return NULL;  /* session["history_lock"] */
    json_t *out = json_object();
    if (!out) return NULL;

    json_set(out, "type", json_string("turn.start"));
    json_set(out, "sid", sid ? json_copy(sid) : json_null());
    json_set(out, "request_id", rid ? json_copy(rid) : json_null());

    const json_t *skey = json_obj_get(session, "session_key");
    json_set(out, "session_key", py_truthy(skey) ? json_copy(skey)
                                                  : (sid ? json_copy(sid) : json_null()));
    json_set(out, "text", text ? json_copy(text) : json_null());
    if (py_truthy(display_kind)) json_set(out, "display_kind", json_copy(display_kind));
    json_set(out, "history", jarray_or_empty(json_obj_get(session, "history")));
    if (py_truthy(display_metadata)) json_set(out, "display_metadata", json_copy(display_metadata));

    const json_t *hv = json_obj_get(session, "history_version");
    json_set(out, "history_version", json_number(py_int(hv, 0.0)));
    const json_t *cols = json_obj_get(session, "cols");
    json_set(out, "cols", json_number(py_int(py_truthy(cols) ? cols : NULL, 80.0)));

    /* Workspace provenance rides with the cwd.  cwd/source resolution is the
     * caller's (session_workdir): the record always carries both on this path. */
    const json_t *cwd = json_obj_get(session, "cwd");
    char *cwd_s = py_truthy(cwd) ? py_str(cwd) : dupn("", 0);
    json_set(out, "cwd", json_string(cwd_s ? cwd_s : ""));
    free(cwd_s);

    int explicit_cwd = py_truthy(json_obj_get(session, "explicit_cwd"));
    json_set(out, "explicit_cwd", json_bool(explicit_cwd));
    char *source = py_strip(py_str_or_empty(json_obj_get(session, "source")));
    json_set(out, "context_cwd_is_launch_artifact", json_bool(!explicit_cwd && source && !strcmp(source, "desktop")));
    json_set(out, "source", json_string(source ? source : ""));
    free(source);

    const json_t *home = json_obj_get(session, "profile_home");
    json_set(out, "profile_home", py_truthy(home) ? json_copy(home) : json_string(""));
    json_set(out, "model_override", jget_or_null(session, "model_override"));
    /* A model switch queued while busy is COPIED across the boundary, not popped. */
    json_set(out, "pending_model_switch", jget_or_null(session, "pending_model_switch"));
    json_set(out, "reasoning_config_override", jget_or_null(session, "create_reasoning_override"));
    json_set(out, "service_tier_override", jget_or_null(session, "create_service_tier_override"));

    const json_t *images = image_paths ? image_paths : json_obj_get(session, "attached_images");
    json_set(out, "attached_images", jarray_or_empty(images));
    const json_t *auth = json_obj_get(session, "auth_user_id");
    json_set(out, "auth_user_id", auth ? json_copy(auth) : json_null());
    json_set(out, "queued_prompt_generation",
             queued_prompt_generation ? json_copy(queued_prompt_generation) : json_null());
    /* Vouch for the registry lease this process already holds (#101416). */
    json_t *vouch = w34_computehos_e233d0_active_session_lease_vouch(session);
    json_set(out, "active_session_lease", vouch ? vouch : json_null());
    return out;
}
