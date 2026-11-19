/*
 * run_inbound_flow.c — FLOW cores + leaves of gateway/run_inbound.py
 * (hermes-agent), ported to C11. Decision sequences only: every live I/O
 * (policy lookups, refusal helpers, clarify/slash-confirm store calls, adapter
 * methods, eviction interrupts, command resolution, i18n, clocks) enters
 * through ONE universal seam: ri_call(ctx, name, args). The harness wires it
 * to a recording stub fed the case's scripted results (in.io); the oracle
 * installs recording monkeypatch stubs emitting the identical event shape
 * {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ri_-prefixed.
 *
 * Representation contract (shared with tests/oracle_run_inbound_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; BaseException
 *   kinds (SystemExit/KeyboardInterrupt/GeneratorExit) pass through every
 *   `except Exception` / `suppress(Exception)` site.
 * - Platform / MessageType members travel as their .value strings.
 * - event: the harness materializes a WORKING COPY (ctx.event); ports read and
 *   mutate it; the harness echoes it as out.state.event_out. The oracle keeps
 *   an EventDouble and echoes norm_event(ev) the same way.
 * - source: a raw fixture dict (platform a .value string); the oracle wraps it
 *   in a SimpleNamespace (platform -> Platform enum) and norms it back.
 * - agent: null | {"sentinel":true} | {"id":str, ...flags}. Projection to seam
 *   args: null->null, sentinel->{"__sentinel__":true}, else the id string.
 *   _AGENT_PENDING_SENTINEL identity is the "sentinel" flag.
 * - adapter: null | {"id":str,...} | str. Projection: dict->id (or null),
 *   str->itself, null->null. Method seams fire only for dict adapters; a str
 *   adapter raises AttributeError ('str' object has no attribute ...).
 * - pending (clarify): null | {"clarify_id":str,...}; outcome: null |
 *   {"steered":bool,"redirected":bool,...}; policy: null | dict.
 * - tools.clarify_gateway.CANCELLED == "\x00cancelled" carries an embedded NUL
 *   libjson cannot hold; BOTH sides project it to the visible literal
 *   "\\x00cancelled" (backslash-x-0-0) in the resolve seam's status arg.
 * - logger calls are silent on both sides (args still evaluated for raises).
 * - self._peek_session_state is a STATE accessor: records the event (ret = the
 *   state node copy) and returns a BORROWED pointer so writes persist.
 * - time.time() / _float_env are seams. asyncio is not used by these funcs.
 * - GATEWAY_KNOWN_COMMANDS is a snapshot of the live frozenset (87 names,
 *   probed from the quarry venv); both sides embed the same sorted list.
 * - Fixture corpus folds ASCII only (str.strip/lower/replace) and restricts
 *   adapter/pending/outcome scripts to the documented null|dict(|str) domains.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── module constants (seeded from the live modules) ── */
/* gateway/run.py:_INTERRUPT_REASON_EVICTED / _INTERRUPT_TOOL_REASON_EVICTED */
#define RI_INTERRUPT_REASON_EVICTED "Session ended while the turn was running"
#define RI_INTERRUPT_TOOL_REASON_EVICTED "session evicted"
/* tools/clarify_gateway.py:CANCELLED projected (embedded NUL -> visible). */
#define RI_CANCELLED_PROJ "\\x00cancelled"
/* run_inbound._SLASH_CONFIRM_TEXT_INPUT_KEYS */
static const char *RI_CONFIRM_TEXT_KEYS[][2] = {
    {"confirm_once", "once"}, {"confirm_always", "always"}, {"confirm_cancel", "cancel"},
};
#define RI_N_TEXT_KEYS (sizeof(RI_CONFIRM_TEXT_KEYS) / sizeof(RI_CONFIRM_TEXT_KEYS[0]))
/* run_inbound._restore_pending_one_turn_model_override note literal. */
#define RI_ONE_TURN_NOTE \
    "[Note: the one-turn model override has ended. The temporary model " \
    "no longer applies; use the current model and provider in your instructions.]"
/* hermes_cli/commands.py:GATEWAY_KNOWN_COMMANDS snapshot (sorted, 87 names). */
static const char *RI_KNOWN_COMMANDS[] = {
    "agents", "approvals", "approve", "autoprune", "bg", "blueprint", "bp",
    "branch", "btw", "bundles", "busy", "codex-runtime", "codex_runtime",
    "commands", "compact", "compress", "context", "ctx", "curator", "debug",
    "deny", "diff", "discard", "egress", "fast", "footer", "fork", "goal",
    "hb", "heartbeat", "help", "init", "insights", "kanban", "learn", "login",
    "loop", "memory", "moa", "model", "new", "pause", "personality", "plan",
    "platform", "proactive", "profile", "q", "queue", "reasoning", "refine",
    "reload-mcp", "reload-skills", "reload_mcp", "reload_skills", "reset",
    "restart", "resume", "retry", "review", "rollback", "s", "save",
    "sessions", "set-home", "sethome", "skills", "start", "status", "steer",
    "stop", "subgoal", "suggest", "suggestions", "tasks", "title", "topic",
    "topup", "undo", "update", "usage", "v", "verbose", "version", "voice",
    "whoami", "yolo",
};
#define RI_N_KNOWN (sizeof(RI_KNOWN_COMMANDS) / sizeof(RI_KNOWN_COMMANDS[0]))

/* ── universal seam context ── */
typedef struct ri_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
    json_t *event;      /* owned working copy of args.event (NULL if none) */
} ri_ctx_t;

