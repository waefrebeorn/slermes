/*
 * session_controls_flow.c — session control / interrupt FLOW cores of
 * gateway/session_controls.py (AuthorityConnection + module refusal helpers).
 *
 * Ports the DECISION sequences: refusal-data projection order, profile label
 * resolution, the storage-refusal dedupe set, refusal log-level branching,
 * storage-failure log truncation, model-only row detection, display-row
 * projection, sibling-authority routing (owner fences + cache re-serve),
 * profile-start holding, ping admission, detach state transitions, event
 * replay cursor admission, voice-stop replay/record/typed gates, the submit
 * admission chain, interrupt routing and the cancel claim/stop ordering.
 * All live I/O enters through one universal seam; state lives in an injected
 * json_t mirror mutated in place.
 *
 * Modeling contract (shared with tests/oracle_session_controls_flow.py):
 *  - State mirror shape (in["state"], copied by the harness before mutation):
 *      storage_refusals:    [[method, label, reason, fieldstr, detailstr]...]
 *                           (_STORAGE_REFUSALS set; sorted on dump)
 *      storage_tracebacks:  [[method, exc_kind]...] (_STORAGE_TRACEBACKS set;
 *                           sorted on dump)
 *      subscriptions:       {session_id: subscription_id}
 *      handed_back:         [session_id...] (set; sorted on dump)
 *      reconnects:          [session_id...] (set; sorted on dump)
 *      replay_cursors:      {session_id: [replay_epoch, last_sequence]}
 *      consumed_voice_stops:[[subject, profile_id, session_id, request_id]...]
 *                           (OrderedDict keys in order; JSON null == the
 *                           authority attribute is ABSENT)
 *      siblings:            {profile_id: {"authority": authority_id}}
 *      authorities:         {aid: {profile_id, instance_id,
 *                           registry: {"members": [aid...]} | null}}
 *                           (input only; never mutated, not in out state)
 *      sessions:            {sid: {"source": {"platform": str} | null}}
 *                           (input only; never mutated, not in out state)
 *  - in["self"]: {authority: aid, actor: {subject, profile_id,
 *    capabilities: [cap...]}, identity: {...}, operator: bool}. host_scoped
 *    is DERIVED: identity.profile_scope == "host".
 *  - in["ref"]: {profile_id, session_id} (SessionRef).
 *  - AuthorityConnection construction is the "new_connection" seam (its
 *    __init__ flow is out of scope); the scripted result is the connection
 *    projection {"authority": aid}. authority.runner.session_authorities
 *    collapses to authority.registry (None when either getattr is None).
 *  - Profile helpers (profile_name_for_home / profile_matches_home) are
 *    seams; Path() args project to the raw home string (clean fixture paths).
 *  - Receipts / handles are scripted as their asdict() field dicts; the
 *    oracle materializes the real dataclasses and asdict()s them back.
 *  - Scripted seam results: NULL == None; {"__raise__": kind, "message": m}
 *    == a raised exception (AuthorityRefusal scripts carry optional
 *    field/detail; message IS the reason). BaseException kinds pass through
 *    every `except AuthorityRefusal` site via env->exc.
 *  - Script sequences: a bare JSON array is ONE RESULT PER CALL (last value
 *    repeats); a list-valued result must be scripted one level deeper.
 *    {"__script__": true, "ret": v} is a constant; absent == None.
 *  - Log seams record raw fmt + args (no printf formatting); the exc arg
 *    records str(exc); exc_info is recorded only when the live call passes it.
 *  - Integer domain: JSON numbers are doubles; isinstance(x, int) sites read
 *    integral-valued numbers as int (fixtures never script fractional ints).
 *    `is True` / `is False` gates are JSON_BOOL-only.
 *  - repr() domain: ASCII printable + \n \r \t + printable non-ASCII kept
 *    verbatim; other control chars escape \xNN (no astral/non-printable
 *    Unicode in fixtures).
 *  - Self-contained: the harness #includes this .c; every symbol is static.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "gateway_lifecycle_flow.h"

/* ── seam environment ─────────────────────────────────────────────────────── */
typedef struct gscf_env {
    json_t *state;   /* borrowed mirror (harness-owned copy of in["state"]) */
    json_t *(*call)(void *ud, const char *name, const json_t *args);
    void *ud;
    json_t *exc;     /* owned unhandled raise descriptor; NULL when none */
} gscf_env;

/* Universal seam call: args BORROWED by the stub (it copies into the trace),
 * result NEW (caller frees). NULL result == Python None. */
static json_t *gscf_call(gscf_env *e, const char *name, json_t *args) {
    json_t *r = e->call(e->ud, name, args);
    json_free(args);
    return r;
}

static bool gscf_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && gflow_is_str(json_obj_get(r, "__raise__"));
}
static const char *gscf_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Normalize a scripted raise spec to the oracle's descriptor shape:
 * {__raise__, message} plus field/detail (null when absent) for refusals.
 * Consumes r. */
static json_t *gscf_norm_raise(json_t *r) {
    const char *kind = gscf_kind(r);
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    json_set(d, "message", json_string(json_get_str(r, "message", "")));
    if (strcmp(kind, "AuthorityRefusal") == 0) {
        const json_t *f = json_obj_get(r, "field");
        json_set(d, "field", f ? json_copy(f) : json_null());
        const json_t *dt = json_obj_get(r, "detail");
        json_set(d, "detail", dt ? json_copy(dt) : json_null());
    }
    json_free(r);
    return d;
}
/* Propagate a seam raise (consumes r when it IS a raise). Returns true when
 * the caller must bail out; false leaves r untouched for the caller to free. */
static bool gscf_prop(gscf_env *e, json_t *r) {
    if (!gscf_is_raise(r)) return false;
    e->exc = gscf_norm_raise(r);
    return true;
}
static void gscf_raise(gscf_env *e, const char *kind, const char *msg) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string(kind));
    json_set(d, "message", json_string(msg ? msg : ""));
    e->exc = d;
}
static void gscf_raise_ar(gscf_env *e, const char *reason, const char *field,
                          const char *detail) {
    json_t *d = json_object();
    json_set(d, "__raise__", json_string("AuthorityRefusal"));
    json_set(d, "message", json_string(reason));
    json_set(d, "field", field ? json_string(field) : json_null());
    json_set(d, "detail", detail ? json_string(detail) : json_null());
    e->exc = d;
}
static bool gscf_is_ar(const json_t *r) {
    return gscf_is_raise(r) && strcmp(gscf_kind(r), "AuthorityRefusal") == 0;
}

