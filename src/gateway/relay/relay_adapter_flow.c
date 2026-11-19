/*
 * relay_adapter_flow.c — relay routing/forwarding FLOW cores of
 * gateway/relay/adapter.py (LIVE authority).
 *
 * Ports decision sequences: call order, branch conditions, early returns,
 * exception->fallback paths and mutation order. Every live effect (transport
 * sends, descriptor negotiation, redial toggles, logging, the session-key
 * namespace resolver) enters through the seam structs declared in
 * relay_adapter_flow.h; the cores below perform no I/O.
 *
 * Python semantics honored: truthiness of dict.get results, `or` fallbacks
 * vs `is None` identity checks, short-circuit evaluation order (seam calls
 * happen exactly when live would call them), str() of scalars, dict
 * insertion order, setdefault presence semantics.
 *
 * The egress-decline classification mirrors gateway/relay/egress.py
 * (is_egress_decline / decline_error / log_decline); the oracle drives the
 * LIVE egress functions, so any drift surfaces as a trace mismatch. The
 * _coerce_flag / _relay_platform_extra / _format_hints / _with_scope /
 * _with_status_thread_anchor chains mirror the already-ported adapter
 * helpers (relay_adapter*.c) and are verified against the LIVE defs.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "relay_adapter_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── internal helpers (no Python counterpart; semantics documented inline) ── */

/* Python truthiness for a JSON value (NULL pointer == Python None). */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* Shortest round-trip decimal (Python repr/str of float). */
static void py_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

/* Python str() for scalars. Containers serialize as JSON (fixtures keep
 * str()-converted fields scalar, matching the live call sites). */
static char *jpystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                py_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* str(x or "") — falsy collapses to the empty string BEFORE str(). */
static char *jpystr_or_empty(const json_t *v) {
    if (!jtruth(v)) return strdup("");
    return jpystr(v);
}

/* ASCII lower/strip (fixtures carry ASCII platform names and YAML flags;
 * Python's Unicode-aware variants agree on that domain). */