/* ── basic helpers ── */
static bool ri_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool ri_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool ri_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static const char *ri_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool ri_str_eq(const json_t *v, const char *s) {
    const char *t = ri_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* `is True` identity: JSON_BOOL true only (fixture 1 reads False). */
static bool ri_is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* exceptions caught by `except Exception` (not SystemExit & co) */
static bool ri_is_exception(const json_t *r) {
    if (!ri_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") != 0 && strcmp(t, "KeyboardInterrupt") != 0
        && strcmp(t, "GeneratorExit") != 0;
}

static json_t *ri_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* AttributeError message builders for the carriers the doubles model. */
static json_t *ri_attr_err(const char *carrier, const char *name) {
    char buf[256];
    if (strcmp(carrier, "bare") == 0)
        snprintf(buf, sizeof buf, "%s", name);
    else
        snprintf(buf, sizeof buf, "'%s' object has no attribute '%s'", carrier, name);
    return ri_err("AttributeError", buf);
}
static json_t *ri_attr_none(const char *n) { return ri_attr_err("NoneType", n); }
static json_t *ri_attr_ns(const char *n) { return ri_attr_err("types.SimpleNamespace", n); }
static json_t *ri_attr_str(const char *n) { return ri_attr_err("str", n); }
static json_t *ri_attr_obj(const char *n) { return ri_attr_err("object", n); }
static json_t *ri_attr_bare(const char *n) { return ri_attr_err("bare", n); }

/* Python type name of a scalar node (for TypeError messages). */
static const char *ri_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *ri_not_iterable(const json_t *v) {
    char buf[128];
    snprintf(buf, sizeof buf, "'%s' object is not iterable", ri_type_name(v));
    return ri_err("TypeError", buf);
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *ri_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return ri_err(type, msg);
}

/* ── string helpers (Python str semantics on the ASCII fixture corpus) ── */
static char *ri_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* str.strip(): ASCII whitespace both ends. */
static char *ri_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold. */
static char *ri_lower(const char *s) {
    char *o = ri_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* str.replace(old, new) for the single-char '_' -> '-' case. */
static char *ri_replace_char(const char *s, char from, char to) {
    char *o = ri_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) if (*p == from) *p = to;
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t ri_utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* f"{v}" for the int/str/bool/None ids the fixtures carry. */
static char *ri_id_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ri_strdup("None");
    if (v->type == JSON_STRING) return ri_strdup(v->str_val);
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%g", v->num_val);
        return ri_strdup(buf);
    }
    if (v->type == JSON_BOOL) return ri_strdup(v->bool_val ? "True" : "False");
    return ri_strdup("");
}

/* ── seam dispatch ── */
/*
 * One scripted entry: a plain value (reused), or {"__script__": true,
 * "ret": v, "raise": spec}. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides. A bare JSON array is a per-call QUEUE.
 */
static json_t *ri_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return ri_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees; a raise object
 * propagates unless the call site's Python handler catches it. */
static json_t *ri_call(ri_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    const json_t *entry = c->io ? json_obj_get(c->io, name) : NULL;
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = ri_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = ri_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = ri_entry_result(json_get(entry, idx));
        }
    } else {
        res = ri_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* Propagate a raise result unless it is NULL (success). */
#define RI_CHK(v) do { if (ri_is_raise(v)) return (v); } while (0)

/* `except Exception` / `suppress(Exception)` catch: frees v; returns NULL when
 * the raise is swallowed (or v was a plain result), or the raise node when a
 * BaseException must propagate. */
static json_t *ri_catch_exception(json_t *v) {
    if (!v) return NULL;
    if (ri_is_raise(v)) {
        if (ri_is_exception(v)) { json_free(v); return NULL; }
        return v;
    }
    json_free(v);
    return NULL;
}

/* ── state helpers ── */
static json_t *ri_sub(ri_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static bool ri_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}

/* self._peek_session_state: STATE accessor. Records the identical event the
 * oracle's recorder emits (ret = the state node copy) and returns a BORROWED
 * pointer so writes persist into out.state. */
static json_t *ri_peek_state(ri_ctx_t *c, const char *key) {
    json_t *states = ri_sub(c, "session_states");
    json_t *node = (states && key) ? json_obj_get(states, key) : NULL;
    json_t *a = json_object();
    json_set_hdr(a, "key", key ? key : "");
    json_t *ev = json_object();
    json_set_hdr(ev, "name", "self._peek_session_state");
    json_set(ev, "args", a);
    json_set(ev, "ret", node ? json_copy(node) : json_null());
    json_append(c->trace, ev);
    return node;
}

/* ── projections ── */
/* agent node -> seam-arg projection (null | {"__sentinel__":true} | id str). */
static json_t *ri_agent_proj(const json_t *agent) {
    if (ri_is_null(agent)) return json_null();
    if (agent->type == JSON_OBJECT) {
        if (json_get_bool(agent, "sentinel", false)) {
            json_t *m = json_object();
            json_set(m, "__sentinel__", json_bool(true));
            return m;
        }
        json_t *id = json_obj_get(agent, "id");
        return id ? json_copy(id) : json_null();
    }
    return json_copy(agent);
}

static bool ri_agent_is_sentinel(const json_t *agent) {
    return agent && agent->type == JSON_OBJECT && json_get_bool(agent, "sentinel", false);
}

/* adapter node -> id projection (dict->id|null, str->itself, null->null). */
static json_t *ri_adapter_id(const json_t *adapter) {
    if (ri_is_null(adapter)) return json_null();
    if (adapter->type == JSON_OBJECT) {
        json_t *id = json_obj_get(adapter, "id");
        return id ? json_copy(id) : json_null();
    }
    return json_copy(adapter);
}

/* source.platform == Platform.<X> on the .value strings fixtures carry. */
static bool ri_platform_is(const json_t *source, const char *value) {
    if (!source || source->type != JSON_OBJECT) return false;
    return ri_str_eq(json_obj_get(source, "platform"), value);
}

/* ── common arg builders (all return OWNED objects) ── */
static json_t *ri_a_key(const char *key) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key ? key : "");
    return a;
}
static json_t *ri_a_event(ri_ctx_t *c) {
    json_t *a = json_object();
    json_set(a, "event", c->event ? json_copy(c->event) : json_null());
    return a;
}
static json_t *ri_a_source(const json_t *source) {
    json_t *a = json_object();
    json_set(a, "source", source ? json_copy(source) : json_null());
    return a;
}

/* event.message_type == MessageType.<X> (value strings). */
static bool ri_msg_type_is(const json_t *event, const char *value) {
    return ri_str_eq(json_obj_get(event, "message_type"), value);
}

/* Read a required string attribute off the event double; on absence emit the
 * bare-key AttributeError the EventDouble raises. Returns NULL on success
 * (value borrowed via *out), or a raise node. */
static json_t *ri_event_str_attr(const json_t *event, const char *key, const char **out) {
    json_t *v = json_obj_get(event, key);
    if (!v) return ri_attr_bare(key);
    *out = ri_str(v);
    return NULL;
}

/* Split a UTF-8 string into an array of single-character strings (ASCII-safe;
 * multibyte codepoints kept whole). Used for str iteration / list(str). */
static json_t *ri_utf8_chars(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    size_t i = 0, n = strlen(s);
    while (i < n) {
        size_t j = i + 1;
        while (j < n && ((unsigned char)s[j] & 0xc0) == 0x80) j++;
        char *tmp = malloc(j - i + 1);
        if (tmp) {
            memcpy(tmp, s + i, j - i);
            tmp[j - i] = '\0';
            json_append(arr, json_string(tmp));
            free(tmp);
        }
        i = j;
    }
    return arr;
}

/* ══════════════ Group A: policy / refusal helpers ══════════════ */

/* PoP: ri_prompt_admin_ids @ gateway/run_inbound.py:_prompt_admin_ids */
static json_t *ri_prompt_admin_ids(ri_ctx_t *c, const json_t *args) {
    (void)args;
    if (!ri_has(c->event, "source")) return ri_attr_bare("source");
    json_t *source = json_obj_get(c->event, "source");
    return ri_call(c, "policy_for_runner_source", ri_a_source(source));
}

/* PoP: ri_update_reply_refused @ gateway/run_inbound.py:_hm_update_reply_refused */
static json_t *ri_update_reply_refused(ri_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *ev = c->event;
    if (!ri_has(ev, "user_id")) return ri_attr_bare("user_id");
    json_t *uid_node = json_obj_get(ev, "user_id");
    json_t *uid = NULL;
    if (ri_truthy(uid_node)) {
        uid = json_copy(uid_node);
    } else {
        if (!ri_has(ev, "source")) return ri_attr_bare("source");
        json_t *source = json_obj_get(ev, "source");
        if (ri_is_null(source)) return ri_attr_none("user_id");
        if (!ri_has(source, "user_id")) return ri_attr_ns("user_id");
        uid = json_copy(json_obj_get(source, "user_id"));
    }
    /* admin_ids = policy_for_runner_source(self, event.source).admin_user_ids */
    if (!ri_has(ev, "source")) { json_free(uid); return ri_attr_bare("source"); }
    json_t *source = json_obj_get(ev, "source");
    json_t *policy = ri_call(c, "policy_for_runner_source", ri_a_source(source));
    if (ri_is_raise(policy)) { json_free(uid); return policy; }
    if (ri_is_null(policy)) { json_free(uid); json_free(policy); return ri_attr_none("admin_user_ids"); }
    if (!ri_has(policy, "admin_user_ids")) {
        json_free(uid); json_free(policy); return ri_attr_ns("admin_user_ids");
    }
    json_t *admin_ids = json_copy(json_obj_get(policy, "admin_user_ids"));
    json_free(policy);
    json_t *a = json_object();
    json_set(a, "user_id", uid);
    json_set(a, "admin_ids", admin_ids);
    json_t *r = ri_call(c, "update_prompt_refusal", a);
    RI_CHK(r);
    bool refused = !ri_is_null(r);
    json_free(r);
    return json_bool(refused);
}

