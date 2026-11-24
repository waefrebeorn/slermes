/*
 * profiles_service_cleanup.c - C11 port of upstream hermes_cli/profiles_service_cleanup.py
 * (gate w25_profilesse_1c3aa6).
 *
 * PoP: w25_profilesse_1c3aa6_owned_by_profile @ hermes_cli/profiles_service_cleanup.py:owned_by_profile
 * PoP: w25_profilesse_1c3aa6_system_unit @ hermes_cli/profiles_service_cleanup.py:remove_system_systemd_unit
 * PoP: w25_profilesse_1c3aa6_windows_task @ hermes_cli/profiles_service_cleanup.py:remove_windows_task
 * PoP: w25_profilesse_1c3aa6_constants @ hermes_cli/profiles_service_cleanup.py:<literal tables>
 *
 * pinned_home(), Path resolution, geteuid(), systemctl, schtasks and the
 * startup folder are seams.  What is ported here is the part that is a pure
 * function of values: whether the definition belongs to this profile, and the
 * exact notices each removal arm prints.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_profilesse_1c3aa6.h"

/* The literal notices, so the arms below read like the upstream f-strings. */
#define NOTICE_LEFT_ALONE "another install's service, left alone"
#define NOTICE_BOOT "; it will restart the removed profile at boot."

static int ps_truthy(const json_t *v) {
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

static const char *ps_str(const json_t *v) {
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return NULL;
}

/* A required string, empty when the caller passed nothing. */
static const char *ps_text(const json_t *v) {
    const char *s = ps_str(v);
    return s ? s : "";
}

/* Python repr() of a str: single quotes unless the text holds a ' and no ". */
static void ps_repr(char *out, size_t cap, const char *s) {
    char quote = '\'';
    size_t i = 0;
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    if (i + 1 < cap) out[i++] = quote;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == (unsigned char)quote) {
            if (i + 2 < cap) { out[i++] = '\\'; out[i++] = (char)c; }
        } else if (c == '\n') {
            if (i + 2 < cap) { out[i++] = '\\'; out[i++] = 'n'; }
        } else if (c == '\r') {
            if (i + 2 < cap) { out[i++] = '\\'; out[i++] = 'r'; }
        } else if (c == '\t') {
            if (i + 2 < cap) { out[i++] = '\\'; out[i++] = 't'; }
        } else if (i + 1 < cap) {
            out[i++] = (char)c;
        }
    }
    if (i + 1 < cap) out[i++] = quote;
    out[i] = '\0';
}

/* str.strip() */
static char *ps_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r' || *b == '\f' || *b == '\v') b++;
    e = b + strlen(b);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'
                     || e[-1] == '\f' || e[-1] == '\v')) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* One printed line, exactly as the upstream print() would render it. */
static void ps_pushf(json_t *list, const char *fmt, ...) {
    va_list ap, copy;
    char *line;
    int n;
    va_start(ap, fmt);
    va_copy(copy, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(copy); return; }
    line = (char *)malloc((size_t)n + 1);
    if (line) {
        vsnprintf(line, (size_t)n + 1, fmt, copy);
        json_append(list, json_string(line));
        free(line);
    }
    va_end(copy);
}

/* f"↷ {definition} runs HERMES_HOME={raw}, not {profile_dir}: another install's
 * service, left alone" -- the one-line notice owned_by_profile prints. */
json_t *w25_profilesse_1c3aa6_owned_by_profile(const json_t *req) {
    const json_t *v_raw, *v_rr, *v_rp;
    const char *raw, *rr, *rp, *def, *pd;
    int owned;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    v_raw = json_obj_get(req, "raw");
    raw = ps_str(v_raw);
    v_rr = json_obj_get(req, "resolved_raw");
    rr = ps_str(v_rr);
    v_rp = json_obj_get(req, "resolved_profile");
    rp = ps_str(v_rp);
    def = ps_str(json_obj_get(req, "definition"));
    pd = ps_str(json_obj_get(req, "profile_dir"));

    /* raw is None (nothing pinned) or the two sides resolve to the same path --
     * including both failing to resolve (None == None). */
    if (!raw) owned = 1;
    else if (!rr || !rp) owned = (!rr && !rp);
    else owned = !strcmp(rr, rp);

    out = json_object();
    json_set(out, "owned", json_bool(owned ? true : false));
    if (owned) {
        json_set(out, "notice", json_null());
        return out;
    }
    {
        size_t need = strlen(def ? def : "") + strlen(raw) + strlen(pd ? pd : "")
                      + strlen(NOTICE_LEFT_ALONE) + 32;
        char *msg = (char *)malloc(need);
        if (msg) {
            snprintf(msg, need, "↷ %s runs HERMES_HOME=%s, not %s: %s",
                     def ? def : "", raw, pd ? pd : "", NOTICE_LEFT_ALONE);
            json_set(out, "notice", json_string(msg));
            free(msg);
        } else {
            json_set(out, "notice", json_null());
        }
    }
    return out;
}