static char *str_lower(const char *s) {
    char *out = strdup(s ? s : "");
    if (!out) return NULL;
    for (char *p = out; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    return out;
}

static bool is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *str_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b = a + strlen(a);
    while (a < b && is_py_ws(*a)) a++;
    while (b > a && is_py_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static bool str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* dict.get(key) -> borrowed value or NULL (None/absent). */
static const json_t *jget(const json_t *obj, const char *key) {
    return json_obj_get(obj, key);
}

static const json_t *state_map(const json_t *state, const char *key) {
    const json_t *m = jget(state, key);
    return (m && m->type == JSON_OBJECT) ? m : NULL;
}

/* chat cache lookup returning a borrowed string value or NULL. */
static const char *cache_str(const json_t *state, const char *map,
                             const char *chat_id) {
    const json_t *m = state_map(state, map);
    const json_t *v = m ? jget(m, chat_id) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static const json_t *state_transport(const json_t *state) {
    const json_t *t = jget(state, "transport");
    return (t && t->type == JSON_OBJECT) ? t : NULL;
}

static const json_t *state_descriptor(const json_t *state) {
    const json_t *d = jget(state, "descriptor");
    return (d && d->type == JSON_OBJECT) ? d : NULL;
}

/* A transport method attribute reads as callable only when modeled as the
 * {"callable": true} marker object (header convention). */
static bool attr_callable(const json_t *transport, const char *name) {
    const json_t *a = transport ? jget(transport, name) : NULL;
    return a && a->type == JSON_OBJECT && json_get_bool(a, "callable", false);
}

/* ── trace recorder ── */
void radapter_flow_trace_add(radapter_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void radapter_flow_trace_free(radapter_trace_t *t) {
    if (!t) return;
    json_free(t->events);
    t->events = NULL;
}

/* ── egress mirror (gateway/relay/egress.py; oracle drives the LIVE defs) ── */

static const char *EGRESS_DECLINE_CODE = "egress_declined";
static const char *EGRESS_DECLINE_MARKER = "egress declined:";

static bool is_egress_decline(const json_t *result) {
    if (!result || result->type != JSON_OBJECT) return false;
    if (jtruth(jget(result, "success"))) return false;
    if (jtruth(jget(result, "ambiguous"))) return false;
    char *code = jpystr_or_empty(jget(result, "code"));
    bool by_code = str_eq(code, EGRESS_DECLINE_CODE);
    free(code);
    if (by_code) return true;
    char *err = jpystr_or_empty(jget(result, "error"));
    char *low = str_lower(err);
    free(err);
    bool by_text = low && strstr(low, EGRESS_DECLINE_MARKER) != NULL;
    free(low);
    return by_text;
}

static char *decline_error(const json_t *result) {
    if (result && result->type == JSON_OBJECT) {
        const json_t *error = jget(result, "error");
        if (jtruth(error)) return jpystr(error);
    }
    return strdup("relay egress declined");
}

/* log_decline: WARNING "relay %s DECLINED for %s: %s" (op, chat, decline_error). */
static void log_decline(const radapter_op_seams_t *s, const char *op,
                        const char *chat, const json_t *result) {
    char *de = decline_error(result);
    size_t cap = strlen(op) + strlen(chat) + strlen(de) + 64;
    char *msg = malloc(cap);
    if (msg) {
        snprintf(msg, cap, "relay %s DECLINED for %s: %s", op, chat, de);
        if (s && s->log_warning) s->log_warning(s->ctx, msg);
        free(msg);
    }
    free(de);
}

/* ── descriptor / config mirrors ── */

/* CapabilityDescriptor.supports_op: op in (supported_ops or LEGACY_OPS). */
static bool supports_op(const json_t *desc, const char *op) {
    static const char *legacy[] = {"send", "edit", "typing", "follow_up"};
    if (!desc) return false;
    const json_t *ops = jget(desc, "supported_ops");
    if (ops && ops->type == JSON_ARRAY && ops->c.count > 0) {
        for (size_t i = 0; i < ops->c.count; i++) {
            const json_t *o = ops->c.items[i];
            if (o && o->type == JSON_STRING && str_eq(o->str_val, op))
                return true;
        }
        return false;
    }
    for (size_t i = 0; i < sizeof(legacy) / sizeof(legacy[0]); i++)
        if (str_eq(legacy[i], op)) return true;
    return false;
}

/* _relay_platform_extra: extra.get(platform) when it IS a dict, else extra.
 * Returns a BORROWED object, or NULL standing for the empty-dict fallback. */
static const json_t *relay_platform_extra(const json_t *state, const char *platform) {
    const json_t *config = jget(state, "config");
    const json_t *extra = (config && config->type == JSON_OBJECT)
                              ? jget(config, "extra") : NULL;
    if (!jtruth(extra) || extra->type != JSON_OBJECT) {
        /* `or {}` — falsy/absent extra is the empty dict. A truthy non-dict
         * extra would raise live; fixtures keep it dict-or-null. */
        return NULL;   /* callers treat NULL as the empty dict */
    }
    const json_t *sub = jget(extra, platform);
    return (sub && sub->type == JSON_OBJECT) ? sub : extra;
}

/* _coerce_flag: None -> default; bool passthrough; else str().strip().lower()
 * in _TRUTHY ({"1","true","yes","on"}). */
static bool coerce_flag(const json_t *raw, bool dflt) {
    if (!raw || raw->type == JSON_NULL) return dflt;
    if (raw->type == JSON_BOOL) return raw->bool_val;
    char *s = jpystr(raw);
    char *t = str_strip(s);
    free(s);
    char *l = str_lower(t);
    free(t);
    bool r = l && (str_eq(l, "1") || str_eq(l, "true") ||
                   str_eq(l, "yes") || str_eq(l, "on"));
    free(l);
    return r;
}

static bool in_truthy_falsy(const char *l) {
    return str_eq(l, "1") || str_eq(l, "true") || str_eq(l, "yes") || str_eq(l, "on") ||
           str_eq(l, "0") || str_eq(l, "false") || str_eq(l, "no") || str_eq(l, "off");
}

/* _slack_unfurl_hints core over a borrowed extra object (NULL = {}). */
static json_t *slack_unfurl_hints_from_extra(const json_t *extra) {
    static const char *knobs[] = {"unfurl_links", "unfurl_media"};
    json_t *hints = json_object();
    for (size_t i = 0; i < 2; i++) {
        const json_t *val = extra ? jget(extra, knobs[i]) : NULL;
        if (val && val->type == JSON_BOOL) {
            json_set(hints, knobs[i], json_bool(val->bool_val));
        } else if (val && val->type == JSON_STRING) {
            char *t = str_strip(val->str_val);
            char *l = str_lower(t);
            free(t);
            if (l && in_truthy_falsy(l)) {
                bool truthy = str_eq(l, "1") || str_eq(l, "true") ||
                              str_eq(l, "yes") || str_eq(l, "on");
                json_set(hints, knobs[i], json_bool(truthy));
            }
            free(l);
        }
    }
    if (hints->c.count == 0) { json_free(hints); return NULL; }
    return hints;
}

/* _format_hints: destination descriptor bit + operator knobs. NEW object or
 * NULL. Mirrors the already-ported helper; oracle drives the LIVE def. */
static json_t *format_hints(const json_t *state, const json_t *descriptor,
                            const char *platform) {
    if (!descriptor || descriptor->type != JSON_OBJECT) return NULL;
    if (!jtruth(jget(descriptor, "supports_block_formatting"))) return NULL;
    char *low = str_lower(platform ? platform : "");
    const json_t *knob_src = relay_platform_extra(state, low);
    free(low);
    static const char *knobs[] = {"rich_blocks", "markdown_blocks"};
    json_t *hints = json_object();
    for (size_t i = 0; i < 2; i++) {
        const json_t *raw = knob_src ? jget(knob_src, knobs[i]) : NULL;
        if (coerce_flag(raw, false))
            json_set(hints, knobs[i], json_bool(true));
    }
    if (hints->c.count == 0) { json_free(hints); return NULL; }
    return hints;
}

/* _with_scope: copy metadata, attach scope_id/user_id/profile when known and
 * not already truthy. NEW object. */
static json_t *with_scope(const json_t *state, const char *chat_id,
                          const json_t *metadata) {
    json_t *meta = (metadata && metadata->type == JSON_OBJECT)
                       ? json_copy(metadata) : json_object();
    static const struct { const char *key; const char *map; } lanes[] = {
        {"scope_id", "scope_by_chat"},
        {"user_id", "dm_user_by_chat"},
        {"profile", "profile_by_chat"},
    };
    for (size_t i = 0; i < 3; i++) {
        if (jtruth(jget(meta, lanes[i].key))) continue;
        const char *value = cache_str(state, lanes[i].map, chat_id);
        if (value && value[0])
            json_set(meta, lanes[i].key, json_string(value));
    }
    return meta;
}

/* _with_status_thread_anchor: copy metadata; Slack DM without a thread anchor
 * gets the turn's anchor (or the chat's latest inbound ts). NEW object. */
static json_t *with_status_thread_anchor(const json_t *state, const char *chat_id,
                                         const json_t *metadata, const char *anchor) {
    json_t *md = (metadata && metadata->type == JSON_OBJECT)
                     ? json_copy(metadata) : json_object();
    bool has_thread = jtruth(jget(md, "thread_id")) || jtruth(jget(md, "thread_ts"));
    const char *plat = cache_str(state, "platform_by_chat", chat_id);
    const char *ctype = cache_str(state, "chat_type_by_chat", chat_id);
    if (!has_thread && str_eq(plat, "slack") && str_eq(ctype, "dm")) {
        const char *a = (anchor && anchor[0])
                            ? anchor
                            : cache_str(state, "last_inbound_ts_by_chat", chat_id);
        if (a && a[0])
            json_set(md, "thread_id", json_string(a));
    }
    return md;
}

/* _URL_RE = r"https?://|<https?:|\]\(https?:" — substring search suffices
 * (no anchors, no capture). */
static bool url_re_search(const char *s) {
    if (!s) return false;
    return strstr(s, "http://") || strstr(s, "https://") ||
           strstr(s, "<http:") || strstr(s, "<https:") ||
           strstr(s, "](http:") || strstr(s, "](https:");
}

/* ── 1. _utf16_len ── */
/* PoP: radapter_flow_utf16_len @ gateway/relay/adapter.py:_utf16_len */
int radapter_flow_utf16_len(const char *text) {
    if (!text) return 0;
    int units = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        unsigned char c = *p;
        unsigned long cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { cp = c; extra = 0; }   /* invalid lead: count one unit */
        p++;
        for (int i = 0; i < extra && *p; i++) {
            if ((*p & 0xC0) != 0x80) break;
            cp = (cp << 6) | (*p & 0x3F);
            p++;
        }
        units += (cp > 0xFFFF) ? 2 : 1;
    }
    return units;
}

/* ── 2. _send_result ── */
/* PoP: radapter_flow_send_result @ gateway/relay/adapter.py:_send_result */
json_t *radapter_flow_send_result(const json_t *result, const json_t *extra) {
    json_t *out = json_object();
    /* SendResult dataclass field order (defaults included). */
    json_set(out, "success", json_bool(jtruth(jget(result, "success"))));
    const json_t *mid = jget(result, "message_id");
    json_set(out, "message_id", mid ? json_copy(mid) : json_null());
    const json_t *err = jget(result, "error");
    json_set(out, "error", err ? json_copy(err) : json_null());
    json_set(out, "raw_response", json_null());
    json_set(out, "retryable", json_bool(false));
    json_set(out, "retry_after", json_null());
    json_set(out, "continuation_message_ids", json_array());
    json_set(out, "error_kind", json_null());
    /* **extra overrides (json_set replaces in place, preserving position). */
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    return out;
}

/* ── 3. _event_ids ── */
/* PoP: radapter_flow_event_ids @ gateway/relay/adapter.py:_event_ids */
json_t *radapter_flow_event_ids(const json_t *event) {
    const json_t *src = jget(event, "source");
    if (src && src->type != JSON_OBJECT) src = NULL;
    const json_t *mid = jget(event, "message_id");
    if (!jtruth(mid))
        mid = src ? jget(src, "message_id") : NULL;
    const json_t *chat = src ? jget(src, "chat_id") : NULL;
    json_t *out = json_array();
    json_append(out, mid ? json_copy(mid) : json_null());
    json_append(out, chat ? json_copy(chat) : json_null());
    return out;
}

/* ── 4. _profile_from_session_key ── */
/* PoP: radapter_flow_profile_from_session_key @ gateway/relay/adapter.py:_profile_from_session_key */
char *radapter_flow_profile_from_session_key(const char *session_key,
                                             const radapter_profile_seams_t *s) {
    const char *k = session_key ? session_key : "";
    const char *c1 = strchr(k, ':');
    if (!c1) return NULL;                    /* len(parts) < 2 */
    size_t ns_len = (size_t)(c1 - k);
    if (ns_len != 5 || strncmp(k, "agent", 5) != 0) return NULL;  /* parts[0] != "agent" */
    const char *ns = c1 + 1;
    const char *c2 = strchr(ns, ':');
    size_t ns_size = c2 ? (size_t)(c2 - ns) : strlen(ns);
    if (ns_size == 0) return NULL;           /* not parts[1] */
    char *ns_buf = malloc(ns_size + 1);
    if (!ns_buf) return NULL;
    memcpy(ns_buf, ns, ns_size);
    ns_buf[ns_size] = '\0';
    const char *profile = (s && s->resolve_namespace)
                              ? s->resolve_namespace(s->ctx, ns_buf) : NULL;
    free(ns_buf);
    if (!profile) return NULL;
    if (strcmp(profile, "default") == 0) return NULL;
    return strdup(profile);
}

/* ── 5. _negotiated_descriptor ── */
/* PoP: radapter_flow_negotiated_descriptor @ gateway/relay/adapter.py:_negotiated_descriptor */
json_t *radapter_flow_negotiated_descriptor(const json_t *state,
                                            const char *platform,
                                            const radapter_desc_seams_t *s) {
    /* `if platform` — truthiness gate BEFORE the getattr (no seam call). */
    if (!platform || !platform[0]) return NULL;
    const json_t *transport = state_transport(state);
    if (!attr_callable(transport, "descriptor_for_platform")) return NULL;
    bool raised = false;
    const json_t *r = (s && s->resolve) ? s->resolve(s->ctx, platform, &raised) : NULL;
    if (raised) return NULL;                 /* except Exception -> None */
    return r ? json_copy(r) : NULL;
}

/* ── 6. _metrics_platform ── */
/* PoP: radapter_flow_metrics_platform @ gateway/relay/adapter.py:_metrics_platform */
json_t *radapter_flow_metrics_platform(const json_t *state, const char *chat_id) {
    const char *cached = cache_str(state, "platform_by_chat", chat_id);
    if (cached && cached[0]) return json_string(cached);   /* `or` truthiness */
    /* fronted = {p for p, _ in (transport._identities or ())} — distinct set. */
    const json_t *transport = state_transport(state);
    const json_t *ids = transport ? jget(transport, "_identities") : NULL;
    size_t distinct = 0;
    const char *seen[64];
    if (ids && ids->type == JSON_ARRAY) {
        for (size_t i = 0; i < ids->c.count; i++) {
            const json_t *pair = ids->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count < 1) continue;
            const json_t *p = pair->c.items[0];
            if (!p || p->type != JSON_STRING) continue;
            bool dup = false;
            for (size_t j = 0; j < distinct; j++)
                if (str_eq(seen[j], p->str_val)) { dup = true; break; }
            if (!dup && distinct < 64) seen[distinct++] = p->str_val;
        }
    }
    if (distinct <= 1) {
        const json_t *desc = state_descriptor(state);
        const json_t *plat = desc ? jget(desc, "platform") : NULL;
        return plat ? json_copy(plat) : NULL;
    }
    return NULL;
}

/* ── 7. _slack_unfurl_hints ── */
/* PoP: radapter_flow_slack_unfurl_hints @ gateway/relay/adapter.py:_slack_unfurl_hints */
json_t *radapter_flow_slack_unfurl_hints(const json_t *state, const char *platform) {
    char *low = str_lower(platform ? platform : "");
    bool slack = str_eq(low, "slack");
    free(low);
    if (!slack) return NULL;
    const json_t *extra = relay_platform_extra(state, "slack");
    return slack_unfurl_hints_from_extra(extra);
}

/* ── 8. prefers_fresh_final_streaming ── */
/* PoP: radapter_flow_prefers_fresh_final_streaming @ gateway/relay/adapter.py:prefers_fresh_final_streaming */
bool radapter_flow_prefers_fresh_final_streaming(const json_t *state,
                                                 const char *content,
                                                 const json_t *metadata,
                                                 const json_t *chat_id) {
    const json_t *platform = NULL;
    if (chat_id) {
        /* chat_id is not None: cache lookup (a stored "" is NOT None). */
        char *key = jpystr(chat_id);
        const json_t *m = state_map(state, "platform_by_chat");
        platform = m ? jget(m, key) : NULL;
        free(key);
        if (platform && platform->type == JSON_NULL) platform = NULL;
    }
    if (!platform && metadata && metadata->type == JSON_OBJECT) {
        platform = jget(metadata, "platform");
        if (platform && platform->type == JSON_NULL) platform = NULL;
    }
    if (!platform) {
        const json_t *desc = state_descriptor(state);
        platform = desc ? jget(desc, "platform") : NULL;
    }
    char *pstr = platform ? jpystr(platform) : NULL;
    json_t *hints = radapter_flow_slack_unfurl_hints(state, pstr);
    free(pstr);
    if (!hints) return false;
    bool any_true = false;
    for (size_t i = 0; i < hints->c.count; i++) {
        const json_t *v = hints->c.items[i];
        if (v && v->type == JSON_BOOL && v->bool_val) { any_true = true; break; }
    }
    json_free(hints);
    if (!any_true) return false;
    return url_re_search(content ? content : "");
}

/* ── 9. _gated_op ── */
/* PoP: radapter_flow_gated_op @ gateway/relay/adapter.py:_gated_op */
json_t *radapter_flow_gated_op(const json_t *state, const char *chat_id,
                               const json_t *action, int decline_level,
                               const json_t *subject, const char *platform,
                               bool surface_declines, bool surface_ambiguous,
                               const radapter_op_seams_t *s) {
    const json_t *op_v = jget(action, "op");
    char *op = jpystr(op_v);
    const json_t *transport = state_transport(state);
    const json_t *desc = state_descriptor(state);
    if (!transport || !supports_op(desc, op)) { free(op); return NULL; }
    /* platform or self._platform_by_chat.get(str(chat_id)) — `or` fallback. */
    const char *eff = (platform && platform[0])
                          ? platform
                          : cache_str(state, "platform_by_chat", chat_id);
    bool raised = false;
    const json_t *result = s->send_outbound(s->ctx, action, eff, &raised);
    if (raised) {
        size_t cap = strlen(op) + 48;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "relay %s transport failure", op);
            if (s->log_debug) s->log_debug(s->ctx, msg);
            free(msg);
        }
        free(op);
        return NULL;
    }
    if (!result || result->type != JSON_OBJECT) { free(op); return NULL; }
    if (jtruth(jget(result, "success"))) { free(op); return json_copy(result); }
    if (surface_ambiguous && jtruth(jget(result, "ambiguous"))) {
        free(op);
        return json_copy(result);
    }
    char *subj = subject ? jpystr(subject) : strdup(chat_id ? chat_id : "");
    if (is_egress_decline(result)) {
        log_decline(s, op, subj, result);
        free(subj);
        free(op);
        return surface_declines ? json_copy(result) : NULL;
    }
    if (decline_level >= 0) {
        char *err = jpystr(jget(result, "error"));   /* str(None) == "None" */
        size_t cap = strlen(op) + strlen(subj) + strlen(err) + 64;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "relay %s declined for %s: %s", op, subj, err);
            if (s->log_level) s->log_level(s->ctx, decline_level, msg);
            free(msg);
        }
        free(err);
    }
    free(subj);
    free(op);
    return NULL;
}