/* Shared tail of _refused_clarify / _refused_slash_confirm: requester truthy ->
 * refusal_text(requester), else None. requester is OWNED. */
static json_t *ri_refusal_tail(ri_ctx_t *c, json_t *requester) {
    RI_CHK(requester);
    if (!ri_truthy(requester)) { json_free(requester); return NULL; }
    json_t *ra = json_object();
    json_set(ra, "requester", requester);
    return ri_call(c, "refusal_text", ra);
}

/* PoP: ri_refused_clarify @ gateway/run_inbound.py:_refused_clarify */
static json_t *ri_refused_clarify(ri_ctx_t *c, const json_t *args) {
    const char *clarify_id = json_get_str(args, "clarify_id", "");
    json_t *sid = ri_call(c, "event.sender_id", json_object());
    RI_CHK(sid);
    json_t *sender = ri_truthy(sid) ? json_copy(sid) : json_string("");
    json_free(sid);
    json_t *admin = ri_prompt_admin_ids(c, args);
    if (ri_is_raise(admin)) { json_free(sender); return admin; }
    json_t *admitted = ri_call(c, "answer_admitted", ri_a_event(c));
    if (ri_is_raise(admitted)) { json_free(sender); json_free(admin); return admitted; }
    json_t *a = json_object();
    json_set_hdr(a, "clarify_id", clarify_id);
    json_set(a, "sender_id", sender);
    json_set(a, "admin_ids", admin);
    json_set(a, "admitted", admitted);
    json_t *requester = ri_call(c, "clarify_refusal", a);
    return ri_refusal_tail(c, requester);
}

/* PoP: ri_refused_slash_confirm @ gateway/run_inbound.py:_refused_slash_confirm */
static json_t *ri_refused_slash_confirm(ri_ctx_t *c, const json_t *args) {
    const char *session_key = json_get_str(args, "session_key", "");
    const char *confirm_id = json_get_str(args, "confirm_id", "");
    json_t *sid = ri_call(c, "event.sender_id", json_object());
    RI_CHK(sid);
    json_t *sender = ri_truthy(sid) ? json_copy(sid) : json_string("");
    json_free(sid);
    json_t *admin = ri_prompt_admin_ids(c, args);
    if (ri_is_raise(admin)) { json_free(sender); return admin; }
    json_t *admitted = ri_call(c, "answer_admitted", ri_a_event(c));
    if (ri_is_raise(admitted)) { json_free(sender); json_free(admin); return admitted; }
    json_t *a = json_object();
    json_set_hdr(a, "session_key", session_key);
    json_set_hdr(a, "confirm_id", confirm_id);
    json_set(a, "sender_id", sender);
    json_set(a, "admin_ids", admin);
    json_set(a, "admitted", admitted);
    json_t *requester = ri_call(c, "slash_confirm_refusal", a);
    return ri_refusal_tail(c, requester);
}

/* PoP: ri_slash_confirm_text_choices @ gateway/run_inbound.py:_slash_confirm_text_choices */
static json_t *ri_slash_confirm_text_choices(ri_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *choices = json_object();
    for (size_t k = 0; k < RI_N_TEXT_KEYS; k++) {
        json_t *a = json_object();
        json_set_hdr(a, "input_key", RI_CONFIRM_TEXT_KEYS[k][0]);
        json_t *words = ri_call(c, "approval_input_words", a);
        if (ri_is_raise(words)) { json_free(choices); return words; }
        json_t *item_list = NULL;
        if (words && words->type == JSON_ARRAY) {
            item_list = json_copy(words);
        } else if (words && words->type == JSON_STRING) {
            item_list = ri_utf8_chars(words->str_val);
        } else if (words && words->type == JSON_OBJECT) {
            item_list = json_array();
            for (size_t i = 0; i < words->c.count; i++)
                json_append(item_list, json_string(words->c.keys[i]));
        } else {
            json_t *e = ri_not_iterable(words);
            json_free(words); json_free(choices); return e;
        }
        json_free(words);
        for (size_t i = 0; i < json_len(item_list); i++) {
            json_t *w = json_get(item_list, i);
            const char *ws = ri_str(w);
            if (!ws) {
                char buf[160];
                snprintf(buf, sizeof buf,
                         "cannot use '%s' as a dict key (unhashable type: '%s')",
                         ri_type_name(w), ri_type_name(w));
                json_free(item_list); json_free(choices);
                return ri_err("TypeError", buf);
            }
            if (!json_obj_get(choices, ws))
                json_set(choices, ws, json_string(RI_CONFIRM_TEXT_KEYS[k][1]));
        }
        json_free(item_list);
    }
    return choices;
}

/* `except Exception` wrapping a whole body: a swallowed Exception returns None
 * (the live handler falls through to its except); a BaseException propagates. */
#define RI_TRY_CHK(v) do { if (ri_is_raise(v)) { json_t *px_ = ri_catch_exception(v); return px_ ? px_ : NULL; } } while (0)

/* ══════════════ Group B: clarify release ══════════════ */

/* PoP: ri_release_pending_clarify @ gateway/run_inbound.py:_release_pending_clarify */
static json_t *ri_release_pending_clarify(ri_ctx_t *c, const json_t *args) {
    json_t *pending = json_obj_get(args, "pending");
    json_t *source = json_obj_get(args, "source");
    if (ri_is_null(pending)) return ri_attr_none("clarify_id");
    if (!ri_has(pending, "clarify_id")) return ri_attr_ns("clarify_id");
    json_t *cid = json_obj_get(pending, "clarify_id");
    json_t *a = json_object();
    json_set(a, "clarify_id", json_copy(cid));
    json_set_hdr(a, "status", RI_CANCELLED_PROJ);
    json_t *resolved = ri_call(c, "clarify_gateway.resolve_gateway_clarify", a);
    RI_CHK(resolved);
    if (!ri_truthy(resolved)) { json_free(resolved); return json_bool(false); }
    json_free(resolved);
    json_t *adapter = ri_call(c, "self._delivery_adapter_for", ri_a_source(source));
    RI_CHK(adapter);
    bool has_card = adapter && adapter->type == JSON_OBJECT
                    && json_get_bool(adapter, "has_retire_card", false);
    if (has_card) {
        json_t *ta = json_object();
        json_set_hdr(ta, "key", "gateway.clarify.cancelled");
        json_t *text = ri_call(c, "t", ta);
        json_t *px = NULL;
        if (ri_is_raise(text)) {
            px = ri_catch_exception(text);
        } else {
            json_t *ra = json_object();
            json_set(ra, "adapter", ri_adapter_id(adapter));
            json_set(ra, "clarify_id", json_copy(cid));
            json_set(ra, "text", text);
            json_t *rr = ri_call(c, "adapter.retire_clarify_card", ra);
            px = ri_catch_exception(rr);
        }
        if (px) { json_free(adapter); return px; }
    }
    json_free(adapter);
    return json_bool(true);
}

