/*
 * port_gateway_agent_flow.c — gateway agent-bridge FLOW cores of
 * hermes_cli/gateway_agent.py. Decision sequences, call order, branch
 * conditions, state transitions and error paths; all connection/i18n/uuid I/O
 * via the universal seam (gwa_seams_t.call). See the header for the seam table
 * and the injected self-model `st`. No I/O is performed here.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "port_gateway_agent_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* ── internal helpers ── */

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
/* Python `x is not None` (a present, non-null node). */
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }

static char *xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

void gwa_trace_add(gwa_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
void gwa_trace_free(gwa_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
json_t *gwa_call(const gwa_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
const char *gwa_raise_class(const json_t *r) {
    if (is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (is_str(c)) return c->str_val;
    }
    return NULL;
}
bool gwa_caught(const json_t *r, const char *cls) {
    const char *k = gwa_raise_class(r);
    if (!k) return false;
    if (!strcmp(cls, k)) return true;
    if (!strcmp(cls, "GatewayClientError")) return !strcmp(k, "GatewayRPCError");
    return false;
}

static json_t *args0(void) { return json_object(); }
static void aset_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void aset_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* st["state"] borrowed (or NULL). */
static json_t *self_state(json_t *st) { return json_obj_get(st, "state"); }
/* state.info or {} — returns borrowed info object or NULL. */
static const json_t *state_info(json_t *st) {
    json_t *state = self_state(st);
    const json_t *info = state ? json_obj_get(state, "info") : NULL;
    return is_obj(info) ? info : NULL;
}

/* _emit_print: honor suppress_status_output, else print seam. */
static void emit_print(json_t *st, const gwa_seams_t *s, const char *text) {
    if (json_get_bool(st, "suppress_status_output", false)) return;
    json_t *a = args0();
    aset_s(a, "text", text);
    json_t *r = gwa_call(s, "print", a);
    json_free(a);
    json_free(r);
}

/* ── _effort_word ── */
/* PoP: gwa_effort_word @ hermes_cli/gateway_agent.py:_effort_word */
char *gwa_effort_word(const json_t *config) {
    if (!is_obj(config)) return NULL;
    const json_t *en = json_obj_get(config, "enabled");
    if (en && en->type == JSON_BOOL && !en->bool_val) return strdup("none");
    const json_t *eff = json_obj_get(config, "effort");
    if (is_str(eff) && eff->str_val && eff->str_val[0]) return strdup(eff->str_val);
    return NULL;
}

/* ── _route_off_session ── */
/* PoP: gwa_route_off_session @ hermes_cli/gateway_agent.py:_route_off_session */
json_t *gwa_route_off_session(json_t *st, const char *model, const char *provider,
                              const char *alias, const gwa_seams_t *s) {
    const json_t *info = state_info(st);
    const json_t *imodel = info ? json_obj_get(info, "model") : NULL;
    bool model_truthy = model && model[0];
    bool info_model_truthy = jtruth(imodel);
    if (!model_truthy || !info_model_truthy) return NULL;
    /* state.runs_route(model, provider) and not alias */
    json_t *a = args0();
    aset_s(a, "model", model);
    aset_s(a, "provider", provider);
    json_t *rr = gwa_call(s, "runs_route", a);
    json_free(a);
    bool runs = jtruth(rr);
    json_free(rr);
    bool alias_truthy = alias && alias[0];
    if (runs && !alias_truthy) return NULL;
    json_t *out = json_object();
    json_set(out, "model", json_string(alias_truthy ? alias : model));
    if (provider && provider[0]) json_set(out, "provider", json_string(provider));
    return out;
}

/* ── _set_reasoning ── */
/* PoP: gwa_set_reasoning @ hermes_cli/gateway_agent.py:_set_reasoning */
bool gwa_set_reasoning(json_t *st, const json_t *config, bool explicit_, const gwa_seams_t *s) {
    json_set(st, "_reasoning_config", config ? json_copy(config) : json_null());
    char *effort = gwa_effort_word(config);
    if (!effort) return false;
    const json_t *info = state_info(st);
    const json_t *cur = info ? json_obj_get(info, "reasoning_effort") : NULL;
    if (!explicit_ && is_str(cur) && cur->str_val && !strcmp(cur->str_val, effort)) {
        free(effort);
        return true;
    }
    const char *sid = json_get_str(st, "session_id", "");
    bool ok = false;
    /* snapshot = connection.call("session.resume", session_id=...) */
    json_t *ra = args0();
    aset_s(ra, "method", "session.resume");
    aset_s(ra, "session_id", sid);
    json_t *snapshot = gwa_call(s, "call", ra);
    json_free(ra);
    if (gwa_caught(snapshot, "GatewayClientError")) { json_free(snapshot); free(effort); return false; }
    /* request_id = uuid.uuid4().hex — Python evaluates the session.mutate kwargs
     * left-to-right, so the uuid seam fires BEFORE snapshot["revision"] /
     * snapshot["execution_generation"] are read (a missing fence key raises
     * KeyError only after the uuid call). */
    json_t *ua = args0();
    json_t *ur = gwa_call(s, "uuid4_hex", ua);
    json_free(ua);
    if (gwa_caught(ur, "GatewayClientError")) { json_free(ur); json_free(snapshot); free(effort); return false; }
    const char *rid = jstr(ur);
    char *rid_free = rid ? strdup(rid) : NULL;
    json_free(ur);
    /* snapshot["revision"], snapshot["execution_generation"] — KeyError if absent,
     * TypeError if snapshot is not subscriptable; both caught -> false, no mutate. */
    const json_t *rev = is_obj(snapshot) ? json_obj_get(snapshot, "revision") : NULL;
    const json_t *gen = is_obj(snapshot) ? json_obj_get(snapshot, "execution_generation") : NULL;
    if (!rev || !gen) { json_free(snapshot); free(rid_free); free(effort); return false; }
    json_t *ma = args0();
    aset_s(ma, "method", "session.mutate");
    aset_s(ma, "session_id", sid);
    aset_s(ma, "request_id", rid_free ? rid_free : "");
    aset_j(ma, "expected_revision", rev);
    aset_j(ma, "expected_generation", gen);
    aset_s(ma, "operation", "reasoning");
    json_t *payload = json_object();
    aset_s(payload, "effort", effort);
    json_set(ma, "payload", payload);
    json_t *mr = gwa_call(s, "call", ma);
    json_free(ma);
    free(rid_free);
    if (gwa_caught(mr, "GatewayClientError")) { json_free(mr); json_free(snapshot); free(effort); return false; }
    json_free(mr);
    /* info = self._state().info or {} — a FALSY info (None or empty dict) binds a
     * DETACHED dict, so the post-success write does not persist into state.info;
     * only a truthy (non-empty) info object receives reasoning_effort. */
    json_t *state = self_state(st);
    json_t *winfo = state ? json_obj_get(state, "info") : NULL;
    if (jtruth(winfo)) json_set(winfo, "reasoning_effort", json_string(effort));
    ok = true;
    json_free(snapshot);
    free(effort);
    return ok;
}

/* ── _sync_show_reasoning ── */
/* PoP: gwa_sync_show_reasoning @ hermes_cli/gateway_agent.py:_sync_show_reasoning */
void gwa_sync_show_reasoning(json_t *st, const gwa_seams_t *s) {
    bool show = jpresent(json_obj_get(st, "_reasoning_callback"));
    const char *sid = json_get_str(st, "session_id", "");
    double epoch = json_get_num(st, "connection_epoch", 0);
    const json_t *sent = json_obj_get(st, "_show_reasoning_sent");
    if (is_arr(sent) && sent->c.count >= 3) {
        const json_t *s0 = sent->c.items[0];
        const json_t *s1 = sent->c.items[1];
        const json_t *s2 = sent->c.items[2];
        bool sid_eq = is_str(s0) && s0->str_val && !strcmp(s0->str_val, sid);
        bool show_eq = s1 && s1->type == JSON_BOOL && s1->bool_val == show;
        bool epoch_eq = s2 && s2->type == JSON_NUMBER && s2->num_val == epoch;
        if (sid_eq && show_eq && (!show || epoch_eq)) return;
    }
    json_t *a = args0();
    aset_s(a, "method", "config.set");
    aset_s(a, "session_id", sid);
    aset_s(a, "key", "show_reasoning");
    aset_s(a, "value", show ? "1" : "0");
    aset_n(a, "timeout", 10);
    json_t *r = gwa_call(s, "call", a);
    json_free(a);
    if (gwa_caught(r, "GatewayClientError")) { json_free(r); return; }
    json_free(r);
    json_t *tuple = json_array();
    json_append(tuple, json_string(sid));
    json_append(tuple, json_bool(show));
    json_append(tuple, json_number(epoch));
    json_set(st, "_show_reasoning_sent", tuple);
}

/* ── service_tier setter ── */
/* PoP: gwa_service_tier_set @ hermes_cli/gateway_agent.py:service_tier */
void gwa_service_tier_set(json_t *st, const char *tier, const gwa_seams_t *s) {
    json_set(st, "_service_tier", tier ? json_string(tier) : json_null());
    json_t *state = self_state(st);
    bool tier_truthy = tier && tier[0];
    const char *wanted = tier_truthy ? tier : "normal";
    const json_t *sent = state ? json_obj_get(state, "service_tier_sent") : NULL;
    bool sent_is_none = !jpresent(sent);
    const char *sent_s = jstr(sent);
    bool wanted_eq_sent = sent_s && !strcmp(sent_s, wanted);
    if (wanted_eq_sent || (sent_is_none && !tier_truthy)) return;
    const char *sid = json_get_str(st, "session_id", "");
    json_t *a = args0();
    aset_s(a, "method", "config.set");
    aset_s(a, "session_id", sid);
    aset_s(a, "key", "service_tier");
    aset_s(a, "value", wanted);
    aset_n(a, "timeout", 10);
    json_t *r = gwa_call(s, "call", a);
    json_free(a);
    if (gwa_caught(r, "GatewayClientError")) { json_free(r); return; }
    json_free(r);
    if (is_obj(state)) json_set(state, "service_tier_sent", json_string(wanted));
}

/* ── _correct ── */
/* PoP: gwa_correct @ hermes_cli/gateway_agent.py:_correct */
json_t *gwa_correct(json_t *st, const char *verb, const char *text, const gwa_seams_t *s) {
    const json_t *gen = json_obj_get(st, "_generation");
    if (!jpresent(gen)) return json_bool(false);
    const char *sid = json_get_str(st, "session_id", "");
    char *method = xsf("session.%s", verb);
    json_t *a = args0();
    aset_s(a, "method", method);
    aset_s(a, "session_id", sid);
    aset_s(a, "text", text);
    aset_j(a, "execution_generation", gen);
    json_t *result = gwa_call(s, "call", a);
    json_free(a);
    free(method);
    if (gwa_caught(result, "GatewayClientError")) { json_free(result); return json_bool(false); }
    const json_t *res = is_obj(result) ? result : NULL;
    const json_t *vs = res ? json_obj_get(res, "voice_stopped") : NULL;
    if (vs && vs->type == JSON_BOOL && vs->bool_val) {
        json_t *ta = args0();
        aset_s(ta, "key", "cli.voice.stop_phrase_typed");
        json_t *tr = gwa_call(s, "t", ta);
        json_free(ta);
        const char *ts = jstr(tr);
        char *tf = ts ? strdup(ts) : NULL;
        json_free(tr);
        emit_print(st, s, tf ? tf : "");
        free(tf);
        json_free(result);
        return json_string("voice_stopped");
    }
    const json_t *status = res ? json_obj_get(res, "status") : NULL;
    bool accepted = is_str(status) && status->str_val &&
                    (!strcmp(status->str_val, "queued") || !strcmp(status->str_val, "redirected"));
    json_free(result);
    return json_bool(accepted);
}

/* ── _take_turn_route ── */
/* PoP: gwa_take_turn_route @ hermes_cli/gateway_agent.py:_take_turn_route */
json_t *gwa_take_turn_route(json_t *st) {
    json_t *pending = json_obj_get(st, "_turn_route");
    json_t *reasoning = json_obj_get(st, "_turn_reasoning");
    json_t *pending_copy = pending ? json_copy(pending) : NULL;
    json_t *reasoning_copy = reasoning ? json_copy(reasoning) : NULL;
    json_set(st, "_turn_route", json_null());
    json_set(st, "_turn_reasoning", json_null());
    json_set(st, "_taken_turn_route", json_null());
    if (!jtruth(pending_copy)) {
        json_free(pending_copy);
        json_free(reasoning_copy);
        return NULL;
    }
    json_t *taken = json_array();
    json_append(taken, pending_copy);
    json_append(taken, reasoning_copy ? reasoning_copy : json_null());
    json_set(st, "_taken_turn_route", taken);
    json_t *out = json_copy(pending_copy);
    if (jtruth(reasoning_copy)) json_set(out, "reasoning", json_copy(reasoning_copy));
    /* reasoning_copy is owned by `taken` (json_append takes ownership); do not free it here. */
    return out;
}

/* ── _give_back_turn_route ── */
/* PoP: gwa_give_back_turn_route @ hermes_cli/gateway_agent.py:_give_back_turn_route */
void gwa_give_back_turn_route(json_t *st) {
    json_t *taken = json_obj_get(st, "_taken_turn_route");
    json_t *taken_copy = taken ? json_copy(taken) : NULL;
    json_set(st, "_taken_turn_route", json_null());
    json_t *cur = json_obj_get(st, "_turn_route");
    if (jtruth(taken_copy) && !jpresent(cur)) {
        if (is_arr(taken_copy) && taken_copy->c.count >= 2) {
            json_set(st, "_turn_route", json_copy(taken_copy->c.items[0]));
            json_set(st, "_turn_reasoning", json_copy(taken_copy->c.items[1]));
        }
    }
    json_free(taken_copy);
}

/* ── _lost_turn_notice (internal) ── */
static char *lost_turn_notice(json_t *st, const gwa_seams_t *s) {
    json_t *state = self_state(st);
    const json_t *pending = state ? json_obj_get(state, "pending") : NULL;
    bool any_unknown = false;
    if (is_arr(pending)) {
        for (size_t i = 0; i < pending->c.count; i++) {
            const json_t *row = pending->c.items[i];
            const json_t *status = is_obj(row) ? json_obj_get(row, "status") : NULL;
            if (is_str(status) && status->str_val && !strcmp(status->str_val, "unknown")) {
                any_unknown = true;
                break;
            }
        }
    }
    if (!any_unknown) return strdup("");
    json_t *a = args0();
    aset_j(a, "pending", pending);
    json_t *r = gwa_call(s, "pending_lines", a);
    json_free(a);
    /* "\n".join(lines) */
    size_t cap = 1;
    if (is_arr(r)) for (size_t i = 0; i < r->c.count; i++) {
        const char *t = jstr(json_get(r, i));
        cap += (t ? strlen(t) : 0) + 1;
    }
    char *joined = malloc(cap);
    joined[0] = '\0';
    if (is_arr(r)) for (size_t i = 0; i < r->c.count; i++) {
        if (i) strcat(joined, "\n");
        const char *t = jstr(json_get(r, i));
        strcat(joined, t ? t : "");
    }
    json_free(r);
    return joined;
}

/* ── _prepare_submit ── */
/* PoP: gwa_prepare_submit @ hermes_cli/gateway_agent.py:_prepare_submit */
char *gwa_prepare_submit(json_t *st, const char *turn_session_id, const gwa_seams_t *s) {
    json_t *state = self_state(st);
    const json_t *gaps = state ? json_obj_get(state, "gaps") : NULL;
    const json_t *attached = state ? json_obj_get(state, "attached_gaps") : NULL;
    /* state.gaps != state.attached_gaps (numeric compare; default 0) */
    double g = gaps && gaps->type == JSON_NUMBER ? gaps->num_val : 0;
    double ag = attached && attached->type == JSON_NUMBER ? attached->num_val : 0;
    bool differ = (g != ag);
    if (differ) {
        json_t *a = args0();
        aset_s(a, "session_id", turn_session_id);
        json_t *r = gwa_call(s, "reattach", a);
        json_free(a);
        json_free(r);
    }
    json_t *sa = args0();
    json_t *sr = gwa_call(s, "start", sa);
    json_free(sa);
    json_free(sr);
    gwa_sync_show_reasoning(st, s);
    char *notice = lost_turn_notice(st, s);
    char *out;
    if (notice && notice[0]) out = xsf("Your message was not sent.\n%s", notice);
    else out = strdup("");
    free(notice);
    return out;
}

/* ── _stage_content_parts (internal) ── */
static char *strip_ws(const char *s) {
    if (!s) return strdup("");
    const char *b = s;
    while (*b && (*b == ' ' || *b == '\t' || *b == '\n' || *b == '\r' || *b == '\v' || *b == '\f')) b++;
    const char *e = s + strlen(s);
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\v' || e[-1] == '\f')) e--;
    size_t n = (size_t)(e - b);
    char *out = malloc(n + 1);
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}
/* process one text part: splitlines, drop "[Image attached" lines, join "\n", strip */
static char *clean_text_part(const char *text) {
    const char *src = text ? text : "";
    size_t cap = strlen(src) + 1;
    char *buf = malloc(cap);
    size_t len = 0;
    const char *p = src;
    bool first_line = true;
    while (*p) {
        const char *nl = p;
        while (*nl && *nl != '\n' && *nl != '\r') nl++;
        size_t ln = (size_t)(nl - p);
        bool drop = (ln >= 15 && !strncmp(p, "[Image attached", 15));
        if (!drop) {
            if (!first_line) { buf[len++] = '\n'; }
            memcpy(buf + len, p, ln);
            len += ln;
            first_line = false;
        }
        p = nl;
        if (*p == '\r') { p++; if (*p == '\n') p++; }
        else if (*p == '\n') p++;
    }
    buf[len] = '\0';
    char *stripped = strip_ws(buf);
    free(buf);
    return stripped;
}
/* returns {text: str, attachments: array}; caller frees. */
static json_t *stage_content_parts(json_t *st, const json_t *parts, const gwa_seams_t *s) {
    json_t *texts = json_array();
    json_t *attachments = json_array();
    const char *sid = json_get_str(st, "session_id", "");
    if (is_arr(parts)) {
        for (size_t i = 0; i < parts->c.count; i++) {
            const json_t *part = parts->c.items[i];
            if (!is_obj(part)) continue;
            const json_t *type = json_obj_get(part, "type");
            const char *ty = jstr(type);
            if (ty && !strcmp(ty, "text")) {
                const json_t *tv = json_obj_get(part, "text");
                /* str(part.get("text") or "") — fixtures keep scalar string */
                const char *t = jstr(tv);
                char *cleaned = clean_text_part(t ? t : "");
                json_append(texts, json_string(cleaned));
                free(cleaned);
            } else if (ty && !strcmp(ty, "image_url")) {
                const json_t *iu = json_obj_get(part, "image_url");
                const json_t *url = is_obj(iu) ? json_obj_get(iu, "url") : NULL;
                const char *u = jstr(url);
                if (u && !strncmp(u, "data:image/", 11)) {
                    json_t *a = args0();
                    aset_s(a, "method", "image.attach_bytes");
                    aset_s(a, "session_id", sid);
                    aset_s(a, "content_base64", u);
                    json_t *staged = gwa_call(s, "call", a);
                    json_free(a);
                    const json_t *path = is_obj(staged) ? json_obj_get(staged, "path") : NULL;
                    const json_t *mime = is_obj(staged) ? json_obj_get(staged, "mime") : NULL;
                    json_t *att = json_object();
                    json_set(att, "path", path ? json_copy(path) : json_null());
                    json_set(att, "mime", mime ? json_copy(mime) : json_null());
                    json_append(attachments, att);
                    json_free(staged);
                }
            }
        }
    }
    /* "\n\n".join(t for t in texts if t) */
    size_t cap = 1;
    for (size_t i = 0; i < texts->c.count; i++) {
        const char *t = jstr(texts->c.items[i]);
        if (t && t[0]) cap += strlen(t) + 2;
    }
    char *joined = malloc(cap);
    joined[0] = '\0';
    bool first = true;
    for (size_t i = 0; i < texts->c.count; i++) {
        const char *t = jstr(texts->c.items[i]);
        if (t && t[0]) {
            if (!first) strcat(joined, "\n\n");
            strcat(joined, t);
            first = false;
        }
    }
    json_free(texts);
    json_t *out = json_object();
    json_set(out, "text", json_string(joined));
    json_set(out, "attachments", attachments);
    free(joined);
    return out;
}

/* ── _submit_params ── */
/* PoP: gwa_submit_params @ hermes_cli/gateway_agent.py:_submit_params */
json_t *gwa_submit_params(json_t *st, const json_t *user_message,
                          const json_t *persist_user_message, const char *turn_author,
                          const gwa_seams_t *s) {
    json_t *attachments = NULL;
    json_t *um = json_copy(user_message);
    json_t *pum = persist_user_message ? json_copy(persist_user_message) : NULL;
    if (is_arr(um)) {
        json_t *staged = stage_content_parts(st, um, s);
        json_free(um);
        um = json_copy(json_obj_get(staged, "text"));
        attachments = json_copy(json_obj_get(staged, "attachments"));
        json_free(staged);
        if (is_arr(pum)) {
            json_free(pum);
            pum = json_copy(um);
        }
    }
    /* text = persist if isinstance(persist,str) else user_message */
    const json_t *text = is_str(pum) ? pum : um;
    const char *sid = json_get_str(st, "session_id", "");
    json_t *ua = args0();
    json_t *ur = gwa_call(s, "uuid4_hex", ua);
    json_free(ua);
    const char *sub = jstr(ur);
    char *sub_free = sub ? strdup(sub) : NULL;
    json_free(ur);

    json_t *params = json_object();
    aset_s(params, "session_id", sid);
    aset_s(params, "submission_id", sub_free ? sub_free : "");
    json_set(params, "text", text ? json_copy(text) : json_null());
    free(sub_free);
    if (jtruth(attachments)) json_set(params, "attachments", json_copy(attachments));
    if (is_str(um)) {
        const char *ums = um->str_val ? um->str_val : "";
        const char *texts = jstr(text);
        if (!texts || strcmp(ums, texts)) json_set(params, "api_text", json_copy(um));
    }
    if (turn_author && turn_author[0]) aset_s(params, "turn_author", turn_author);
    if (json_get_bool(st, "_voice_turn_pending", false)) json_set(params, "voice_turn", json_bool(true));
    const json_t *staged_msg = json_obj_get(st, "_pending_cli_user_message");
    if (is_obj(staged_msg)) {
        const json_t *dk = json_obj_get(staged_msg, "display_kind");
        if (jtruth(dk)) json_set(params, "display_kind", json_copy(dk));
    }
    if (json_get_bool(st, "_finite", false)) json_set(params, "finite", json_bool(true));
    if (jpresent(json_obj_get(st, "clarify_callback"))) json_set(params, "clarify_batch", json_bool(true));

    const json_t *info = state_info(st);
    json_t *pending = gwa_take_turn_route(st);
    if (jtruth(pending)) {
        json_set(params, "model_override", pending);
    } else {
        json_free(pending);
        const json_t *moa = json_obj_get(st, "_moa_preset");
        const char *moa_s = jstr(moa);
        if (moa_s && moa_s[0]) {
            const json_t *ip = info ? json_obj_get(info, "provider") : NULL;
            const json_t *im = info ? json_obj_get(info, "model") : NULL;
            const char *ips = jstr(ip);
            const char *ims = jstr(im);
            bool same = ips && !strcmp(ips, "moa") && ims && !strcmp(ims, moa_s);
            if (!same) {
                json_t *ov = json_object();
                aset_s(ov, "model", moa_s);
                aset_s(ov, "provider", "moa");
                json_set(params, "model_override", ov);
            }
        }
    }
    json_free(um);
    json_free(pum);
    json_free(attachments);
    return params;
}

/* ── clarify_reply ── */
/* PoP: gwa_clarify_reply @ hermes_cli/gateway_agent.py:clarify_reply */
json_t *gwa_clarify_reply(bool callback_present, const json_t *params, const gwa_seams_t *s) {
    json_t *questions = json_array();
    const json_t *qs = is_obj(params) ? json_obj_get(params, "questions") : NULL;
    if (is_arr(qs)) {
        for (size_t i = 0; i < qs->c.count; i++) {
            const json_t *q = qs->c.items[i];
            if (!is_obj(q)) continue;
            const json_t *qid = json_obj_get(q, "qid");
            if (!jtruth(qid)) continue;
            json_t *item = json_object();
            json_set(item, "qid", json_copy(qid));
            const json_t *question = json_obj_get(q, "question");
            json_set(item, "question", jtruth(question) ? json_copy(question) : json_string(""));
            const json_t *choices = json_obj_get(q, "choices");
            json_t *ch = json_array();
            if (is_arr(choices)) for (size_t j = 0; j < choices->c.count; j++)
                json_append(ch, json_copy(choices->c.items[j]));
            json_set(item, "choices", ch);
            const json_t *ms = json_obj_get(q, "multi_select");
            json_set(item, "multi_select", json_bool(jtruth(ms)));
            json_append(questions, item);
        }
    }
    json_t *cancelled = json_object();
    json_set(cancelled, "outcome", json_string("cancelled"));
    if (!callback_present || !jtruth(questions)) {
        json_free(questions);
        return cancelled;
    }
    json_t *ca = args0();
    json_set(ca, "questions", json_copy(questions));
    json_t *reply = gwa_call(s, "clarify_callback", ca);
    json_free(ca);
    json_free(questions);
    if (!is_obj(reply)) { json_free(reply); return cancelled; }
    json_free(cancelled);
    json_t *answers = json_object();
    const json_t *ra = json_obj_get(reply, "answers");
    if (is_obj(ra)) {
        for (size_t i = 0; i < ra->c.count; i++) {
            const json_t *v = ra->c.items[i];
            const char *key = ra->c.keys[i];
            if (is_str(v) || !jpresent(v)) {
                json_set(answers, key, v ? json_copy(v) : json_null());
            } else {
                /* json.dumps(value) — fixtures keep scalar numbers (no spacing drift) */
                char *ser = json_serialize(v);
                json_set(answers, key, json_string(ser ? ser : "null"));
                free(ser);
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "answers", answers);
    const json_t *outcome = json_obj_get(reply, "outcome");
    const char *os = jstr(outcome);
    if (os && (!strcmp(os, "submitted") || !strcmp(os, "timed_out") || !strcmp(os, "cancelled")))
        json_set(out, "outcome", json_copy(outcome));
    json_free(reply);
    return out;
}