/* ── 10. _outbound ── */
/* PoP: radapter_flow_outbound @ gateway/relay/adapter.py:_outbound */
json_t *radapter_flow_outbound(const json_t *state, const char *chat_id,
                               const json_t *action, bool *raised,
                               const radapter_op_seams_t *s) {
    if (raised) *raised = false;
    const json_t *op_v = jget(action, "op");
    char *op = op_v ? jpystr(op_v) : strdup("?");   /* str(action.get("op", "?")) */
    const json_t *transport = state_transport(state);
    if (!transport) {
        /* live: None.send_outbound -> AttributeError (propagates). */
        if (raised) *raised = true;
        free(op);
        return NULL;
    }
    const char *plat = cache_str(state, "platform_by_chat", chat_id);
    bool r = false;
    const json_t *result = s->send_outbound(s->ctx, action, plat, &r);
    if (r) {
        if (raised) *raised = true;
        free(op);
        return NULL;
    }
    if (result && result->type == JSON_OBJECT) {
        if (!jtruth(jget(result, "success")) && is_egress_decline(result))
            log_decline(s, op, chat_id ? chat_id : "", result);
    }
    free(op);
    return result ? json_copy(result) : NULL;
}

/* ── 11. _toggle_transport_redial ── */
/* PoP: radapter_flow_toggle_transport_redial @ gateway/relay/adapter.py:_toggle_transport_redial */
bool radapter_flow_toggle_transport_redial(const json_t *state,
                                           const char *method_name,
                                           const radapter_toggle_seams_t *s) {
    const json_t *transport = state_transport(state);
    if (!attr_callable(transport, method_name)) return false;
    bool raised = false;
    if (s && s->call) s->call(s->ctx, method_name, &raised);
    if (raised) {
        size_t cap = strlen(method_name) + 32;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "relay %s failed", method_name);
            if (s && s->log_debug) s->log_debug(s->ctx, msg);
            free(msg);
        }
        return false;
    }
    return true;
}