/* PoP: ri_release_clarify_owning_turn @ gateway/run_inbound.py:_release_clarify_owning_turn */
static json_t *ri_release_clarify_owning_turn(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    json_t *source = json_obj_get(args, "source");
    json_t *a = json_object();
    json_set_hdr(a, "session_key", key);
    json_set(a, "include_choice_prompts", json_bool(true));
    json_t *pending = ri_call(c, "clarify_gateway.get_pending_for_session", a);
    RI_CHK(pending);
    if (ri_is_null(pending)) { json_free(pending); return json_bool(false); }
    json_t *ca = json_object();
    json_set(ca, "pending", pending);
    json_set(ca, "source", source ? json_copy(source) : json_null());
    json_t *released = ri_release_pending_clarify(c, ca);
    json_free(ca);
    return released;
}

/* ══════════════ Group C: eviction / merge / drop ══════════════ */

/* PoP: ri_evict_running_agent @ gateway/run_inbound.py:_hm_evict_running_agent */
static json_t *ri_evict_running_agent(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    const char *reason = json_get_str(args, "reason", "");
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "interrupt_reason", RI_INTERRUPT_REASON_EVICTED);
    json_set_hdr(a, "invalidation_reason", reason);
    json_set_hdr(a, "tool_reason", RI_INTERRUPT_TOOL_REASON_EVICTED);
    json_t *gen = ri_call(c, "self._interrupt_running_turn", a);
    RI_CHK(gen);
    json_t *da = json_object();
    json_set_hdr(da, "key", key);
    json_set(da, "run_generation", gen);
    json_t *r = ri_call(c, "self._drop_turn_slot", da);
    RI_CHK(r);
    json_free(r);
    return NULL;
}

/* PoP: ri_evict_reaped_agent @ gateway/run_inbound.py:_hm_evict_reaped_agent */
static json_t *ri_evict_reaped_agent(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    json_t *store = json_obj_get(c->state, "session_store");
    bool has_peek = json_get_bool(store, "has_peek", false);
    bool has_ended = json_get_bool(store, "has_ended", false);
    json_t *sid = NULL;
    if (has_peek) {
        sid = ri_call(c, "store.peek_session_id", ri_a_key(key));
        RI_TRY_CHK(sid);
    }
    bool proceed = sid && sid->type == JSON_STRING && sid->str_val[0] != '\0' && has_ended;
    if (proceed) {
        json_t *ea = json_object();
        json_set_hdr(ea, "session_id", sid->str_val);
        json_t *ended = ri_call(c, "store._is_session_ended_in_db", ea);
        RI_TRY_CHK(ended);
        bool is_ended = ri_is_true(ended);
        json_free(ended);
        if (is_ended) {
            json_free(sid);
            json_t *ea2 = json_object();
            json_set_hdr(ea2, "key", key);
            json_set_hdr(ea2, "reason", "reaped_session_eviction");
            json_t *r = ri_evict_running_agent(c, ea2);
            json_free(ea2);
            RI_TRY_CHK(r);
            json_free(r);
            return NULL;
        }
    }
    json_free(sid);
    return NULL;
}

/* PoP: ri_merge_pending_for_source @ gateway/run_inbound.py:_hm_merge_pending_for_source */
static json_t *ri_merge_pending_for_source(ri_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    bool merge_text = json_get_bool(args, "merge_text", false);
    json_t *adapter = ri_call(c, "self._delivery_adapter_for", ri_a_source(source));
    RI_CHK(adapter);
    if (!ri_truthy(adapter)) { json_free(adapter); return NULL; }
    json_t *pending = NULL;
    if (adapter->type == JSON_OBJECT) {
        json_t *pm = json_obj_get(adapter, "pending_messages");
        pending = pm ? json_copy(pm) : json_object();
    } else {
        json_t *e = ri_attr_err(ri_type_name(adapter), "_pending_messages");
        json_free(adapter);
        return e;
    }
    json_t *a = json_object();
    json_set(a, "pending_messages", pending);
    json_set_hdr(a, "key", key);
    json_set(a, "event", c->event ? json_copy(c->event) : json_null());
    json_set(a, "merge_text", json_bool(merge_text));
    json_free(adapter);
    json_t *r = ri_call(c, "merge_pending_message_event", a);
    RI_CHK(r);
    json_free(r);
    return NULL;
}

/* PoP: ri_drop_held_input @ gateway/run_inbound.py:_hm_drop_held_input */
static json_t *ri_drop_held_input(ri_ctx_t *c, const json_t *args) {
    (void)args;
    if (!ri_has(c->event, "source")) return ri_attr_bare("source");
    json_t *source = json_obj_get(c->event, "source");
    json_t *adapter = ri_call(c, "self._intake_adapter_for", ri_a_source(source));
    RI_CHK(adapter);
    if (ri_is_null(adapter)) { json_free(adapter); return NULL; }
    if (adapter->type != JSON_OBJECT) {
        /* live resolves adapter._discard_text_debounce (the outer method) BEFORE
         * evaluating the adapter._event_session_key(event) argument. */
        json_t *e = ri_attr_err(ri_type_name(adapter), "_discard_text_debounce");
        json_free(adapter);
        return e;
    }
    json_t *ka = json_object();
    json_set(ka, "adapter", ri_adapter_id(adapter));
    json_set(ka, "event", c->event ? json_copy(c->event) : json_null());
    json_t *key = ri_call(c, "adapter._event_session_key", ka);
    if (ri_is_raise(key)) { json_free(adapter); return key; }
    json_t *da = json_object();
    json_set(da, "adapter", ri_adapter_id(adapter));
    json_set(da, "key", key);
    json_free(adapter);
    json_t *r = ri_call(c, "adapter._discard_text_debounce", da);
    RI_CHK(r);
    json_free(r);
    return NULL;
}