/* ── JSON equality / membership (sets modelled as arrays) ────────────────── */
static bool gscf_json_eq(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL:   return true;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                        b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!gscf_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                const json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !gscf_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
    }
    return false;
}
static bool gscf_arr_has(const json_t *arr, const json_t *v) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (gscf_json_eq(arr->c.items[i], v)) return true;
    return false;
}
static bool gscf_strset_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val
                && strcmp(it->str_val, s) == 0) return true;
    }
    return false;
}
static void gscf_strset_add(json_t *arr, const char *s) {
    if (!arr || !s || gscf_strset_has(arr, s)) return;
    json_append(arr, json_string(s));
}
static void gscf_strset_discard(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val
                && strcmp(it->str_val, s) == 0) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++)
                arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            return;
        }
    }
}
/* Ordered-map key move_to_end: remove the equal entry, append a fresh copy. */
static void gscf_key_move_end(json_t *arr, const json_t *key) {
    for (size_t i = 0; i < arr->c.count; i++) {
        if (gscf_json_eq(arr->c.items[i], key)) {
            json_free(arr->c.items[i]);
            for (size_t j = i; j + 1 < arr->c.count; j++)
                arr->c.items[j] = arr->c.items[j + 1];
            arr->c.count--;
            break;
        }
    }
    json_append(arr, json_copy(key));
}
static void gscf_arr_pop0(json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY || arr->c.count == 0) return;
    json_free(arr->c.items[0]);
    for (size_t j = 0; j + 1 < arr->c.count; j++)
        arr->c.items[j] = arr->c.items[j + 1];
    arr->c.count--;
}

/* ── state mirror access (borrowed pointers) ─────────────────────────────── */
static json_t *gscf_sect(gscf_env *e, const char *name) {
    return json_obj_get(e->state, name);
}
static json_t *gscf_authority(gscf_env *e, const json_t *selfn) {
    const char *aid = json_get_str(selfn, "authority", "");
    return json_obj_get(gscf_sect(e, "authorities"), aid);
}
static bool gscf_host_scoped(const json_t *selfn) {
    const json_t *id = json_obj_get(selfn, "identity");
    const char *scope = id ? json_get_str(id, "profile_scope", "") : "";
    return strcmp(scope, "host") == 0;
}
static const char *gscf_sid(const json_t *in) {
    return json_get_str(json_obj_get(in, "ref"), "session_id", "");
}
static const char *gscf_subject(const json_t *selfn) {
    return json_get_str(json_obj_get(selfn, "actor"), "subject", "");
}
static bool gscf_subscribed(gscf_env *e, const char *sid) {
    return json_has(gscf_sect(e, "subscriptions"), sid);
}

/* ── UTF-8 helpers (codepoint length / prefix slice) ─────────────────────── */
static size_t gscf_cp_adv(const char *s) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}
static size_t gscf_ulen(const char *s) {
    size_t n = 0;
    while (s && *s) { s += gscf_cp_adv(s); n++; }
    return n;
}
static char *gscf_uslice(const char *s, size_t n) {
    size_t i = 0, k = 0;
    if (!s) s = "";
    while (s[i] && k < n) { i += gscf_cp_adv(s + i); k++; }
    char *r = (char *)malloc(i + 1);
    if (!r) return strdup("");
    memcpy(r, s, i);
    r[i] = '\0';
    return r;
}

/* ── Python repr() for the fixture domain ────────────────────────────────── */
static char *gscf_pyrepr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type != JSON_STRING) return gflow_pystr(v);
    const char *s = v->str_val ? v->str_val : "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *r = (char *)malloc(cap);
    if (!r) return strdup("");
    size_t n = 0;
    r[n++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') { r[n++] = '\\'; r[n++] = '\\'; }
        else if (c == (unsigned char)q) { r[n++] = '\\'; r[n++] = (char)c; }
        else if (c == '\n') { r[n++] = '\\'; r[n++] = 'n'; }
        else if (c == '\r') { r[n++] = '\\'; r[n++] = 'r'; }
        else if (c == '\t') { r[n++] = '\\'; r[n++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            n += (size_t)snprintf(r + n, cap - n, "\\x%02x", c);
        }
        else r[n++] = (char)c;   /* printable ASCII + printable UTF-8 kept */
    }
    r[n++] = q;
    r[n] = '\0';
    return r;
}

/* ── param key-set gates ─────────────────────────────────────────────────── */
static bool gscf_str_in(const char *s, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(set[i], s) == 0) return true;
    return false;
}
/* set(params) - {end_reason} == {session_id, subscription_id} */
static bool gscf_detach_keyset(const json_t *params) {
    if (!gflow_is_obj(params)) return false;
    if (!json_has(params, "session_id") || !json_has(params, "subscription_id"))
        return false;
    for (size_t i = 0; i < params->c.count; i++) {
        const char *k = params->c.keys[i];
        if (strcmp(k, "session_id") != 0 && strcmp(k, "subscription_id") != 0
                && strcmp(k, "end_reason") != 0) return false;
    }
    return true;
}
/* set(params) == required (exact) */
static bool gscf_keyset_eq(const json_t *params, const char *const *req, size_t n) {
    if (!gflow_is_obj(params) || params->c.count != n) return false;
    for (size_t i = 0; i < n; i++)
        if (!json_has(params, req[i])) return false;
    return true;
}
/* _END_REASON = re.compile(r'[a-z][a-z_]{0,31}') fullmatch */
static bool gscf_end_reason_ok(const char *s) {
    if (!s || !s[0]) return false;
    size_t n = strlen(s);
    if (n > 32) return false;
    if (s[0] < 'a' || s[0] > 'z') return false;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || c == '_')) return false;
    }
    return true;
}

/* ── log seam (raw fmt + args; stub emits {ev:"log",level,fmt,args[,exc_info]}) */
static void gscf_log(gscf_env *e, int level, const char *fmt, json_t *args,
                     const json_t *exc_info) {
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args);   /* adopts */
    if (exc_info) json_set(a, "exc_info", json_copy(exc_info));
    json_free(gscf_call(e, "log", a));
}

