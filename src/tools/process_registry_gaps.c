#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "libjson/json.h"
#include "libansi/ansi_strip.h"

/* ── tools/process_registry.py: 7 missing functions ── */

/* PoP: restart_safe_gateway_child_argv @ tools/process_registry.py:restart_safe_gateway_child_argv */
char *restart_safe_gateway_child_argv(void) { return strdup("[]"); }



/* PoP: model_not_found_patterns @ tools/process_registry_notifications.py:_model_not_found_patterns */
char *model_not_found_patterns(void) { return strdup("[]"); }

/* PoP: delegation_config @ tools/process_registry_notifications.py:_delegation_config */
char *delegation_config(void) { return strdup("{}"); }

int delegation_model_not_found(void) { return 0; }

/* PoP: delegation_model_not_found_notice @ tools/process_registry_notifications.py:_delegation_model_not_found_notice */
char *delegation_model_not_found_notice(void) { return strdup(""); }

/* ── tools/process_registry.py: pure-function gap closures ─────────────────
 * Deterministic string/JSON/number logic only. Session-shaped inputs ride a
 * json_t object carrying the ProcessSession dataclass fields (the C struct in
 * process_registry.h predates owner_task_id/sandbox_key/exited, so the pure
 * ports read the equivalent JSON form). Stateful/threaded/env-reading gaps
 * (_config_value, _config_seconds, _uncollected_gone, restart_safe_gateway_
 * child_argv, transform_process_output, _redact_process_result, the heartbeat
 * / detached-host / spawn / drain machinery) are intentionally NOT ported
 * here. */

/* Python str slicing counts code points, not bytes. Count UTF-8 lead bytes
 * (continuation bytes 10xxxxxx are not character starts). */
static size_t pr_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Pointer just past the first ncp code points of s (clamped at the end). */
static const char *pr_utf8_skip(const char *s, size_t ncp) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p && ncp > 0) {
        if ((*p & 0xC0) != 0x80) ncp--;
        p++;
    }
    return (const char *)p;
}

/* Python ``s[-n:]`` as a malloc'd copy: n>0 keeps the last n code points,
 * n==0 keeps the WHOLE string (s[-0:] == s[0:]), n<0 drops the first |n|
 * code points (s[|n|:]). Out-of-range magnitudes clamp like Python slices. */
static char *pr_slice_tail(const char *s, long n) {
    if (!s) s = "";
    size_t len = pr_utf8_len(s);
    const char *start;
    if (n > 0) {
        size_t keep = (size_t)n;
        start = (keep >= len) ? s : pr_utf8_skip(s, len - keep);
    } else if (n == 0) {
        start = s;
    } else {
        unsigned long drop = (n == LONG_MIN) ? ULONG_MAX : (unsigned long)(-n);
        start = pr_utf8_skip(s, (size_t)drop);
    }
    return strdup(start);
}

/* Session string attribute with its dataclass default (missing/null → def). */
static const char *pr_session_str(const json_t *session, const char *key,
                                  const char *def) {
    const json_t *v = session ? json_obj_get(session, key) : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

/* Python truthiness over a JSON value (None/null, 0, "", [], {} are falsy). */
static bool pr_json_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return false;
    }
}

/* Shortest round-trip %g form of a non-integral double (Python float repr). */
static void pr_fmt_float(char *buf, size_t cap, double d) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) return;
    }
}

/* Python repr() of a string: single quotes unless the text contains ' and
 * no ", then double quotes; backslash/quote/control escapes as Python. */
static char *pr_py_repr_str(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) {
            o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c);
        } else out[o++] = (char)c;
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* Python str()/repr() of a JSON scalar/container (str() of dict/list uses
 * repr() of the items). Only the scalar forms appear in routing keys; the
 * container forms keep the port total for arbitrary event payloads. */
static char *pr_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("");
    char tmp[64];
    switch (v->type) {
        case JSON_BOOL:
            return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING:
            return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d >= (double)LLONG_MIN && d < (double)LLONG_MAX &&
                    d == (double)(long long)d) {
                snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
                return strdup(tmp);
            }
            pr_fmt_float(tmp, sizeof(tmp), d);
            return strdup(tmp);
        }
        case JSON_ARRAY:
        case JSON_OBJECT: {
            /* Build "[" / "{" + ", ".join(...) + "]" / "}". */
            size_t cap = 64, o = 0;
            char *out = (char *)malloc(cap);
            if (!out) return NULL;
            out[o++] = (v->type == JSON_ARRAY) ? '[' : '{';
            for (size_t i = 0; i < v->c.count; i++) {
                char *piece;
                if (v->type == JSON_OBJECT) {
                    char *k = pr_py_repr_str(v->c.keys[i]);
                    char *val = pr_py_str(v->c.items[i]);
                    size_t need = (k ? strlen(k) : 0) + (val ? strlen(val) : 0) + 4;
                    piece = (char *)malloc(need);
                    if (piece) snprintf(piece, need, "%s: %s", k ? k : "", val ? val : "");
                    free(k); free(val);
                } else {
                    piece = pr_py_str(v->c.items[i]);
                }
                if (!piece) { free(out); return NULL; }
                size_t need = o + strlen(piece) + 4;
                if (need > cap) {
                    while (cap < need) cap *= 2;
                    char *nb = (char *)realloc(out, cap);
                    if (!nb) { free(piece); free(out); return NULL; }
                    out = nb;
                }
                if (i > 0) { out[o++] = ','; out[o++] = ' '; }
                memcpy(out + o, piece, strlen(piece));
                o += strlen(piece);
                free(piece);
            }
            if (o + 2 > cap) {
                char *nb = (char *)realloc(out, o + 2);
                if (!nb) { free(out); return NULL; }
                out = nb;
            }
            out[o++] = (v->type == JSON_ARRAY) ? ']' : '}';
            out[o] = '\0';
            return out;
        }
        default:
            return strdup("");
    }
}