/* Deep JSON equality mirroring Python == (bool/number numeric cross-equal). */
static bool ri_json_eq(const json_t *a, const json_t *b) {
    bool an = ri_is_null(a), bn = ri_is_null(b);
    if (an || bn) return an && bn;
    bool a_num = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    bool b_num = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (a_num && b_num) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    if (a->type == JSON_STRING) return strcmp(a->str_val, b->str_val) == 0;
    if (a->type == JSON_ARRAY) {
        if (json_len(a) != json_len(b)) return false;
        for (size_t i = 0; i < json_len(a); i++)
            if (!ri_json_eq(json_get(a, i), json_get(b, i))) return false;
        return true;
    }
    if (a->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !ri_json_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

/* ══════════════ Group D: busy-path helpers ══════════════ */

/* PoP: ri_text_only @ gateway/run_inbound.py:_hm_text_only */
static json_t *ri_text_only(ri_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *ev = c->event;
    if (!ri_has(ev, "message_type")) return ri_attr_bare("message_type");
    if (!ri_msg_type_is(ev, "text")) return json_bool(false);
    if (!ri_has(ev, "media_urls")) return ri_attr_bare("media_urls");
    if (ri_truthy(json_obj_get(ev, "media_urls"))) return json_bool(false);
    if (!ri_has(ev, "media_types")) return ri_attr_bare("media_types");
    if (ri_truthy(json_obj_get(ev, "media_types"))) return json_bool(false);
    return json_bool(true);
}

/* PoP: ri_in_telegram_grace @ gateway/run_inbound.py:_hm_in_telegram_grace */
static json_t *ri_in_telegram_grace(ri_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *ev = c->event;
    json_t *grace = NULL, *tt = NULL, *ret = NULL;
    json_t *fa = json_object();
    json_set_hdr(fa, "name", "HERMES_TELEGRAM_FOLLOWUP_GRACE_SECONDS");
    json_set(fa, "default", json_number(3.0));
    grace = ri_call(c, "_float_env", fa);
    if (ri_is_raise(grace)) { ret = grace; grace = NULL; goto done; }
    json_t *st = ri_peek_state(c, key);
    bool started_truthy = false, started_num = true;
    double started = 0.0;
    const char *started_type = "int";
    if (st) {
        if (!ri_has(st, "turn")) { ret = ri_attr_bare("turn"); goto done; }
        json_t *turn = json_obj_get(st, "turn");
        if (!ri_has(turn, "started_ts")) { ret = ri_attr_bare("started_ts"); goto done; }
        json_t *sn = json_obj_get(turn, "started_ts");
        started_truthy = ri_truthy(sn);
        started_type = ri_type_name(sn);
        if (sn && sn->type == JSON_NUMBER) started = sn->num_val;
        else if (sn && sn->type == JSON_BOOL) started = sn->bool_val ? 1.0 : 0.0;
        else started_num = false;
    }
    json_t *raw = json_obj_get(ev, "raw_message");
    bool have_arrived = false, arrived_num = true;
    double arrived = 0.0;
    const char *arrived_type = "float";
    if (ri_truthy(raw)) {
        if (!ri_has(ev, "timestamp")) { ret = ri_attr_bare("timestamp"); goto done; }
        json_t *ts = json_obj_get(ev, "timestamp");
        json_t *edit = json_obj_get(raw, "edit_date");
        json_t *sent = (edit && edit->type == JSON_NUMBER) ? edit : ts;
        if (sent && sent->type == JSON_NUMBER) { arrived = sent->num_val; have_arrived = true; }
    }
    if (!have_arrived) {
        tt = ri_call(c, "time.time", json_object());
        if (ri_is_raise(tt)) { ret = tt; tt = NULL; goto done; }
        arrived_type = ri_type_name(tt);
        if (tt && tt->type == JSON_NUMBER) arrived = tt->num_val;
        else if (tt && tt->type == JSON_BOOL) arrived = tt->bool_val ? 1.0 : 0.0;
        else arrived_num = false;
    }
    if (ri_is_null(source)) { ret = ri_attr_none("platform"); goto done; }
    if (!ri_has(source, "platform")) { ret = ri_attr_ns("platform"); goto done; }
    if (!ri_platform_is(source, "telegram")) { ret = json_bool(false); goto done; }
    if (!ri_has(ev, "message_type")) { ret = ri_attr_bare("message_type"); goto done; }
    if (!(ri_msg_type_is(ev, "text") || ri_msg_type_is(ev, "voice"))) { ret = json_bool(false); goto done; }
    bool grace_num = grace && (grace->type == JSON_NUMBER || grace->type == JSON_BOOL);
    if (!grace_num) {
        char buf[160];
        snprintf(buf, sizeof buf, "'>' not supported between instances of '%s' and 'int'", ri_type_name(grace));
        ret = ri_err("TypeError", buf); goto done;
    }
    double gval = grace->type == JSON_BOOL ? (grace->bool_val ? 1.0 : 0.0) : grace->num_val;
    if (!(gval > 0.0)) { ret = json_bool(false); goto done; }
    if (!started_truthy) { ret = json_bool(false); goto done; }
    if (!arrived_num || !started_num) {
        char buf[160];
        snprintf(buf, sizeof buf, "unsupported operand type(s) for -: '%s' and '%s'", arrived_type, started_type);
        ret = ri_err("TypeError", buf); goto done;
    }
    double diff = arrived - started;
    if (diff < -1.0 || diff > gval) { ret = json_bool(false); goto done; }
    ret = json_bool(true);
done:
    json_free(grace);
    json_free(tt);
    return ret;
}

/* PoP: ri_busy_telegram_grace_queue @ gateway/run_inbound.py:_hm_busy_telegram_grace_queue */
static json_t *ri_busy_telegram_grace_queue(ri_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    const char *mode = json_get_str(args, "effective_busy_input_mode", "");
    json_t *ga = json_object();
    json_set(ga, "source", source ? json_copy(source) : json_null());
    json_set_hdr(ga, "key", key);
    json_t *grace = ri_in_telegram_grace(c, ga);
    json_free(ga);
    RI_CHK(grace);
    bool in_grace = ri_truthy(grace);
    json_free(grace);
    if (!in_grace) return json_bool(false);
    if (strcmp(mode, "queue") != 0) {
        json_t *ma = json_object();
        json_set(ma, "source", source ? json_copy(source) : json_null());
        json_set_hdr(ma, "key", key);
        json_set(ma, "merge_text", json_bool(true));
        json_t *r = ri_merge_pending_for_source(c, ma);
        json_free(ma);
        RI_CHK(r);
        json_free(r);
    } else {
        json_t *adapter = ri_call(c, "self._delivery_adapter_for", ri_a_source(source));
        RI_CHK(adapter);
        if (ri_truthy(adapter)) {
            json_t *ea = json_object();
            json_set_hdr(ea, "key", key);
            json_set(ea, "event", c->event ? json_copy(c->event) : json_null());
            json_set(ea, "adapter", ri_adapter_id(adapter));
            json_t *r = ri_call(c, "self._enqueue_fifo", ea);
            if (ri_is_raise(r)) { json_free(adapter); return r; }
            json_free(r);
        }
        json_free(adapter);
    }
    return json_bool(true);
}

/* PoP: ri_busy_steer @ gateway/run_inbound.py:_hm_busy_steer */
static json_t *ri_busy_steer(ri_ctx_t *c, const json_t *args) {
    json_t *agent = json_obj_get(args, "agent");
    const char *key = json_get_str(args, "key", "");
    json_t *ev = c->event;
    if (!ri_has(ev, "text")) return ri_attr_bare("text");
    json_t *text = json_obj_get(ev, "text");
    const char *ts = ri_truthy(text) ? ri_str(text) : "";
    if (ri_truthy(text) && !ts) return ri_attr_err(ri_type_name(text), "strip");
    char *steer_text = ri_strip(ts ? ts : "");
    json_t *to = ri_text_only(c, args);
    if (ri_is_raise(to)) { free(steer_text); return to; }
    bool text_only = ri_truthy(to);
    json_free(to);
    bool has_steer = agent && agent->type == JSON_OBJECT && json_get_bool(agent, "has_steer", false);
    bool steered = false;
    if (text_only && steer_text && steer_text[0] && has_steer) {
        json_t *oa = json_object();
        json_set_hdr(oa, "text", steer_text);
        json_set(oa, "event", ev ? json_copy(ev) : json_null());
        json_t *origin = ri_call(c, "self._steer_text_with_origin", oa);
        json_t *px = NULL;
        if (ri_is_raise(origin)) {
            px = ri_catch_exception(origin);
        } else {
            json_t *sa = json_object();
            json_set(sa, "agent", ri_agent_proj(agent));
            json_set(sa, "text", origin);
            json_t *sr = ri_call(c, "self._steer_running_agent", sa);
            if (ri_is_raise(sr)) {
                px = ri_catch_exception(sr);
            } else {
                steered = ri_truthy(sr);
                json_free(sr);
            }
        }
        if (px) { free(steer_text); return px; }
    }
    free(steer_text);
    if (steered) {
        json_t *fa = json_object();
        json_set(fa, "agent", ri_agent_proj(agent));
        json_set_hdr(fa, "key", key);
        json_set(fa, "event", ev ? json_copy(ev) : json_null());
        json_t *r = ri_call(c, "self._fold_into_running_turn", fa);
        RI_CHK(r);
        json_free(r);
        return NULL;
    }
    json_t *qa = json_object();
    json_set_hdr(qa, "key", key);
    json_set(qa, "event", ev ? json_copy(ev) : json_null());
    json_t *r = ri_call(c, "self._queue_or_replace_pending_event", qa);
    RI_CHK(r);
    json_free(r);
    return NULL;
}

/* PoP: ri_busy_interrupt @ gateway/run_inbound.py:_hm_busy_interrupt */
static json_t *ri_busy_interrupt(ri_ctx_t *c, const json_t *args) {
    json_t *agent = json_obj_get(args, "agent");
    const char *key = json_get_str(args, "key", "");
    json_t *ev = c->event;
    bool can_redirect = agent && agent->type == JSON_OBJECT
                        && ri_is_true(json_obj_get(agent, "supports_redirect"));
    json_t *to = ri_text_only(c, args);
    RI_CHK(to);
    bool text_only = ri_truthy(to);
    json_free(to);
    bool has_redirect = agent && agent->type == JSON_OBJECT && json_get_bool(agent, "has_redirect", false);
    if (text_only && can_redirect && has_redirect) {
        if (!ri_has(ev, "text")) return ri_attr_bare("text");
        json_t *text = json_obj_get(ev, "text");
        const char *ts = ri_truthy(text) ? ri_str(text) : "";
        if (ri_truthy(text) && !ts) return ri_attr_err(ri_type_name(text), "strip");
        char *stripped = ri_strip(ts ? ts : "");
        json_t *ra = json_object();
        json_set(ra, "agent", ri_agent_proj(agent));
        json_set_hdr(ra, "text", stripped ? stripped : "");
        json_set_hdr(ra, "key", key);
        json_set(ra, "event", ev ? json_copy(ev) : json_null());
        free(stripped);
        json_t *redir = ri_call(c, "self._redirect_active_turn", ra);
        RI_CHK(redir);
        bool done = ri_truthy(redir);
        json_free(redir);
        if (done) return NULL;
    }
    json_t *ap = ri_call(c, "self._pending_event_audio_paths", ri_a_event(c));
    RI_CHK(ap);
    bool has_audio = ri_truthy(ap);
    json_free(ap);
    if (has_audio) {
        json_t *qa = json_object();
        json_set_hdr(qa, "key", key);
        json_set(qa, "event", ev ? json_copy(ev) : json_null());
        json_t *r = ri_call(c, "self._queue_or_replace_pending_event", qa);
        RI_CHK(r);
        json_free(r);
    }
    /* live resolves running_agent.interrupt (the outer method) BEFORE evaluating
     * the self._busy_interrupt_text(event) argument: a non-carrier agent raises
     * here without firing the busy_interrupt_text seam. */
    if (ri_is_null(agent)) return ri_attr_none("interrupt");
    if (ri_agent_is_sentinel(agent)) return ri_attr_obj("interrupt");
    if (agent->type != JSON_OBJECT) return ri_attr_err(ri_type_name(agent), "interrupt");
    json_t *itext = ri_call(c, "self._busy_interrupt_text", ri_a_event(c));
    RI_CHK(itext);
    json_t *ia = json_object();
    json_set(ia, "agent", ri_agent_proj(agent));
    json_set(ia, "text", itext);
    json_t *r = ri_call(c, "agent.interrupt", ia);
    RI_CHK(r);
    json_free(r);
    return NULL;
}

/* PoP: ri_clarify_takeover @ gateway/run_inbound.py:_clarify_takeover */
static json_t *ri_clarify_takeover(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "key", "");
    json_t *gen = json_obj_get(c->event, "_clarify_superseded");
    if (ri_is_null(gen)) return json_bool(false);
    json_t *cur = ri_call(c, "self._current_session_run_generation", ri_a_key(key));
    RI_CHK(cur);
    bool eq = ri_json_eq(gen, cur);
    json_free(cur);
    return json_bool(eq);
}

/* PoP: ri_busy_preempt @ gateway/run_inbound.py:_hm_busy_preempt */
static json_t *ri_busy_preempt(ri_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    json_t *ev = c->event;
    json_t *mode_node = NULL, *adapter = NULL, *outcome = NULL, *ret = NULL;
    json_t *ta = json_object();
    json_set_hdr(ta, "key", key);
    json_t *sup = ri_clarify_takeover(c, ta);
    json_free(ta);
    if (ri_is_raise(sup)) { ret = sup; goto done; }
    bool superseded = ri_truthy(sup);
    json_free(sup);
    const char *mode = "interrupt";
    if (!superseded) {
        mode_node = ri_call(c, "self._effective_busy_input_mode", ri_a_source(source));
        if (ri_is_raise(mode_node)) { ret = mode_node; mode_node = NULL; goto done; }
        mode = ri_str(mode_node) ? ri_str(mode_node) : "";
    }
    if (strcmp(mode, "interrupt") == 0 && !superseded) {
        json_t *internal = json_obj_get(ev, "internal");
        if (!ri_truthy(internal)) {
            json_t *cmd = ri_call(c, "event.get_command", json_object());
            if (ri_is_raise(cmd)) { ret = cmd; goto done; }
            bool has_cmd = ri_truthy(cmd);
            json_free(cmd);
            if (!has_cmd) {
                json_t *to = ri_text_only(c, args);
                if (ri_is_raise(to)) { ret = to; goto done; }
                bool text_only = ri_truthy(to);
                json_free(to);
                bool voice = false;
                if (!text_only) {
                    if (!ri_has(ev, "message_type")) { ret = ri_attr_bare("message_type"); goto done; }
                    voice = ri_msg_type_is(ev, "voice");
                }
                if (text_only || voice) {
                    json_t *ga = json_object();
                    json_set(ga, "source", source ? json_copy(source) : json_null());
                    json_set_hdr(ga, "key", key);
                    json_t *gr = ri_in_telegram_grace(c, ga);
                    json_free(ga);
                    if (ri_is_raise(gr)) { ret = gr; goto done; }
                    bool in_grace = ri_truthy(gr);
                    json_free(gr);
                    if (in_grace) { ret = json_bool(false); goto done; }
                }
            }
        }
    }
    json_t *st = ri_peek_state(c, key);
    json_t *running_agent = NULL;
    if (st) {
        if (!ri_has(st, "turn")) { ret = ri_attr_bare("turn"); goto done; }
        json_t *turn = json_obj_get(st, "turn");
        if (!ri_has(turn, "agent")) { ret = ri_attr_bare("agent"); goto done; }
        running_agent = json_obj_get(turn, "agent");
    }
    adapter = ri_call(c, "self._delivery_adapter_for", ri_a_source(source));
    if (ri_is_raise(adapter)) { ret = adapter; adapter = NULL; goto done; }
    bool draining = json_get_bool(c->state, "_draining", false);
    bool skip = draining || ri_is_null(adapter) || ri_is_null(running_agent)
                || ri_agent_is_sentinel(running_agent);
    if (!skip) {
        if (!ri_has(ev, "message_type")) { ret = ri_attr_bare("message_type"); goto done; }
        if (ri_msg_type_is(ev, "text") && strcmp(mode, "steer") != 0 && !superseded) {
            json_t *tm = ri_call(c, "self._effective_busy_text_mode", ri_a_source(source));
            if (ri_is_raise(tm)) { ret = tm; goto done; }
            bool queue = ri_str_eq(tm, "queue");
            json_free(tm);
            if (queue) skip = true;
        }
    }
    if (skip) { ret = json_bool(false); goto done; }
    json_t *oa = json_object();
    json_set(oa, "event", ev ? json_copy(ev) : json_null());
    json_set_hdr(oa, "key", key);
    json_set_hdr(oa, "mode", mode);
    json_set(oa, "agent", ri_agent_proj(running_agent));
    outcome = ri_call(c, "self._resolve_busy_steer_or_redirect", oa);
    if (ri_is_raise(outcome)) { ret = outcome; outcome = NULL; goto done; }
    json_t *na = json_object();
    json_set(na, "event", ev ? json_copy(ev) : json_null());
    json_set(na, "outcome", outcome ? json_copy(outcome) : json_null());
    json_t *nr = ri_call(c, "note_busy_outcome", na);
    if (ri_is_raise(nr)) { ret = nr; goto done; }
    json_free(nr);
    json_t *aa = json_object();
    json_set(aa, "event", ev ? json_copy(ev) : json_null());
    json_set_hdr(aa, "key", key);
    json_set(aa, "adapter", ri_adapter_id(adapter));
    json_set(aa, "state", st ? json_copy(st) : json_null());
    json_set(aa, "agent", ri_agent_proj(running_agent));
    json_set(aa, "outcome", outcome ? json_copy(outcome) : json_null());
    json_t *ar = ri_call(c, "self._busy_interrupt_and_ack", aa);
    if (ri_is_raise(ar)) { ret = ar; goto done; }
    json_free(ar);
    if (ri_is_null(outcome)) { ret = ri_attr_none("steered"); goto done; }
    if (!ri_has(outcome, "steered")) { ret = ri_attr_ns("steered"); goto done; }
    bool steered = ri_truthy(json_obj_get(outcome, "steered"));
    if (steered) { ret = json_bool(true); goto done; }
    if (!ri_has(outcome, "redirected")) { ret = ri_attr_ns("redirected"); goto done; }
    bool redirected = ri_truthy(json_obj_get(outcome, "redirected"));
    ret = json_bool(redirected);
done:
    json_free(mode_node);
    json_free(adapter);
    json_free(outcome);
    return ret;
}

/* ── event.text mutation (EventDouble.__setattr__ model) ── */
/* Takes ownership of val; frees it without writing when the double raises. */
static json_t *ri_event_set_text(ri_ctx_t *c, json_t *val) {
    json_t *ev = c->event;
    json_t *tr = json_obj_get(ev, "text_raise");
    if (tr && tr->type == JSON_OBJECT) { json_free(val); return ri_make_exc(tr); }
    if (json_get_bool(ev, "text_readonly", false)) {
        json_free(val);
        return ri_err("AttributeError", "text readonly");
    }
    json_set(ev, "text", val);
    return NULL;
}

/* ", ".join(seq) raise-check (logger args are evaluated even when silent). */
static json_t *ri_join_check(const json_t *seq) {
    json_t *items = NULL;
    if (seq && seq->type == JSON_ARRAY) items = json_copy(seq);
    else if (seq && seq->type == JSON_STRING) items = ri_utf8_chars(seq->str_val);
    else if (seq && seq->type == JSON_OBJECT) {
        items = json_array();
        for (size_t i = 0; i < seq->c.count; i++)
            json_append(items, json_string(seq->c.keys[i]));
    } else return ri_not_iterable(seq);
    json_t *err = NULL;
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *it = json_get(items, i);
        if (!it || it->type != JSON_STRING) {
            char buf[160];
            snprintf(buf, sizeof buf, "sequence item %zu: expected str instance, %s found",
                     i, ri_type_name(it));
            err = ri_err("TypeError", buf);
            break;
        }
    }
    json_free(items);
    return err;
}