/* ── shared leaves ───────────────────────────────────────────────────────── */
/* _profile_label core: falsy -> default; separator-free str kept; a home path
 * resolves through the profile_name_for_home seam (or -> default). */
static char *gscf_profile_label_str(gscf_env *e, const json_t *profile_id) {
    if (!gflow_truth(profile_id)) return strdup("default");
    char *text = gflow_pystr(profile_id);
    if (!strchr(text, '/') && !strchr(text, '\\')) return text;
    json_t *a = json_object();
    json_set(a, "path", json_string(text));
    json_t *r = gscf_call(e, "profile_name_for_home", a);
    char *out;
    if (gscf_is_raise(r)) {
        /* The live leaf has no guard: a raising seam propagates. The caller
         * checks env->exc; return NULL to unwind. */
        e->exc = gscf_norm_raise(r);
        free(text);
        return NULL;
    }
    if (gflow_truth(r)) out = gflow_pystr(r);
    else out = strdup("default");
    json_free(r);
    free(text);
    return out;
}

/* _voice_stop_key: (subject, profile_id, session_id, request_id) or NULL.
 * request_id = params.get('submission_id') or params.get('input_id'). */
static json_t *gscf_voice_stop_key(const json_t *selfn, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const json_t *ref = json_obj_get(in, "ref");
    const json_t *rid = gflow_obj_get(params, "submission_id");
    if (!gflow_truth(rid)) rid = gflow_obj_get(params, "input_id");
    if (!gflow_is_str(rid) || !gflow_truth(rid)) return NULL;
    json_t *key = json_array();
    json_append(key, json_string(gscf_subject(selfn)));
    json_append(key, json_string(json_get_str(ref, "profile_id", "")));
    json_append(key, json_string(json_get_str(ref, "session_id", "")));
    json_append(key, json_copy(rid));
    return key;
}

/* ── 1. _refusal_data ────────────────────────────────────────────────────── */
/* PoP: gscf_refusal_data @ gateway/session_controls.py:_refusal_data */
static json_t *gscf_refusal_data(gscf_env *e, const json_t *in) {
    (void)e;
    const json_t *exc = json_obj_get(in, "exc");
    json_t *data = json_object();
    const json_t *reason = gflow_obj_get(exc, "reason");
    json_set(data, "reason", reason ? json_copy(reason) : json_null());
    static const char *strkeys[] = { "field", "detail" };
    for (int i = 0; i < 2; i++) {
        const json_t *v = gflow_obj_get(exc, strkeys[i]);
        if (gflow_truth(v)) json_set(data, strkeys[i], json_copy(v));
    }
    static const char *intkeys[] = { "revision", "execution_generation" };
    for (int i = 0; i < 2; i++) {
        const json_t *v = gflow_obj_get(exc, intkeys[i]);
        if (!v) continue;
        /* isinstance(x, int): bool IS int; integral numbers are the int
         * fixture domain (fractional floats are not int). */
        bool is_int = (v->type == JSON_BOOL)
            || (v->type == JSON_NUMBER && v->num_val == (double)(long long)v->num_val);
        if (is_int) json_set(data, intkeys[i], json_copy(v));
    }
    const json_t *adm = gflow_obj_get(exc, "admitted");
    if (adm && adm->type == JSON_BOOL && !adm->bool_val)
        json_set(data, "admitted", json_bool(false));
    return data;
}

/* ── 2. _profile_label ───────────────────────────────────────────────────── */
/* PoP: gscf_profile_label @ gateway/session_controls.py:_profile_label */
static json_t *gscf_profile_label(gscf_env *e, const json_t *in) {
    char *r = gscf_profile_label_str(e, json_obj_get(in, "profile_id"));
    if (!r) return NULL;   /* env->exc set by the leaf */
    json_t *out = json_string(r);
    free(r);
    return out;
}

/* ── 3. _seen_storage_refusal ────────────────────────────────────────────── */
/* PoP: gscf_seen_storage_refusal @ gateway/session_controls.py:_seen_storage_refusal */
static json_t *gscf_seen_storage_refusal(gscf_env *e, const json_t *in) {
    const json_t *key = json_obj_get(in, "key");
    json_t *set = gscf_sect(e, "storage_refusals");
    if (gscf_arr_has(set, key)) return json_bool(true);
    if (set && set->type == JSON_ARRAY && set->c.count < 512)
        json_append(set, json_copy(key));
    return json_bool(false);
}

/* ── 4. _log_refusal ─────────────────────────────────────────────────────── */
/* PoP: gscf_log_refusal @ gateway/session_controls.py:_log_refusal */
static json_t *gscf_log_refusal(gscf_env *e, const json_t *in) {
    const json_t *profile = json_obj_get(in, "profile");
    const char *method = json_get_str(in, "method", "");
    const json_t *exc = json_obj_get(in, "exc");
    const char *reason = json_get_str(exc, "reason", "");
    const json_t *fieldv = gflow_obj_get(exc, "field");
    const json_t *detailv = gflow_obj_get(exc, "detail");
    /* args tuple evaluation order: method, _profile_label(profile), reason,
     * field f-string, detail f-string. */
    char *label = gscf_profile_label_str(e, profile);
    if (!label) return NULL;
    char *fieldstr = NULL, *detailstr = NULL;
    if (gflow_truth(fieldv)) {
        char *fs = gflow_pystr(fieldv);
        size_t n = strlen(fs) + 8;
        fieldstr = (char *)malloc(n);
        snprintf(fieldstr, n, " field=%s", fs);
        free(fs);
    }
    if (gflow_truth(detailv)) {
        char *ds = gflow_pystr(detailv);
        size_t n = strlen(ds) + 9;
        detailstr = (char *)malloc(n);
        snprintf(detailstr, n, " detail=%s", ds);
        free(ds);
    }
    json_t *args = json_array();
    json_append(args, json_string(method));
    json_append(args, json_string(label));
    json_append(args, json_string(reason));
    json_append(args, json_string(fieldstr ? fieldstr : ""));
    json_append(args, json_string(detailstr ? detailstr : ""));
    int level;
    if (strcmp(reason, "session_busy") == 0
            || !(gflow_truth(fieldv) || gflow_truth(detailv))) {
        level = 10;
    } else if (gflow_is_str(fieldv) && strcmp(fieldv->str_val, "request_id") == 0
               && strcmp(reason, "admission_conflict") == 0) {
        level = 20;
    } else if (strcmp(reason, "storage_unavailable") == 0) {
        /* key = (method, *args[1:]) */
        json_t *key = json_array();
        json_append(key, json_string(method));
        json_append(key, json_string(label));
        json_append(key, json_string(reason));
        json_append(key, json_string(fieldstr ? fieldstr : ""));
        json_append(key, json_string(detailstr ? detailstr : ""));
        bool seen = gscf_arr_has(gscf_sect(e, "storage_refusals"), key);
        if (!seen) {
            json_t *set = gscf_sect(e, "storage_refusals");
            if (set && set->type == JSON_ARRAY && set->c.count < 512)
                json_append(set, key);
            else
                json_free(key);
        } else {
            json_free(key);
        }
        level = seen ? 10 : 30;
    } else {
        level = 30;
    }
    gscf_log(e, level, "%s refused for profile=%s: %s%s%s", args, NULL);
    free(label);
    free(fieldstr);
    free(detailstr);
    return json_null();
}