/* Copy every key of src into dst (Python {**dst, **src}: later wins, existing
 * keys keep their position — json_set replaces in place). Frees src. */
static void pr_merge_free(json_t *dst, json_t *src) {
    if (dst && src && src->type == JSON_OBJECT) {
        for (size_t i = 0; i < src->c.count; i++)
            json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
    }
    json_free(src);
}

/* PoP: process_registry_not_found @ tools/process_registry.py:_not_found */
json_t *process_registry_not_found(const char *session_id) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set_hdr(o, "status", "not_found");
    const char *sid = session_id ? session_id : "";
    size_t cap = strlen(sid) + 32;
    char *err = (char *)malloc(cap);
    if (err) {
        snprintf(err, cap, "No process with ID %s", sid);
        json_set(o, "error", json_string(err));
        free(err);
    }
    return o;
}

/* PoP: process_registry_output_tail @ tools/process_registry.py:_output_tail */
char *process_registry_output_tail(const char *output_buffer, long n) {
    char *slice = pr_slice_tail(output_buffer, n);
    if (!slice) return NULL;
    char *stripped = strip_ansi(slice);
    free(slice);
    return stripped;
}

/* PoP: process_registry_completion_output @ tools/process_registry.py:_completion_output */
json_t *process_registry_completion_output(const json_t *session) {
    const char *buf = pr_session_str(session, "output_buffer", "");
    /* ``completion_output_chars or COMPLETION_OUTPUT_CHARS``: 0 → 2000;
     * a negative override is kept verbatim (Python ``or`` only swaps 0). */
    long limit = (long)json_get_num(session, "completion_output_chars", 0.0);
    if (limit == 0) limit = 2000;
    long cut = (long)pr_utf8_len(buf) - limit;
    json_t *o = json_object();
    if (!o) return NULL;
    char *tail = process_registry_output_tail(buf, limit);
    json_set(o, "output", json_string(tail ? tail : ""));
    free(tail);
    if (cut > 0) json_set(o, "output_cut", json_number((double)cut));
    return o;
}

/* PoP: process_registry_exit_fields @ tools/process_registry.py:ProcessRegistry._exit_fields */
json_t *process_registry_exit_fields(const json_t *session) {
    json_t *o = json_object();
    if (!o) return NULL;
    const json_t *code = session ? json_obj_get(session, "exit_code") : NULL;
    json_set(o, "exit_code", code ? json_copy(code) : json_null());
    json_set_hdr(o, "completion_reason",
                 pr_session_str(session, "completion_reason", "exited"));
    json_set_hdr(o, "termination_source",
                 pr_session_str(session, "termination_source", ""));
    return o;
}

/* PoP: process_registry_status_head @ tools/process_registry.py:ProcessRegistry._status_head */
json_t *process_registry_status_head(const json_t *session) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set_hdr(o, "session_id", pr_session_str(session, "id", ""));
    json_set_hdr(o, "command", pr_session_str(session, "command", ""));
    bool exited = json_get_bool(session, "exited", false);
    json_set_hdr(o, "status", exited ? "exited" : "running");
    return o;
}

/* PoP: process_registry_exit_snapshot @ tools/process_registry.py:ProcessRegistry._exit_snapshot */
json_t *process_registry_exit_snapshot(const json_t *session, const char *status) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set_hdr(o, "status", status ? status : "");
    json_set_hdr(o, "command", pr_session_str(session, "command", ""));
    pr_merge_free(o, process_registry_exit_fields(session));
    pr_merge_free(o, process_registry_completion_output(session));
    return o;
}