/* 3-tuple unpack (py3.14 messages). On success *out3 is an OWNED 3-item array. */
static json_t *ri_unpack3(const json_t *v, json_t **out3) {
    *out3 = NULL;
    json_t *items = NULL;
    bool is_str = false;
    if (v && v->type == JSON_ARRAY) items = json_copy(v);
    else if (v && v->type == JSON_STRING) { items = ri_utf8_chars(v->str_val); is_str = true; }
    else if (v && v->type == JSON_OBJECT) {
        items = json_array();
        for (size_t i = 0; i < v->c.count; i++)
            json_append(items, json_string(v->c.keys[i]));
    } else {
        char buf[128];
        snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", ri_type_name(v));
        return ri_err("TypeError", buf);
    }
    size_t n = json_len(items);
    if (n < 3) {
        char buf[128];
        snprintf(buf, sizeof buf, "not enough values to unpack (expected 3, got %zu)", n);
        json_free(items);
        return ri_err("ValueError", buf);
    }
    if (n > 3) {
        char buf[128];
        if (is_str) snprintf(buf, sizeof buf, "too many values to unpack (expected 3)");
        else snprintf(buf, sizeof buf, "too many values to unpack (expected 3, got %zu)", n);
        json_free(items);
        return ri_err("ValueError", buf);
    }
    *out3 = items;
    return NULL;
}