/* ── 5. _log_storage_failure ─────────────────────────────────────────────── */
/* PoP: gscf_log_storage_failure @ gateway/session_controls.py:_log_storage_failure */
static json_t *gscf_log_storage_failure(gscf_env *e, const json_t *in) {
    const json_t *profile = json_obj_get(in, "profile");
    const char *method = json_get_str(in, "method", "");
    const json_t *session_id = json_obj_get(in, "session_id");
    const json_t *exc = json_obj_get(in, "exc");
    const char *kind = json_get_str(exc, "kind", "Exception");
    const char *msg = json_get_str(exc, "message", "");
    /* key = (method, type(exc).__name__); first = key not in set; add. */
    json_t *key = json_array();
    json_append(key, json_string(method));
    json_append(key, json_string(kind));
    json_t *tb = gscf_sect(e, "storage_tracebacks");
    bool first = !gscf_arr_has(tb, key);
    if (first && tb && tb->type == JSON_ARRAY) json_append(tb, key);
    else json_free(key);
    /* shown = repr(session_id); len > 64 -> shown[:48] + '… (N chars)' */
    char *shown = gscf_pyrepr(session_id);
    size_t slen = gscf_ulen(shown);
    if (slen > 64) {
        char *pre = gscf_uslice(shown, 48);
        size_t n = strlen(pre) + 48;
        char *cut = (char *)malloc(n);
        snprintf(cut, n, "%s\xe2\x80\xa6 (%zu chars)", pre, slen);
        free(pre);
        free(shown);
        shown = cut;
    }
    char *label = gscf_profile_label_str(e, profile);
    if (!label) { free(shown); return NULL; }
    json_t *args = json_array();
    json_append(args, json_string(method));
    json_append(args, json_string(label));
    json_append(args, json_string(shown));
    json_append(args, json_string(msg));   /* str(exc) */
    json_t *ei = json_bool(first);
    gscf_log(e, 30, "%s storage failure for profile=%s session_id=%s: %s", args, ei);
    json_free(ei);
    free(shown);
    free(label);
    return json_null();
}

/* ── 6. model_only ───────────────────────────────────────────────────────── */
/* PoP: gscf_model_only @ gateway/session_controls.py:model_only */
static json_t *gscf_model_only(gscf_env *e, const json_t *in) {
    (void)e;
    const json_t *row = json_obj_get(in, "row");
    const json_t *md = gflow_obj_get(row, "display_metadata");
    bool r = gflow_is_obj(md) && gflow_truth(gflow_obj_get(md, "model_only"));
    return json_bool(r);
}

/* ── 7. display_projection ───────────────────────────────────────────────── */
/* PoP: gscf_display_projection @ gateway/session_controls.py:display_projection */
static json_t *gscf_display_projection(gscf_env *e, const json_t *in) {
    const json_t *history = json_obj_get(in, "history");
    json_t *rows = json_array();
    if (gflow_is_arr(history)) {
        for (size_t i = 0; i < history->c.count; i++) {
            const json_t *row = history->c.items[i];
            json_t *copy = json_copy(row);
            if (gflow_is_obj(row) && json_has(row, "row_id"))
                json_set(copy, "_row_id", json_copy(json_obj_get(row, "row_id")));
            json_append(rows, copy);
        }
    }
    json_t *a = json_object();
    json_set(a, "rows", rows);   /* adopts */
    const json_t *ph = json_obj_get(in, "profile_home");
    json_set(a, "profile_home", ph ? json_copy(ph) : json_null());
    const json_t *iu = json_has(in, "image_urls") ? json_obj_get(in, "image_urls") : NULL;
    json_set(a, "image_urls", iu ? json_copy(iu) : json_bool(true));
    json_t *r = gscf_call(e, "history_to_messages", a);
    if (gscf_prop(e, r)) return NULL;
    return r;
}