/* ── 12. _ack_emoji ── */
/* PoP: radapter_flow_ack_emoji @ gateway/relay/adapter.py:_ack_emoji */
json_t *radapter_flow_ack_emoji(const json_t *state, const json_t *event,
                                const char *chat_id) {
    static const char *dflt[] = {"\xf0\x9f\x91\x80", "\xe2\x9c\x85", "\xe2\x9d\x8c"};
    static const char *telegram[] = {"\xf0\x9f\x91\x80", "\xf0\x9f\x91\x8d", "\xf0\x9f\x91\x8e"};
    const json_t *src = jget(event, "source");
    if (src && src->type != JSON_OBJECT) src = NULL;
    const json_t *candidates[3];
    candidates[0] = src ? jget(src, "platform") : NULL;
    const json_t *m = state_map(state, "platform_by_chat");
    candidates[1] = m ? jget(m, chat_id) : NULL;
    const json_t *desc = state_descriptor(state);
    candidates[2] = desc ? jget(desc, "platform") : NULL;
    for (size_t i = 0; i < 3; i++) {
        const json_t *cand = candidates[i];
        /* getattr(candidate, "value", candidate) — enum double unwrapping. */
        const json_t *inner = cand;
        if (cand && cand->type == JSON_OBJECT) {
            const json_t *v = jget(cand, "value");
            if (v) inner = v;
        }
        char *name = jpystr_or_empty(inner);   /* str(... or "").lower() */
        char *low = str_lower(name);
        free(name);
        bool resolved = low && low[0] &&
                        !str_eq(low, "relay");   /* _ACK_PLATFORM_UNRESOLVED */
        const char **set = dflt;
        if (resolved && str_eq(low, "telegram")) set = telegram;
        free(low);
        if (resolved) {
            json_t *out = json_array();
            for (size_t k = 0; k < 3; k++) json_append(out, json_string(set[k]));
            return out;
        }
    }
    json_t *out = json_array();
    for (size_t k = 0; k < 3; k++) json_append(out, json_string(dflt[k]));
    return out;
}