/* ══════════════ Group E: command leaves ══════════════ */

/* PoP: ri_send_payload_as_turn @ gateway/run_inbound.py:_hm_send_payload_as_turn */
static json_t *ri_send_payload_as_turn(ri_ctx_t *c, const json_t *args) {
    const char *usage = json_get_str(args, "usage", "");
    json_t *ev = c->event;
    json_t *ca = ri_call(c, "event.get_command_args", json_object());
    RI_CHK(ca);
    if (ri_is_null(ca)) { json_free(ca); return ri_attr_none("strip"); }
    if (ca->type != JSON_STRING) {
        json_t *e = ri_attr_err(ri_type_name(ca), "strip");
        json_free(ca);
        return e;
    }
    char *payload = ri_strip(ca->str_val);
    json_free(ca);
    if (!payload || !payload[0]) {
        if (!ri_has(ev, "media_urls")) { free(payload); return ri_attr_bare("media_urls"); }
        if (!ri_truthy(json_obj_get(ev, "media_urls"))) {
            free(payload);
            json_t *out = json_array();
            json_append(out, json_bool(true));
            json_append(out, json_string(usage));
            return out;
        }
    }
    json_t *px = ri_event_set_text(c, json_string(payload ? payload : ""));
    free(payload);
    if (px) {
        json_t *pp = ri_catch_exception(px);
        if (pp) return pp;
    }
    json_t *out = json_array();
    json_append(out, json_bool(false));
    json_append(out, json_null());
    return out;
}

/* PoP: ri_unknown_slash_reply @ gateway/run_inbound.py:_hm_unknown_slash_reply */
static json_t *ri_unknown_slash_reply(ri_ctx_t *c, const json_t *args) {
    const char *command = json_get_str(args, "command", "");
    json_t *source = json_obj_get(args, "source");
    char *norm = ri_replace_char(command, '_', '-');
    bool known = false;
    for (size_t i = 0; i < RI_N_KNOWN; i++)
        if (strcmp(norm, RI_KNOWN_COMMANDS[i]) == 0) { known = true; break; }
    free(norm);
    if (known) return NULL;
    json_t *ua = json_object();
    json_set_hdr(ua, "command", command);
    json_t *unavail = ri_call(c, "_check_unavailable_skill", ua);
    RI_CHK(unavail);
    if (ri_truthy(unavail)) return unavail;
    json_free(unavail);
    /* logger.warning arg: source.platform.value if source.platform else "?" */
    if (ri_is_null(source)) return ri_attr_none("platform");
    if (!ri_has(source, "platform")) return ri_attr_ns("platform");
    json_t *ta = json_object();
    json_set_hdr(ta, "key", "gateway.unknown_command");
    json_set_hdr(ta, "command", command);
    return ri_call(c, "t", ta);
}