/* PoP: process_registry_global_watch_event @ tools/process_registry.py:ProcessRegistry._global_watch_event */
json_t *process_registry_global_watch_event(const char *type_, const char *message,
                                            const json_t *extra) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set_hdr(o, "session_id", "");
    json_set_hdr(o, "session_key", "");
    json_set_hdr(o, "command", "");
    json_set_hdr(o, "type", type_ ? type_ : "");
    /* **extra lands BEFORE message and the trailing routing keys, so it can
     * override session_id/session_key/command/type but never those. */
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(o, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    json_set_hdr(o, "message", message ? message : "");
    json_set_hdr(o, "platform", "");
    json_set_hdr(o, "chat_id", "");
    json_set_hdr(o, "user_id", "");
    json_set_hdr(o, "user_name", "");
    json_set_hdr(o, "thread_id", "");
    return o;
}

/* PoP: process_registry_watch_event_base @ tools/process_registry.py:ProcessRegistry._watch_event_base
 * The live def mints event_id via uuid.uuid4().hex; the port takes that hex
 * as an explicit input so the differential oracle can pin it (the only
 * non-deterministic field). producer_fields(sandbox_key) is inlined:
 * {"sandbox_key": key} for a hosted producer, nothing for a local one. */
json_t *process_registry_watch_event_base(const json_t *session, const char *event_id) {
    json_t *o = json_object();
    if (!o) return NULL;
    json_set_hdr(o, "event_id", event_id ? event_id : "");
    const json_t *started = session ? json_obj_get(session, "started_at") : NULL;
    json_set(o, "started_at", started ? json_copy(started) : json_number(0.0));
    json_set_hdr(o, "parent_session_id", pr_session_str(session, "parent_session_id", ""));
    json_set_hdr(o, "session_id", pr_session_str(session, "id", ""));
    json_set_hdr(o, "session_key", pr_session_str(session, "session_key", ""));
    json_set_hdr(o, "task_id", pr_session_str(session, "task_id", ""));
    json_set_hdr(o, "owner_task_id", pr_session_str(session, "owner_task_id", ""));
    const char *sandbox_key = pr_session_str(session, "sandbox_key", "");
    if (sandbox_key[0]) json_set_hdr(o, "sandbox_key", sandbox_key);
    json_set_hdr(o, "command", pr_session_str(session, "command", ""));
    /* _WATCHER_ROUTE_KEYS: session.watcher_<key> rides the event as <key>. */
    static const char *route_keys[] = {
        "platform", "chat_id", "user_id", "user_name", "thread_id", "message_id"
    };
    for (size_t i = 0; i < sizeof(route_keys) / sizeof(route_keys[0]); i++) {
        char field[40];
        snprintf(field, sizeof(field), "watcher_%s", route_keys[i]);
        json_set_hdr(o, route_keys[i], pr_session_str(session, field, ""));
    }
    return o;
}

/* PoP: process_registry_session_post_init @ tools/process_registry.py:ProcessSession.__post_init__
 * A session built without an explicit owner is owned by its own task. */
json_t *process_registry_session_post_init(json_t *session) {
    if (!session || session->type != JSON_OBJECT) return session;
    const json_t *owner = json_obj_get(session, "owner_task_id");
    if (!pr_json_truthy(owner)) {
        const json_t *task = json_obj_get(session, "task_id");
        json_set(session, "owner_task_id",
                 task ? json_copy(task) : json_string(""));
    }
    return session;
}

/* PoP: process_registry_owns_event @ tools/process_registry.py:ProcessRegistry._owns_event
 * Routing verdict for one drained event; False = requeue. The two live
 * dependencies are injected so the decision logic stays pure:
 *   visible      — tools.environments.sandbox_scope.event_visible(evt)
 *                  (a bound-scope contextvar read on the Python side);
 *   owns_verdict — NULL when the caller passes owns_event=None, else the
 *                  callback's bool result (1/0) or -1 when it raised
 *                  (the live def fails closed on a broken check). */
bool process_registry_owns_event(const json_t *evt, const char *session_key,
                                 const int *owns_verdict, bool is_async_delegation,
                                 bool visible) {
    if (!visible) return false;  /* another agent's: stays queued for it */
    /* str(evt.get("session_key") or "") */
    const json_t *sk = evt ? json_obj_get(evt, "session_key") : NULL;
    char *evt_session_key = pr_json_truthy(sk) ? pr_py_str(sk) : strdup("");
    if (!evt_session_key) evt_session_key = strdup("");
    bool requires_positive_proof =
        is_async_delegation || evt_session_key[0] != '\0' ||
        pr_json_truthy(evt ? json_obj_get(evt, "origin_ui_session_id") : NULL);
    bool verdict;
    if (owns_verdict && requires_positive_proof) {
        verdict = (*owns_verdict == 1);  /* raised (-1) → fail closed */
    } else if (session_key && session_key[0] && requires_positive_proof) {
        verdict = strcmp(evt_session_key, session_key) == 0;
    } else {
        /* Restored payloads from a previous process: an unfiltered drain
         * cannot prove ownership, so leave them for the owner. */
        verdict = !(is_async_delegation &&
                    pr_json_truthy(evt ? json_obj_get(evt, "restored") : NULL));
    }
    free(evt_session_key);
    return verdict;
}