/* ── 13. _stamp_format_hints ── */
/* PoP: radapter_flow_stamp_format_hints @ gateway/relay/adapter.py:_stamp_format_hints */
json_t *radapter_flow_stamp_format_hints(const json_t *state,
                                         const json_t *descriptor,
                                         const char *platform,
                                         const json_t *metadata) {
    json_t *hints = format_hints(state, descriptor, platform);
    if (!hints)
        return metadata ? json_copy(metadata) : NULL;
    json_t *merged = (metadata && metadata->type == JSON_OBJECT)
                         ? json_copy(metadata) : json_object();
    /* setdefault: presence (even a null value) wins over the stamp. */
    if (json_obj_get(merged, "format_hints")) {
        json_free(hints);
    } else {
        json_set(merged, "format_hints", hints);
    }
    return merged;
}

/* ── 14. _typing_frame ── */
/* PoP: radapter_flow_typing_frame @ gateway/relay/adapter.py:_typing_frame */
void radapter_flow_typing_frame(const json_t *state, const char *chat_id,
                                const json_t *metadata, const json_t *content,
                                const char *lane, const radapter_op_seams_t *s) {
    json_t *md = with_status_thread_anchor(state, chat_id, metadata, NULL);
    json_t *frame = json_object();
    /* Live frame insertion order: op, chat_id, metadata, then content. */
    json_set(frame, "op", json_string("typing"));
    json_set(frame, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(frame, "metadata", with_scope(state, chat_id, md));
    json_free(md);
    if (content && content->type != JSON_NULL)
        json_set(frame, "content", json_copy(content));
    /* _outbound mirror: no op gate; transport None raises AttributeError,
     * caught here -> debug log (cosmetic lane never raises). */
    const json_t *transport = state_transport(state);
    bool failed = false;
    if (!transport) {
        failed = true;
    } else {
        const char *plat = cache_str(state, "platform_by_chat", chat_id);
        bool raised = false;
        const json_t *result = s->send_outbound(s->ctx, frame, plat, &raised);
        if (raised) {
            failed = true;
        } else if (result && result->type == JSON_OBJECT &&
                   !jtruth(jget(result, "success")) && is_egress_decline(result)) {
            log_decline(s, "typing", chat_id ? chat_id : "", result);
        }
    }
    if (failed) {
        size_t cap = strlen(lane ? lane : "") + strlen(chat_id ? chat_id : "") + 48;
        char *msg = malloc(cap);
        if (msg) {
            snprintf(msg, cap, "relay %s failed for %s", lane ? lane : "",
                     chat_id ? chat_id : "");
            if (s->log_debug) s->log_debug(s->ctx, msg);
            free(msg);
        }
    }
    json_free(frame);
}