json_t *w25_profilesse_1c3aa6_system_unit(const json_t *req) {
    const json_t *v;
    const char *svc, *unit;
    int exists, owned, is_root;
    long rc = 0;
    json_t *out, *msgs;

    if (!req || req->type != JSON_OBJECT) return json_null();
    svc = ps_str(json_obj_get(req, "svc_name"));
    unit = ps_str(json_obj_get(req, "unit"));
    if (!svc) svc = "";
    if (!unit) unit = "";
    exists = ps_truthy(json_obj_get(req, "exists"));
    owned = ps_truthy(json_obj_get(req, "owned"));
    is_root = ps_truthy(json_obj_get(req, "is_root"));
    v = json_obj_get(req, "stop_rc");
    if (v && v->type == JSON_NUMBER) rc = (long)v->num_val;
    else if (v && v->type == JSON_BOOL) rc = v->bool_val ? 1 : 0;

    out = json_object();
    msgs = json_array();
    if (!exists || !owned) { /* nothing to remove, nothing to say */
        json_set(out, "removed", json_bool(false));
        json_set(out, "messages", msgs);
        return out;
    }
    if (!is_root) {
        ps_pushf(msgs, "⚠ System service %s remains at %s" NOTICE_BOOT, svc, unit);
        ps_pushf(msgs,
                 "  Remove it with: sudo systemctl disable --now %s && sudo rm %s && sudo systemctl daemon-reload",
                 svc, unit);
        json_set(out, "removed", json_bool(false));
        json_set(out, "messages", msgs);
        return out;
    }
    /* root arm: disable, stop, unlink and daemon-reload already succeeded */
    if (rc != 0) {
        ps_pushf(msgs,
                 "⚠ System service %s unit removed, but `systemctl stop` exited %ld; "
                 "its gateway may still be running. Stop it with: sudo systemctl stop %s",
                 svc, rc, svc);
    } else {
        ps_pushf(msgs, "✓ System service %s removed", svc);
    }
    json_set(out, "removed", json_bool(true));
    json_set(out, "messages", msgs);
    return out;
}

json_t *w25_profilesse_1c3aa6_windows_task(const json_t *req) {
    const json_t *entries, *v_code;
    const char *task, *err;
    int registered, removed = 0;
    long code = 0;
    json_t *out, *msgs;
    char repr[1024];

    if (!req || req->type != JSON_OBJECT) return json_null();
    task = ps_str(json_obj_get(req, "task_name"));
    err = ps_str(json_obj_get(req, "err"));
    registered = ps_truthy(json_obj_get(req, "registered"));
    v_code = json_obj_get(req, "delete_code");
    if (v_code && v_code->type == JSON_NUMBER) code = (long)v_code->num_val;

    out = json_object();
    msgs = json_array();
    ps_repr(repr, sizeof repr, task);

    if (registered) {
        if (code == 0) {
            ps_pushf(msgs, "✓ Removed Scheduled Task %s", repr);
            removed = 1;
        } else {
            char *stripped = ps_strip(err);
            ps_pushf(msgs, "⚠ Scheduled Task %s remains (schtasks code %ld): %s",
                     repr, code, stripped ? stripped : "");
            free(stripped);
        }
    }

    entries = json_obj_get(req, "entries");
    if (entries && entries->type == JSON_ARRAY) {
        size_t i, n = json_len(entries);
        for (i = 0; i < n; i++) {
            const json_t *pair = json_get(entries, i);
            const json_t *v_exists;
            if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) continue;
            v_exists = json_get(pair, 1);
            if (!ps_truthy(v_exists)) continue; /* FileNotFoundError: continue */
            ps_pushf(msgs, "✓ Removed Windows login item: %s", ps_text(json_get(pair, 0)));
            removed = 1;
        }
    }

    json_set(out, "removed", json_bool(removed ? true : false));
    json_set(out, "messages", msgs);
    return out;
}

json_t *w25_profilesse_1c3aa6_constants(void) {
    json_t *o = json_object();
    json_set(o, "system_unit_dir", json_string(W25_PROFILESSE_1C3AA6_SYSTEM_UNIT_DIR));
    json_set(o, "unit_suffix", json_string(W25_PROFILESSE_1C3AA6_UNIT_SUFFIX));
    json_set(o, "run_timeout_s", json_number(W25_PROFILESSE_1C3AA6_RUN_TIMEOUT_S));
    json_set(o, "schtasks_timeout_s", json_number(W25_PROFILESSE_1C3AA6_SCHTASKS_TIMEOUT_S));
    return o;
}