/* ── 8. _sibling_for ─────────────────────────────────────────────────────── */
/* PoP: gscf_sibling_for @ gateway/session_controls.py:_sibling_for */
static json_t *gscf_sibling_for(gscf_env *e, const json_t *in) {
    const json_t *selfn = json_obj_get(in, "self");
    const char *profile = json_get_str(in, "profile", "");
    const char *aid = json_get_str(selfn, "authority", "");
    const json_t *auth = gscf_authority(e, selfn);
    /* profile_matches_home(profile, Path(self.authority.profile_id)) */
    json_t *a = json_object();
    json_set(a, "profile", json_string(profile));
    json_set(a, "home", json_string(json_get_str(auth, "profile_id", "")));
    json_t *r = gscf_call(e, "profile_matches_home", a);
    if (gscf_prop(e, r)) return NULL;
    bool own = gflow_truth(r);
    json_free(r);
    if (own) return json_null();
    if (!gscf_host_scoped(selfn)) {
        gscf_raise_ar(e, "profile_mismatch", NULL, NULL);
        return NULL;
    }
    const json_t *registry = gflow_obj_get(auth, "registry");
    if (gflow_is_obj(registry)) {
        const json_t *members = json_obj_get(registry, "members");
        if (gflow_is_arr(members)) {
            for (size_t i = 0; i < members->c.count; i++) {
                const json_t *mid = members->c.items[i];
                if (!gflow_is_str(mid)) continue;
                const char *said = mid->str_val;
                if (strcmp(said, aid) == 0) continue;   /* sibling is not self.authority */
                const json_t *sib = json_obj_get(gscf_sect(e, "authorities"), said);
                const char *spid = json_get_str(sib, "profile_id", "");
                json_t *ma = json_object();
                json_set(ma, "profile", json_string(profile));
                json_set(ma, "home", json_string(spid));
                json_t *mr = gscf_call(e, "profile_matches_home", ma);
                if (gscf_prop(e, mr)) return NULL;
                bool match = gflow_truth(mr);
                json_free(mr);
                if (!match) continue;
                json_t *siblings = gscf_sect(e, "siblings");
                const json_t *cached = json_obj_get(siblings, spid);
                const char *caid = cached ? json_get_str(cached, "authority", "") : "";
                if (!cached || strcmp(caid, said) != 0) {
                    /* identity = dict(self._identity, profile_id=..., instance_id=...) */
                    json_t *id2 = json_copy(json_obj_get(selfn, "identity"));
                    if (!id2 || id2->type != JSON_OBJECT) {
                        json_free(id2);
                        id2 = json_object();
                    }
                    json_set(id2, "profile_id", json_string(spid));
                    json_set(id2, "instance_id",
                             json_string(json_get_str(sib, "instance_id", "")));
                    json_t *na = json_object();
                    json_set(na, "authority_id", json_string(said));
                    json_set(na, "identity", id2);   /* adopts */
                    json_set(na, "operator",
                             json_bool(json_get_bool(selfn, "operator", false)));
                    json_t *conn = gscf_call(e, "new_connection", na);
                    if (gscf_prop(e, conn)) return NULL;
                    if (!conn) conn = json_object();
                    json_set(siblings, spid, json_copy(conn));
                    return conn;
                }
                return json_copy(cached);
            }
        }
    }
    if (gflow_is_obj(registry)) {
        json_t *sa = json_object();
        json_set(sa, "profile", json_string(profile));
        json_t *sr = gscf_call(e, "registry_starting", sa);
        if (gscf_prop(e, sr)) return NULL;
        bool starting = sr != NULL && sr->type != JSON_NULL;
        json_free(sr);
        if (starting) {
            gscf_raise_ar(e, "runtime_draining", "profile",
                          "the profile is still starting");
            return NULL;
        }
    }
    gscf_raise_ar(e, "profile_mismatch", NULL, NULL);
    return NULL;
}

/* ── 9. awaits_profile_start ─────────────────────────────────────────────── */
/* PoP: gscf_awaits_profile_start @ gateway/session_controls.py:awaits_profile_start */
static json_t *gscf_awaits_profile_start(gscf_env *e, const json_t *in) {
    const json_t *selfn = json_obj_get(in, "self");
    const json_t *request = json_obj_get(in, "request");
    const json_t *held = json_obj_get(in, "held");
    const json_t *auth = gscf_authority(e, selfn);
    const json_t *params = gflow_is_obj(request) ? json_obj_get(request, "params") : NULL;
    const json_t *profile = gflow_is_obj(params) ? json_obj_get(params, "profile") : NULL;
    const json_t *registry = gflow_obj_get(auth, "registry");
    if (!(gscf_host_scoped(selfn) && gflow_is_str(profile) && gflow_truth(profile)
          && gflow_is_obj(registry)))
        return json_null();
    const char *pstr = profile->str_val;
    const char *key = NULL;
    if (gflow_is_arr(held)) {
        for (size_t i = 0; i < held->c.count; i++) {
            const json_t *h = held->c.items[i];
            if (!gflow_is_str(h)) continue;
            json_t *a = json_object();
            json_set(a, "profile", json_string(pstr));
            json_set(a, "home", json_string(h->str_val));
            json_t *r = gscf_call(e, "profile_matches_home", a);
            if (gscf_prop(e, r)) return NULL;
            bool match = gflow_truth(r);
            json_free(r);
            if (match) { key = h->str_val; break; }
        }
    }
    if (key) {
        json_t *out = json_array();
        json_append(out, json_string(key));
        json_append(out, json_null());
        return out;
    }
    json_t *a = json_object();
    json_set(a, "profile", json_string(pstr));
    json_t *r = gscf_call(e, "registry_starting", a);
    if (gscf_prop(e, r)) return NULL;
    return r;
}

/* ── 10. ping ────────────────────────────────────────────────────────────── */
/* PoP: gscf_ping @ gateway/session_controls.py:ping */
static json_t *gscf_ping(gscf_env *e, const json_t *in) {
    const json_t *selfn = json_obj_get(in, "self");
    const json_t *caps = gflow_obj_get(json_obj_get(selfn, "actor"), "capabilities");
    if (!gflow_truth(caps)) {
        gscf_raise_ar(e, "permission_denied", NULL, NULL);
        return NULL;
    }
    const json_t *params = json_obj_get(in, "params");
    if (gflow_truth(params)) {
        gscf_raise_ar(e, "invalid_params", NULL, NULL);
        return NULL;
    }
    json_t *out = json_object();
    json_set(out, "pong", json_bool(true));
    return out;
}