/* PoP: ri_bundle_slash_rewrite @ gateway/run_inbound.py:_hm_bundle_slash_rewrite */
static json_t *ri_bundle_slash_rewrite(ri_ctx_t *c, const json_t *args) {
    json_t *source = json_obj_get(args, "source");
    const char *key = json_get_str(args, "key", "");
    const char *command = json_get_str(args, "command", "");
    json_t *ret = NULL;
    json_t *bk = NULL, *ca = NULL, *br = NULL, *items = NULL, *platform = json_null();
    char *instruction = NULL;
    json_t *ba = json_object();
    json_set_hdr(ba, "command", command);
    bk = ri_call(c, "resolve_bundle_command_key", ba);
    if (ri_is_raise(bk)) { ret = bk; bk = NULL; goto done; }
    if (ri_is_null(bk)) { ret = json_bool(false); goto done; }
    ca = ri_call(c, "event.get_command_args", json_object());
    if (ri_is_raise(ca)) { ret = ca; ca = NULL; goto done; }
    if (ri_is_null(ca)) { ret = ri_attr_none("strip"); goto done; }
    if (ca->type != JSON_STRING) { ret = ri_attr_err(ri_type_name(ca), "strip"); goto done; }
    instruction = ri_strip(ca->str_val);
    if (ri_is_null(source)) { ret = ri_attr_none("platform"); goto done; }
    if (!ri_has(source, "platform")) { ret = ri_attr_ns("platform"); goto done; }
    json_t *p = json_obj_get(source, "platform");
    if (ri_truthy(p)) { json_free(platform); platform = json_copy(p); }
    json_t *bua = json_object();
    json_set(bua, "bundle_key", json_copy(bk));
    json_set_hdr(bua, "instruction", instruction ? instruction : "");
    json_set_hdr(bua, "task_id", key);
    json_set(bua, "platform", platform);
    platform = NULL;
    br = ri_call(c, "build_bundle_invocation_message", bua);
    if (ri_is_raise(br)) { ret = br; br = NULL; goto done; }
    if (!ri_truthy(br)) { ret = json_bool(false); goto done; }
    json_t *uerr = ri_unpack3(br, &items);
    if (uerr) { ret = uerr; goto done; }
    json_t *serr = ri_event_set_text(c, json_copy(json_get(items, 0)));
    if (serr) { ret = serr; goto done; }
    json_t *missing = json_get(items, 2);
    if (ri_truthy(missing)) {
        json_t *jerr = ri_join_check(missing);
        if (jerr) { ret = jerr; goto done; }
    }
    ret = json_bool(true);
done:
    free(instruction);
    json_free(bk);
    json_free(ca);
    json_free(br);
    json_free(items);
    json_free(platform);
    if (ri_is_raise(ret)) {
        json_t *px = ri_catch_exception(ret);
        return px ? px : json_bool(false);
    }
    return ret;
}

/* ══════════════ Group F: state leaves ══════════════ */

/* PoP: ri_consume_pending_native_image_paths @ gateway/run_inbound.py:_consume_pending_native_image_paths */
static json_t *ri_consume_pending_native_image_paths(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    json_t *st = ri_peek_state(c, key);
    if (!st) return json_array();
    if (!ri_has(st, "persistent")) return ri_attr_bare("persistent");
    json_t *persistent = json_obj_get(st, "persistent");
    if (!ri_has(persistent, "native_image_paths")) return ri_attr_bare("native_image_paths");
    json_t *nip = json_obj_get(persistent, "native_image_paths");
    json_t *paths = NULL;
    if (!ri_truthy(nip)) paths = json_array();
    else if (nip->type == JSON_ARRAY) paths = json_copy(nip);
    else if (nip->type == JSON_STRING) paths = ri_utf8_chars(nip->str_val);
    else if (nip->type == JSON_OBJECT) {
        paths = json_array();
        for (size_t i = 0; i < nip->c.count; i++)
            json_append(paths, json_string(nip->c.keys[i]));
    } else return ri_not_iterable(nip);
    if (ri_truthy(paths))
        json_set(persistent, "native_image_paths", json_array());
    return paths;
}

/* PoP: ri_inbound_original_names @ gateway/run_inbound.py:_inbound_original_names */
static json_t *ri_inbound_original_names(ri_ctx_t *c, const json_t *args) {
    (void)args;
    json_t *ev = c->event;
    if (!ri_has(ev, "media_urls")) return ri_attr_bare("media_urls");
    json_t *urls = json_obj_get(ev, "media_urls");
    json_t *names = json_obj_get(ev, "media_filenames");
    json_t *out = json_object();
    if (ri_truthy(urls) && urls->type == JSON_ARRAY && ri_truthy(names) && names->type == JSON_ARRAY) {
        size_t n = json_len(urls);
        if (json_len(names) < n) n = json_len(names);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_get(urls, i);
            json_t *nm = json_get(names, i);
            if (!ri_truthy(nm)) continue;
            const char *pk = ri_str(p);
            if (pk) {
                json_set(out, pk, json_copy(nm));
            } else if (p && (p->type == JSON_ARRAY || p->type == JSON_OBJECT)) {
                char buf[160];
                snprintf(buf, sizeof buf, "cannot use '%s' as a dict key (unhashable type: '%s')",
                         ri_type_name(p), ri_type_name(p));
                json_free(out);
                return ri_err("TypeError", buf);
            } else {
                char *ks = ri_id_str(p);
                json_set(out, ks ? ks : "", json_copy(nm));
                free(ks);
            }
        }
    }
    return out;
}

/* PoP: ri_restore_pending_one_turn_model_override @ gateway/run_inbound.py:_restore_pending_one_turn_model_override */
static json_t *ri_restore_pending_one_turn_model_override(ri_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    if (!key || !key[0]) return NULL;
    json_t *ret = NULL;
    json_t *st = ri_peek_state(c, key);
    if (!st) return NULL;
    if (!ri_has(st, "conversation")) { ret = ri_attr_bare("conversation"); goto done; }
    json_t *conv = json_obj_get(st, "conversation");
    if (!ri_has(conv, "one_turn_restore")) { ret = ri_attr_bare("one_turn_restore"); goto done; }
    json_t *otr = json_obj_get(conv, "one_turn_restore");
    if (!ri_truthy(otr)) return NULL;
    json_t *rg = json_obj_get(args, "run_generation");
    if (!ri_is_null(rg)) {
        json_t *ca = json_object();
        json_set_hdr(ca, "key", key);
        json_set(ca, "run_generation", json_copy(rg));
        json_t *cur = ri_call(c, "self._is_session_run_current", ca);
        if (ri_is_raise(cur)) { ret = cur; goto done; }
        bool current = ri_truthy(cur);
        json_free(cur);
        if (!current) return NULL;
    }
    json_t *snapshot = json_copy(otr);
    json_set(conv, "one_turn_restore", json_null());
    json_t *ra = json_object();
    json_set_hdr(ra, "key", key);
    json_set(ra, "snapshot", snapshot);
    json_t *rr = ri_call(c, "self._restore_session_model_override", ra);
    if (ri_is_raise(rr)) { ret = rr; goto done; }
    json_free(rr);
    json_t *pn = json_obj_get(c->state, "_pending_model_notes");
    if (!pn || pn->type != JSON_OBJECT) {
        pn = json_object();
        json_set(c->state, "_pending_model_notes", pn);
    }
    json_set_hdr(pn, key, RI_ONE_TURN_NOTE);
    return NULL;
done:
    if (ri_is_raise(ret)) {
        json_t *px = ri_catch_exception(ret);
        return px ? px : NULL;
    }
    return ret;
}