/* ── 11. detach ──────────────────────────────────────────────────────────── */
/* PoP: gscf_detach @ gateway/session_controls.py:detach */
static json_t *gscf_detach(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    if (!gscf_detach_keyset(params)) {
        gscf_raise_ar(e, "invalid_params", NULL, NULL);
        return NULL;
    }
    const json_t *sidv = json_obj_get(params, "session_id");
    const json_t *subv = json_obj_get(params, "subscription_id");
    const json_t *er = json_obj_get(params, "end_reason");   /* NULL == absent */
    const char *sid = gflow_str(sidv);
    const char *subid = gflow_str(subv);
    if (!sid || !sid[0] || !subid || !subid[0]
            || (er && er->type != JSON_NULL
                && (!gflow_is_str(er) || !gscf_end_reason_ok(er->str_val)))) {
        gscf_raise_ar(e, "invalid_params", NULL, NULL);
        return NULL;
    }
    json_t *result = json_object();
    json_set(result, "session_id", json_string(sid));
    json_set(result, "subscription_id", json_string(subid));
    json_set(result, "detached", json_bool(false));
    json_t *subs = gscf_sect(e, "subscriptions");
    const json_t *cur = json_obj_get(subs, sid);
    if (!cur || !gflow_is_str(cur) || strcmp(cur->str_val, subid) != 0)
        return result;
    json_t *a = json_object();
    json_set(a, "session_id", json_string(sid));
    json_set(a, "subscription_id", json_string(subid));
    json_set(a, "end_reason", er ? json_copy(er) : json_null());
    json_t *r = gscf_call(e, "detach", a);
    if (gscf_prop(e, r)) { json_free(result); return NULL; }
    json_free(r);
    /* The detach may have evicted the subscription: re-check before delete. */
    const json_t *cur2 = json_obj_get(subs, sid);
    if (cur2 && gflow_is_str(cur2) && strcmp(cur2->str_val, subid) == 0)
        json_obj_del(subs, sid);
    if (gflow_truth(er))
        gscf_strset_discard(gscf_sect(e, "handed_back"), sid);
    else
        gscf_strset_add(gscf_sect(e, "handed_back"), sid);
    gscf_strset_discard(gscf_sect(e, "reconnects"), sid);
    json_set(result, "detached", json_bool(true));
    return result;
}

/* ── 12. events_since ────────────────────────────────────────────────────── */
/* PoP: gscf_events_since @ gateway/session_controls.py:events_since */
static json_t *gscf_events_since(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const char *sid = gscf_sid(in);
    json_t *a = json_object();
    json_set(a, "capability", json_string("session:read"));
    json_set(a, "session_id", json_string(sid));
    json_t *r = gscf_call(e, "authorize", a);
    if (gscf_prop(e, r)) return NULL;
    json_free(r);
    /* sequence = params.get('last_sequence', params.get('last_seen', 0)) —
     * both absent: the default 0 IS a valid int; a present null is not. */
    const json_t *seqv = json_has(params, "last_sequence")
        ? json_obj_get(params, "last_sequence")
        : (json_has(params, "last_seen") ? json_obj_get(params, "last_seen") : NULL);
    double seq = 0;
    bool seq_ok;
    if (!seqv) {
        seq_ok = true;
    } else if (seqv->type == JSON_NUMBER
               && seqv->num_val == (double)(long long)seqv->num_val) {
        seq = seqv->num_val;
        seq_ok = true;
    } else {
        seq_ok = false;
    }
    const json_t *epoch = gflow_obj_get(params, "replay_epoch");
    if (!seq_ok || seq < 0 || (epoch && epoch->type != JSON_NULL && !gflow_is_str(epoch))) {
        gscf_raise_ar(e, "invalid_params", NULL, NULL);
        return NULL;
    }
    const json_t *sess = json_obj_get(gscf_sect(e, "sessions"), sid);
    if (!sess) {
        char *rep = gscf_pyrepr(json_string(sid));
        gscf_raise(e, "KeyError", rep);
        free(rep);
        return NULL;
    }
    json_t *sa = json_object();
    json_set(sa, "epoch", epoch ? json_copy(epoch) : json_null());
    json_set(sa, "sequence", json_number(seq));
    json_t *replay = gscf_call(e, "stream_since", sa);
    if (gscf_prop(e, replay)) return NULL;
    if (!gflow_is_obj(replay)) {
        /* live indexes replay['snapshot_required'] — a non-mapping raises
         * TypeError; fixtures always script an object. Fail closed. */
        json_free(replay);
        gscf_raise(e, "TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (!gscf_subscribed(e, sid) && !gscf_strset_has(gscf_sect(e, "handed_back"), sid))
        gscf_strset_add(gscf_sect(e, "reconnects"), sid);
    const json_t *sr = json_obj_get(replay, "snapshot_required");
    if (!sr) {
        json_free(replay);
        gscf_raise(e, "KeyError", "'snapshot_required'");
        return NULL;
    }
    if (!gflow_truth(sr)) {
        const json_t *re = json_obj_get(replay, "replay_epoch");
        if (!re) {
            json_free(replay);
            gscf_raise(e, "KeyError", "'replay_epoch'");
            return NULL;
        }
        /* max((frame['seq'] ...), default=sequence): the default applies ONLY
         * to an empty event list; a non-empty list maxes its own seqs. */
        double maxseq = seq;
        bool any = false;
        const json_t *events = json_obj_get(replay, "events");
        if (gflow_is_arr(events)) {
            for (size_t i = 0; i < events->c.count; i++) {
                const json_t *frame = events->c.items[i];
                const json_t *sq = gflow_obj_get(frame, "seq");
                if (!sq) {
                    json_free(replay);
                    gscf_raise(e, "KeyError", "'seq'");
                    return NULL;
                }
                if (sq->type == JSON_NUMBER) {
                    if (!any) { maxseq = sq->num_val; any = true; }
                    else if (sq->num_val > maxseq) maxseq = sq->num_val;
                }
            }
        }
        json_t *cursor = json_array();
        json_append(cursor, json_copy(re));
        json_append(cursor, json_number(maxseq));
        json_set(gscf_sect(e, "replay_cursors"), sid, cursor);   /* adopts */
    }
    json_t *oa = json_object();
    json_set(oa, "session_id", json_string(sid));
    json_t *openr = gscf_call(e, "open_requests", oa);
    if (gscf_prop(e, openr)) { json_free(replay); return NULL; }
    json_set(replay, "open_requests", openr ? openr : json_null());   /* adopts */
    return replay;
}

/* ── 13. _voice_stop_replay ──────────────────────────────────────────────── */
/* PoP: gscf_voice_stop_replay @ gateway/session_controls.py:_voice_stop_replay */
static json_t *gscf_voice_stop_replay(gscf_env *e, const json_t *in) {
    const json_t *selfn = json_obj_get(in, "self");
    const char *sid = gscf_sid(in);
    json_t *key = gscf_voice_stop_key(selfn, in);
    if (!key) return json_bool(false);
    const json_t *consumed = gscf_sect(e, "consumed_voice_stops");
    bool known = consumed && consumed->type == JSON_ARRAY && gscf_arr_has(consumed, key);
    json_free(key);
    if (!known) return json_bool(false);
    if (!gscf_subscribed(e, sid)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "capability", json_string("session:submit"));
    json_set(a, "session_id", json_string(sid));
    json_t *r = gscf_call(e, "authorize", a);
    if (gscf_prop(e, r)) return NULL;
    json_free(r);
    return json_bool(true);
}

/* ── 14. _record_voice_stop ──────────────────────────────────────────────── */
/* PoP: gscf_record_voice_stop @ gateway/session_controls.py:_record_voice_stop */
static json_t *gscf_record_voice_stop(gscf_env *e, const json_t *in) {
    const json_t *selfn = json_obj_get(in, "self");
    json_t *key = gscf_voice_stop_key(selfn, in);
    if (!key) return json_null();
    json_t *consumed = gscf_sect(e, "consumed_voice_stops");
    if (!consumed || consumed->type == JSON_NULL) {
        consumed = json_array();
        json_set(e->state, "consumed_voice_stops", consumed);   /* adopts */
    }
    gscf_key_move_end(consumed, key);
    json_free(key);
    while (consumed->c.count > 256) gscf_arr_pop0(consumed);
    return json_null();
}

/* ── 15. _typed_voice_stop ───────────────────────────────────────────────── */
/* PoP: gscf_typed_voice_stop @ gateway/session_controls.py:_typed_voice_stop */
static json_t *gscf_typed_voice_stop(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const char *sid = gscf_sid(in);
    const json_t *text = gflow_obj_get(params, "text");
    if (!gflow_is_str(text)) return json_bool(false);
    if (!gscf_subscribed(e, sid)) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "capability", json_string("session:submit"));
    json_set(a, "session_id", json_string(sid));
    json_t *r = gscf_call(e, "authorize", a);
    if (gscf_prop(e, r)) return NULL;
    json_free(r);
    const json_t *live = json_obj_get(gscf_sect(e, "sessions"), sid);
    const json_t *source = live ? json_obj_get(live, "source") : NULL;
    if (!source || source->type == JSON_NULL) return json_bool(false);
    const char *platform = json_get_str(source, "platform", "");
    if (strcmp(platform, "local") != 0) return json_bool(false);
    json_t *va = json_object();
    json_set(va, "session_id", json_string(sid));
    json_t *vr = gscf_call(e, "local_voice_enabled", va);
    if (gscf_prop(e, vr)) return NULL;
    bool enabled = gflow_truth(vr);
    json_free(vr);
    if (!enabled) return json_bool(false);
    json_t *ca = json_object();
    json_set(ca, "text", json_copy(text));
    json_set(ca, "session_id", json_string(sid));
    json_set(ca, "emit", json_bool(false));
    json_t *cr = gscf_call(e, "consume_typed_stop_phrase", ca);
    if (gscf_prop(e, cr)) return NULL;
    return cr;
}

/* ── 16. submission ──────────────────────────────────────────────────────── */
/* PoP: gscf_submission @ gateway/session_controls.py:submission */
static json_t *gscf_submission(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const json_t *ref = json_obj_get(in, "ref");
    const char *sid = gscf_sid(in);
    if (!gscf_subscribed(e, sid)) {
        gscf_raise_ar(e, "permission_denied", NULL, NULL);
        return NULL;
    }
    static const char *allowed[] = {
        "session_id", "text", "submission_id", "input_id", "queued", "attachments",
        "finite", "unattended", "surface", "voice_context", "interrupted",
        "voice_turn", "display_kind", "title_preview", "turn_author", "api_text",
        "model_override", "clarify_batch",
    };
    if (gflow_is_obj(params)) {
        for (size_t i = 0; i < params->c.count; i++) {
            if (!gscf_str_in(params->c.keys[i], allowed,
                             sizeof(allowed) / sizeof(allowed[0]))) {
                gscf_raise_ar(e, "invalid_params", NULL, NULL);
                return NULL;
            }
        }
    }
    const json_t *rid = gflow_obj_get(params, "submission_id");
    if (!gflow_truth(rid)) rid = gflow_obj_get(params, "input_id");
    if (!gflow_is_str(rid) || !gflow_truth(rid)) {
        gscf_raise_ar(e, "invalid_params", NULL, NULL);
        return NULL;
    }
    /* payload = {'text': ..., **admit_finite(...), **surface, **display, **cli} */
    json_t *payload = json_object();
    const json_t *text = gflow_obj_get(params, "text");
    json_set(payload, "text", text ? json_copy(text) : json_null());
    json_t *fa = json_object();
    json_set(fa, "params", params ? json_copy(params) : json_object());
    json_set(fa, "ref", ref ? json_copy(ref) : json_object());
    json_t *fr = gscf_call(e, "admit_finite", fa);
    if (gscf_prop(e, fr)) { json_free(payload); return NULL; }
    if (gflow_is_obj(fr)) {
        for (size_t i = 0; i < fr->c.count; i++)
            json_set(payload, fr->c.keys[i], json_copy(fr->c.items[i]));
    }
    json_free(fr);
    static const char *mergers[] = {
        "submit_surface_fields", "submit_display_fields", "submit_cli_turn_fields",
    };
    for (int m = 0; m < 3; m++) {
        json_t *ma = json_object();
        json_set(ma, "params", params ? json_copy(params) : json_object());
        json_t *mr = gscf_call(e, mergers[m], ma);
        if (gscf_prop(e, mr)) { json_free(payload); return NULL; }
        if (gflow_is_obj(mr)) {
            for (size_t i = 0; i < mr->c.count; i++)
                json_set(payload, mr->c.keys[i], json_copy(mr->c.items[i]));
        }
        json_free(mr);
    }
    if (json_has(params, "attachments"))
        json_set(payload, "attachments", json_copy(json_obj_get(params, "attachments")));
    /* Submission(request_id, ref, payload, 'queue') — asdict projection. */
    json_t *out = json_object();
    json_set(out, "request_id", json_copy(rid));
    json_t *refout = json_object();
    json_set(refout, "profile_id", json_string(json_get_str(ref, "profile_id", "")));
    json_set(refout, "session_id", json_string(sid));
    json_set(out, "ref", refout);
    json_set(out, "payload", payload);   /* adopts */
    json_set(out, "intent", json_string("queue"));
    return out;
}

/* ── 17/18. interrupt + cancel shared interrupt core ─────────────────────── */
/* self.interrupt(ref, {'execution_generation': gen}): interrupt_managed first,
 * then the authority interrupt. swallow_stale models cancel's STARTED-branch
 * `except AuthorityRefusal: if reason != 'stale_generation': raise` (the
 * queued-claim branch has NO guard live). Any raise from the three seams is
 * normalized into env->exc unless swallowed. */
static json_t *gscf_do_interrupt(gscf_env *e, const char *sid, const json_t *gen,
                                 bool swallow_stale) {
    json_t *a = json_object();
    json_set(a, "session_id", json_string(sid));
    json_set(a, "execution_generation", gen ? json_copy(gen) : json_null());
    json_t *im = gscf_call(e, "interrupt_managed", a);
    if (gscf_is_raise(im)) {
        if (swallow_stale && gscf_is_ar(im)
                && strcmp(json_get_str(im, "message", ""), "stale_generation") == 0) {
            json_free(im);
            return NULL;   /* swallowed: cancel continues to its receipt read */
        }
        e->exc = gscf_norm_raise(im);
        return NULL;
    }
    bool managed = gflow_truth(im);
    json_free(im);
    json_t *ha = json_object();
    json_set(ha, "session_id", json_string(sid));
    json_t *h;
    if (managed) {
        h = gscf_call(e, "handle", ha);
    } else {
        json_set(ha, "execution_generation", gen ? json_copy(gen) : json_null());
        h = gscf_call(e, "authority_interrupt", ha);
    }
    if (gscf_is_raise(h)) {
        if (swallow_stale && gscf_is_ar(h)
                && strcmp(json_get_str(h, "message", ""), "stale_generation") == 0) {
            json_free(h);
            return NULL;
        }
        e->exc = gscf_norm_raise(h);
        return NULL;
    }
    return h;
}

/* PoP: gscf_interrupt @ gateway/session_controls.py:interrupt */
static json_t *gscf_interrupt(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const json_t *gen = gflow_obj_get(params, "execution_generation");
    json_t *r = gscf_do_interrupt(e, gscf_sid(in), gen, false);
    if (e->exc) { json_free(r); return NULL; }
    return r;
}

/* ── 18. cancel ──────────────────────────────────────────────────────────── */
/* PoP: gscf_cancel @ gateway/session_controls.py:cancel */
static json_t *gscf_cancel(gscf_env *e, const json_t *in) {
    const json_t *params = json_obj_get(in, "params");
    const char *sid = gscf_sid(in);
    const json_t *admid = gflow_obj_get(params, "admission_id");
    json_t *ra = json_object();
    json_set(ra, "session_id", json_string(sid));
    json_set(ra, "admission_id", admid ? json_copy(admid) : json_null());
    json_t *rc = gscf_call(e, "receipt", ra);
    if (gscf_prop(e, rc)) return NULL;
    const char *status = json_get_str(rc, "status", "");
    bool stop_started = false;
    if (strcmp(status, "started") == 0) {
        /* authority_persistence(authority).admissions.get(admission_id=...) */
        json_t *ga = json_object();
        json_set(ga, "admission_id", admid ? json_copy(admid) : json_null());
        json_t *row = gscf_call(e, "admissions_get", ga);
        if (gscf_prop(e, row)) { json_free(rc); return NULL; }
        if (!row || row->type == JSON_NULL) {
            json_free(row);
            json_free(rc);
            gscf_raise(e, "AttributeError",
                       "'NoneType' object has no attribute 'status'");
            return NULL;
        }
        stop_started = strcmp(json_get_str(row, "status", ""), "started") == 0;
        json_free(row);
    }
    if (stop_started) {
        /* Stop this claim in-RPC; a stale generation means it already settled. */
        const json_t *gen = gflow_obj_get(rc, "execution_generation");
        json_t *h = gscf_do_interrupt(e, sid, gen, true);
        if (e->exc) { json_free(h); json_free(rc); return NULL; }
        json_free(h);
        json_free(rc);
        json_t *r2a = json_object();
        json_set(r2a, "session_id", json_string(sid));
        json_set(r2a, "admission_id", admid ? json_copy(admid) : json_null());
        json_t *rc2 = gscf_call(e, "receipt", r2a);
        if (gscf_prop(e, rc2)) return NULL;
        return rc2;
    }
    if (strcmp(status, "queued") != 0) {
        json_free(rc);
        json_t *ca = json_object();
        json_set(ca, "session_id", json_string(sid));
        json_set(ca, "admission_id", admid ? json_copy(admid) : json_null());
        json_t *r = gscf_call(e, "cancel_queued", ca);
        if (gscf_prop(e, r)) return NULL;
        return r;
    }
    /* Queued at this read: cancel, or stop the claim the drain took. Live has
     * NO stale_generation guard on this branch's interrupt. */
    json_t *cla = json_object();
    json_set(cla, "session_id", json_string(sid));
    json_set(cla, "admission_id", admid ? json_copy(admid) : json_null());
    json_t *row = gscf_call(e, "cancel_or_claimed", cla);
    if (gscf_prop(e, row)) { json_free(rc); return NULL; }
    if (row && row->type != JSON_NULL) {
        const json_t *gen = json_obj_get(row, "generation");
        if (!gen) {
            json_free(row);
            json_free(rc);
            gscf_raise(e, "KeyError", "'generation'");
            return NULL;
        }
        json_t *h = gscf_do_interrupt(e, sid, gen, false);
        if (e->exc) { json_free(h); json_free(row); json_free(rc); return NULL; }
        json_free(h);
    }
    json_free(row);
    json_free(rc);
    json_t *r2a = json_object();
    json_set(r2a, "session_id", json_string(sid));
    json_set(r2a, "admission_id", admid ? json_copy(admid) : json_null());
    json_t *rc2 = gscf_call(e, "receipt", r2a);
    if (gscf_prop(e, rc2)) return NULL;
    return rc2;
}
