/*
 * gateway_session_flow.c — FLOW cores + leaves of gateway/session.py
 * (hermes-agent): the PII/redaction decision helpers, the platform-note
 * builders, the routing-index mutation flows (release/restore/rekey/purge/
 * remove), the route-transition apply/recover/create cores and the small
 * self-model accessors (_lazy / _has_active_processes_safe / lookup / yolo).
 *
 * Decision sequences only: every live collaborator call (claimed siblings
 * is_shared_multi_user_session / _slack_tools_loaded / _discord_tools_loaded,
 * the persistence mixin verbs _entry_locked / _ensure_loaded_locked / _save /
 * _persistence_for_key / _reopen_session_row / _announce_route /
 * _heal_compression_tip_locked / _query_recoverable_session /
 * _session_create_kwargs, the identity helpers canonical_whatsapp_identifier /
 * transport_profile_of / tag_safe_display_name, the lifecycle _new_session_id,
 * the platform_registry lookup and logger calls) enters through ONE universal
 * seam: gse_call(ctx, name, args). The harness wires it to a recording stub fed
 * the case's scripted results (in.io); the oracle installs recording monkeypatch
 * stubs emitting the identical event shape {"name","args","ret"} from the SAME
 * scripts. Ported targets compose through real C calls (schedule_reset inside
 * _apply_route_checks); the oracle runs the extracted live methods, which
 * likewise compose.
 *
 * _update_entry (session_lifecycle.py, unclaimed, outside this module) is modeled
 * by its contract at the set_session_yolo call site: it resolves the entry via
 * the _entry_locked seam, runs the mutate callback, and full-saves via the _save
 * seam only when the entry exists and the callback did not return False. The
 * callback's effect is observable in out.state._entries.
 *
 * Self-contained: the harness #includes this .c directly; libjson only. All
 * symbols are static and gse_-prefixed.
 *
 * Representation contract (shared with tests/oracle_gateway_session_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; the BaseException
 *   trio (SystemExit/KeyboardInterrupt/GeneratorExit) passes through every
 *   `except Exception` site.
 * - SessionSource / SessionEntry / _RouteDecision travel as FULL field dicts
 *   (dataclass defaults filled on both sides). Platform members travel as their
 *   .value strings; datetime fields as canonical round-tripping ISO strings
 *   (datetime.fromisoformat(s).isoformat() == s); _rows_pending as a SORTED id
 *   array. origin is a nested full SessionSource dict.
 * - _entry_locked returns the STORED _entries[key] object (live identity): the
 *   script gates null/raise and supplies the trace ret; fixtures keep the script
 *   equal to the stored entry so mutations echo into out.state.
 * - logger calls are seams (logger.warning / logger.info) recording {msg, args}
 *   with args VERBATIM (the exc slot carries str(exc)).
 * - Scripted seams: a plain value (reused), a bare array (per-call queue, last
 *   value does NOT repeat), or {"__script__": true, "ret": v, "raise": spec}.
 *   A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * - Fixture corpus is ASCII; the live note constants carry a UTF-8 em-dash
 *   embedded as hex escapes. Scripted raise kinds are plain-message classes
 *   (KeyError excluded: str(KeyError(k)) == repr(k)).
 *
 * Production default (no seam channel installed) is fail-closed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct gse_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} gse_ctx_t;

/* ── basic helpers ── */
static bool gse_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool gse_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static bool gse_truthy(const json_t *v) {
    if (gse_is_null(v)) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *gse_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool gse_str_eq(const json_t *v, const char *s) {
    const char *t = gse_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* exceptions caught by `except Exception` (not the BaseException trio) */
static bool gse_is_base_exc(const json_t *r) {
    if (!gse_is_raise(r)) return false;
    const char *t = json_get_str(r, "__raise__", "");
    return strcmp(t, "SystemExit") == 0 || strcmp(t, "KeyboardInterrupt") == 0
        || strcmp(t, "GeneratorExit") == 0;
}

static json_t *gse_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static const char *gse_typename(const json_t *v) {
    if (gse_is_null(v)) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *gse_attr_error(const json_t *v, const char *attr) {
    char msg[192];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", gse_typename(v), attr);
    return gse_err("AttributeError", msg);
}

static json_t *gse_self_attr_error(const char *attr) {
    char msg[192];
    snprintf(msg, sizeof msg, "'types.SimpleNamespace' object has no attribute '%s'", attr);
    return gse_err("AttributeError", msg);
}

static char *gse_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static char *gse_fmt(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return gse_strdup("");
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : gse_strdup("");
}

/* Python `is` for the bool/null tri-state domain (None singletons, bool singletons). */
static bool gse_identical(const json_t *a, const json_t *b) {
    bool an = gse_is_null(a), bn = gse_is_null(b);
    if (an || bn) return an && bn;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    return false;
}

/* Python == for string/null (entry.session_id == snapshot_sid). */
static bool gse_eq_str(const json_t *a, const json_t *b) {
    if (gse_is_null(a) || gse_is_null(b)) return gse_is_null(a) && gse_is_null(b);
    const char *x = gse_str(a), *y = gse_str(b);
    return x && y && strcmp(x, y) == 0;
}

/* ASCII repr of a string (single quotes; escape backslash, quote, controls). */
static char *gse_py_repr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = malloc(n * 4 + 3);
    if (!out) return gse_strdup("''");
    size_t k = 0;
    out[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\'') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20) { k += (size_t)sprintf(out + k, "\\x%02x", c); }
        else out[k++] = (char)c;
    }
    out[k++] = '\'';
    out[k] = '\0';
    return out;
}

/* str.strip() over the ASCII White_Space set (fixture corpus is ASCII). */
static char *gse_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    #define GSE_WS(c) ((c)==' '||(c)=='\t'||(c)=='\n'||(c)=='\r'||(c)=='\v'||(c)=='\f'|| \
                       (c)=='\x1c'||(c)=='\x1d'||(c)=='\x1e'||(c)=='\x1f')
    while (a < b && GSE_WS((unsigned char)s[a])) a++;
    while (b > a && GSE_WS((unsigned char)s[b - 1])) b--;
    #undef GSE_WS
    char *o = malloc(b - a + 1);
    if (!o) return gse_strdup("");
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

static bool gse_startswith(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

/* ── seam dispatch ── */
static json_t *gse_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return gse_err(type, msg);
}

static json_t *gse_entry_result(const json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return gse_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

/* Record one seam event and answer its script. args is OWNED (moved into the
 * trace event). The result is a NEW node the caller frees. */
static json_t *gse_call(gse_ctx_t *c, const char *name, json_t *args) {
    if (!args) args = json_object();
    if (!c->io) {
        /* fail-closed: no seam channel installed */
        json_t *ev = json_object();
        json_set_hdr(ev, "name", name);
        json_set(ev, "args", args);
        char msg[256];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        json_t *res = gse_err("RuntimeError", msg);
        json_set(ev, "ret", json_copy(res));
        json_append(c->trace, ev);
        return res;
    }
    const json_t *entry = json_obj_get(c->io, name);
    json_t *res;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        res = gse_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(c->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            res = gse_err("RuntimeError", msg);
        } else {
            json_set(c->cursors, name, json_number(cur + 1));
            res = gse_entry_result(json_get(entry, idx));
        }
    } else {
        res = gse_entry_result(entry);
    }
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args);
    json_set(ev, "ret", res ? json_copy(res) : json_null());
    json_append(c->trace, ev);
    return res;
}

/* ── state helpers ── */
static json_t *gse_sub(gse_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *gse_sub_ensure(gse_ctx_t *c, const char *key) {
    json_t *v = gse_sub(c, key);
    if (!v) { v = json_object(); json_set(c->state, key, v); }
    return v;
}

/* set-valued state (_rows_pending) is a SORTED ARRAY. */
static json_t *gse_set_ensure(gse_ctx_t *c, const char *key) {
    json_t *v = json_obj_get(c->state, key);
    if (!v || v->type != JSON_ARRAY) { v = json_array(); json_set(c->state, key, v); }
    return v;
}

static bool gse_set_has(const json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return false;
    for (size_t i = 0; i < json_len(arr); i++)
        if (gse_str_eq(json_get(arr, i), id)) return true;
    return false;
}

static void gse_set_add(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return;
    if (gse_set_has(arr, id)) return;
    size_t n = json_len(arr), pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *s = gse_str(json_get(arr, i));
        if (s && strcmp(s, id) > 0) { pos = i; break; }
    }
    json_t *copy = json_array();
    for (size_t i = 0; i < n; i++) {
        if (i == pos) json_append(copy, json_string(id));
        json_append(copy, json_copy(json_get(arr, i)));
    }
    if (pos == n) json_append(copy, json_string(id));
    for (size_t i = 0; i < arr->c.count; i++) {
        json_free(arr->c.items[i]);
        free(arr->c.keys ? arr->c.keys[i] : NULL);
    }
    free(arr->c.items); free(arr->c.keys);
    arr->c.items = copy->c.items; arr->c.keys = copy->c.keys; arr->c.count = copy->c.count;
    copy->c.items = NULL; copy->c.keys = NULL; copy->c.count = 0;
    json_free(copy);
}

/* fill dst[key] from src[key] when present (incl null), else owned default. */
static void gse_fill(json_t *dst, const json_t *src, const char *key, json_t *defval) {
    json_t *v = src ? json_obj_get(src, key) : NULL;
    json_set(dst, key, v ? json_copy(v) : defval);
}

/* ── dataclass materialization (full field dicts, defaults filled) ── */
/* SessionSource: 24 fields; __post_init__ mirrors scope_id/guild_id. */
static json_t *gse_make_source(const json_t *p) {
    json_t *s = json_object();
    gse_fill(s, p, "platform", json_string(""));
    gse_fill(s, p, "chat_id", json_string(""));
    gse_fill(s, p, "chat_name", json_null());
    gse_fill(s, p, "chat_type", json_string("dm"));
    gse_fill(s, p, "user_id", json_null());
    gse_fill(s, p, "user_name", json_null());
    gse_fill(s, p, "thread_id", json_null());
    gse_fill(s, p, "chat_topic", json_null());
    gse_fill(s, p, "user_id_alt", json_null());
    gse_fill(s, p, "chat_id_alt", json_null());
    gse_fill(s, p, "is_bot", json_bool(false));
    gse_fill(s, p, "scope_id", json_null());
    gse_fill(s, p, "guild_id", json_null());
    gse_fill(s, p, "parent_chat_id", json_null());
    gse_fill(s, p, "message_id", json_null());
    gse_fill(s, p, "role_authorized", json_bool(false));
    gse_fill(s, p, "profile", json_null());
    gse_fill(s, p, "profile_route_rejected", json_bool(false));
    gse_fill(s, p, "auto_thread_created", json_bool(false));
    gse_fill(s, p, "auto_thread_initial_name", json_null());
    gse_fill(s, p, "prospective_thread_id", json_null());
    gse_fill(s, p, "shared_session", json_bool(false));
    gse_fill(s, p, "delivered_via_upstream_relay", json_bool(false));
    gse_fill(s, p, "detached_route", json_null());
    json_t *scope = json_obj_get(s, "scope_id");
    json_t *guild = json_obj_get(s, "guild_id");
    if (gse_is_null(scope) && !gse_is_null(guild)) json_set(s, "scope_id", json_copy(guild));
    else if (!gse_is_null(scope)) json_set(s, "guild_id", json_copy(scope));
    return s;
}

/* SessionEntry: 33 fields. origin -> full source dict; metadata -> object. */
static json_t *gse_make_entry(const json_t *p) {
    json_t *e = json_object();
    gse_fill(e, p, "session_key", json_string(""));
    gse_fill(e, p, "session_id", json_string(""));
    gse_fill(e, p, "created_at", json_string(""));
    gse_fill(e, p, "updated_at", json_string(""));
    json_t *ov = p ? json_obj_get(p, "origin") : NULL;
    json_set(e, "origin", (ov && ov->type == JSON_OBJECT) ? gse_make_source(ov) : json_null());
    gse_fill(e, p, "display_name", json_null());
    gse_fill(e, p, "platform", json_null());
    gse_fill(e, p, "chat_type", json_string("dm"));
    json_t *mv = p ? json_obj_get(p, "metadata") : NULL;
    json_set(e, "metadata", (mv && mv->type == JSON_OBJECT) ? json_copy(mv) : json_object());
    gse_fill(e, p, "input_tokens", json_number(0));
    gse_fill(e, p, "output_tokens", json_number(0));
    gse_fill(e, p, "cache_read_tokens", json_number(0));
    gse_fill(e, p, "cache_write_tokens", json_number(0));
    gse_fill(e, p, "total_tokens", json_number(0));
    gse_fill(e, p, "estimated_cost_usd", json_number(0.0));
    gse_fill(e, p, "cost_status", json_string("unknown"));
    gse_fill(e, p, "last_prompt_tokens", json_number(0));
    gse_fill(e, p, "was_auto_reset", json_bool(false));
    gse_fill(e, p, "auto_reset_reason", json_null());
    gse_fill(e, p, "reset_had_activity", json_bool(false));
    gse_fill(e, p, "prev_session_id", json_null());
    gse_fill(e, p, "is_fresh_reset", json_bool(false));
    gse_fill(e, p, "expiry_finalized", json_bool(false));
    gse_fill(e, p, "suspended", json_bool(false));
    gse_fill(e, p, "resume_pending", json_bool(false));
    gse_fill(e, p, "resume_reason", json_null());
    gse_fill(e, p, "last_resume_marked_at", json_null());
    gse_fill(e, p, "active_turn_token", json_null());
    gse_fill(e, p, "active_turn_started_at", json_null());
    gse_fill(e, p, "model_override", json_null());
    gse_fill(e, p, "transport_profile", json_null());
    gse_fill(e, p, "prompt_pin", json_null());
    gse_fill(e, p, "yolo", json_null());
    return e;
}

/* _RouteDecision: 7 fields. entry -> full entry dict or null. */
static json_t *gse_make_decision(const json_t *p) {
    json_t *d = json_object();
    json_t *ev = p ? json_obj_get(p, "entry") : NULL;
    json_set(d, "entry", (ev && ev->type == JSON_OBJECT) ? gse_make_entry(ev) : json_null());
    gse_fill(d, p, "needs_save", json_bool(false));
    gse_fill(d, p, "metadata_only_save", json_bool(false));
    gse_fill(d, p, "needs_recover", json_bool(false));
    gse_fill(d, p, "reset_reason", json_null());
    gse_fill(d, p, "reset_had_activity", json_bool(false));
    gse_fill(d, p, "prev_session_id", json_null());
    return d;
}

/* SessionEntry field names (for _replace_route_locked **fields binding). */
static const char *GSE_ENTRY_FIELDS[] = {
    "session_key", "session_id", "created_at", "updated_at", "origin", "display_name",
    "platform", "chat_type", "metadata", "input_tokens", "output_tokens",
    "cache_read_tokens", "cache_write_tokens", "total_tokens", "estimated_cost_usd",
    "cost_status", "last_prompt_tokens", "was_auto_reset", "auto_reset_reason",
    "reset_had_activity", "prev_session_id", "is_fresh_reset", "expiry_finalized",
    "suspended", "resume_pending", "resume_reason", "last_resume_marked_at",
    "active_turn_token", "active_turn_started_at", "model_override", "transport_profile",
    "prompt_pin", "yolo",
};
/* kwargs passed explicitly by _replace_route_locked (collision -> multiple values). */
static const char *GSE_RRL_EXPLICIT[] = {
    "session_key", "session_id", "created_at", "updated_at",
    "origin", "platform", "chat_type", "transport_profile",
};

static bool gse_in_list(const char *const *list, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (strcmp(list[i], s) == 0) return true;
    return false;
}

/* Materialize the self-model state in place (mirrors the oracle setup):
 * _entries values -> full entry dicts; decision -> full decision dict;
 * _rows_pending -> sorted unique array. Idempotent on already-full dicts. */
static void gse_state_setup(json_t *state) {
    if (!state || state->type != JSON_OBJECT) return;
    json_t *entries = json_obj_get(state, "_entries");
    if (entries && entries->type == JSON_OBJECT) {
        for (size_t i = 0; i < entries->c.count; i++) {
            const char *key = entries->c.keys[i];
            json_t *full = gse_make_entry(entries->c.items[i]);
            json_set(entries, key, full);
        }
    }
    json_t *dec = json_obj_get(state, "decision");
    if (dec && dec->type == JSON_OBJECT) json_set(state, "decision", gse_make_decision(dec));
    json_t *rp = json_obj_get(state, "_rows_pending");
    if (rp && rp->type == JSON_ARRAY) {
        json_t *sorted = json_array();
        for (size_t i = 0; i < json_len(rp); i++) {
            const char *id = gse_str(json_get(rp, i));
            if (id) gse_set_add(sorted, id);
        }
        json_set(state, "_rows_pending", sorted);
    }
}

/* _entry_locked seam: script gates null/raise; the returned object is the
 * STORED _entries[key] double (live identity). Returns a BORROWED entry (or
 * NULL == None); *err carries an OWNED raise spec on a scripted raise. */
static json_t *gse_entry_locked(gse_ctx_t *c, const char *key, json_t **err) {
    *err = NULL;
    json_t *a = json_object();
    json_set_hdr(a, "session_key", key ? key : "");
    json_t *r = gse_call(c, "_entry_locked", a);
    if (gse_is_raise(r)) { *err = r; return NULL; }
    bool isnull = gse_is_null(r);
    json_free(r);
    if (isnull) return NULL;
    json_t *entries = gse_sub(c, "_entries");
    return entries ? json_obj_get(entries, key) : NULL;
}

/* ── live note constants (byte-exact; em-dash hex-escaped) ── */
#define GSE_SLACK_TOOLS_NOTE \
    "**Platform notes:** You are running inside Slack and have access to Slack-specific " \
    "tools this session. Consult the available Slack tool schemas for the exact operations " \
    "supported (e.g. channel history and thread lookups, posting, reactions) \xe2\x80\x94 use those " \
    "tools for Slack-specific requests, and do not promise Slack actions beyond what the " \
    "loaded tools actually expose."
#define GSE_SLACK_NO_TOOLS_NOTE \
    "**Platform notes:** You are running inside Slack. You do NOT have access to " \
    "Slack-specific APIs \xe2\x80\x94 you cannot search channel history, pin/unpin messages, manage " \
    "channels, or list users. Do not promise to perform these actions. The gateway may " \
    "inline the current message's Slack block/attachment payload when available, but you " \
    "still cannot call Slack APIs yourself."
#define GSE_SLACK_SHARED_NOTE \
    "In shared Slack threads, use the current turn's sender prefix as the only verified " \
    "current-author mention target. Do not guess or reuse `<@U...>` mentions from names, " \
    "memory, or prior conversation history."
#define GSE_DISCORD_IDS_HEADER "**Discord IDs (for the `discord` / `discord_admin` tools):**"
#define GSE_DISCORD_TRIG \
    "  - Triggering message: when available, its ID is provided per-turn in the incoming " \
    "user message (use it as `message_id` for reply/react/pin)"
#define GSE_DISCORD_NO_TOOLS_NOTE \
    "**Platform notes:** You are running inside Discord. You do NOT have access to " \
    "Discord-specific APIs \xe2\x80\x94 you cannot search channel history, pin messages, manage " \
    "roles, or list server members. Do not promise to perform these actions. If the user " \
    "asks, explain that you can only read messages sent directly to you and respond."
#define GSE_VOICE_NOTE \
    "Voice-channel state, when relevant, appears in the current message as a " \
    "`[Voice channel now: ...]` note."
#define GSE_THREAD_SUFFIX "` (use as `channel_id` for fetch_messages etc.)"

/* _PII_SAFE_PLATFORMS membership (Platform .value strings). */
static bool gse_pii_safe(const char *plat) {
    return plat && (strcmp(plat, "whatsapp") == 0 || strcmp(plat, "signal") == 0
        || strcmp(plat, "telegram") == 0 || strcmp(plat, "bluebubbles") == 0);
}

/* ── forward declarations ── */
static json_t *gse_is_path_unsafe(gse_ctx_t *c, const json_t *args);
static json_t *gse_session_key_namespace(gse_ctx_t *c, const json_t *args);
static json_t *gse_describe(gse_ctx_t *c, const json_t *args);
static json_t *gse_canonical_participant(gse_ctx_t *c, const json_t *args);
static json_t *gse_should_redact_pii(gse_ctx_t *c, const json_t *args);
static json_t *gse_label_shared_sender(gse_ctx_t *c, const json_t *args);
static json_t *gse_slack_platform_notes(gse_ctx_t *c, const json_t *args);
static json_t *gse_discord_platform_notes(gse_ctx_t *c, const json_t *args);
static json_t *gse_schedule_reset(gse_ctx_t *c, const json_t *args);
static json_t *gse_lazy(gse_ctx_t *c, const json_t *args);
static json_t *gse_has_active_processes_safe(gse_ctx_t *c, const json_t *args);
static json_t *gse_lookup_by_session_key(gse_ctx_t *c, const json_t *args);
static json_t *gse_set_session_yolo(gse_ctx_t *c, const json_t *args);
static json_t *gse_release_route(gse_ctx_t *c, const json_t *args);
static json_t *gse_purge_profile_routing(gse_ctx_t *c, const json_t *args);
static json_t *gse_rekey_profile_routing(gse_ctx_t *c, const json_t *args);
static json_t *gse_remove_by_session_id(gse_ctx_t *c, const json_t *args);
static json_t *gse_restore_route(gse_ctx_t *c, const json_t *args);
static json_t *gse_replace_route_locked(gse_ctx_t *c, const json_t *args);
static json_t *gse_route_recover(gse_ctx_t *c, const json_t *args);
static json_t *gse_route_create(gse_ctx_t *c, const json_t *args);
static json_t *gse_apply_route_checks(gse_ctx_t *c, const json_t *args);

/* ── leaves ── */

/* PoP: gse_is_path_unsafe @ gateway/session.py:_is_path_unsafe */
static json_t *gse_is_path_unsafe(gse_ctx_t *c, const json_t *args) {
    (void)c;
    json_t *value = json_obj_get(args, "value");
    if (!gse_truthy(value)) return json_bool(false);
    const char *s = gse_str(value);
    if (!s) s = "";
    bool strict = json_get_bool(args, "strict", true);
    if (strstr(s, "..") || (strict && (strchr(s, '/') || strchr(s, '\\'))))
        return json_bool(true);
    if (!strict && (s[0] == '/' || s[0] == '\\'))
        return json_bool(true);
    size_t n = strlen(s);
    bool drive = n >= 2 && isalpha((unsigned char)s[0]) && s[1] == ':';
    return json_bool(drive);
}

/* PoP: gse_session_key_namespace @ gateway/session.py:_session_key_namespace */
static json_t *gse_session_key_namespace(gse_ctx_t *c, const json_t *args) {
    (void)c;
    const char *p = gse_str(json_obj_get(args, "profile"));
    if (!p || !p[0] || strcmp(p, "default") == 0) return json_string("agent:main");
    if (strcmp(p, "main") == 0) return json_string("agent:main~");
    char *r = gse_fmt("agent:%s", p);
    json_t *out = json_string(r);
    free(r);
    return out;
}

/* PoP: gse_describe @ gateway/session.py:_describe */
static json_t *gse_describe(gse_ctx_t *c, const json_t *args) {
    (void)c;
    const char *chat_type = json_get_str(args, "chat_type", "");
    const char *user_label = json_get_str(args, "user_label", "");
    const char *chat_label = json_get_str(args, "chat_label", "");
    if (strcmp(chat_type, "dm") == 0) {
        char *r = gse_fmt("DM with %s", user_label);
        json_t *out = json_string(r); free(r); return out;
    }
    const char *prefix = "";
    if (strcmp(chat_type, "group") == 0) prefix = "group: ";
    else if (strcmp(chat_type, "channel") == 0) prefix = "channel: ";
    char *r = gse_fmt("%s%s", prefix, chat_label);
    json_t *out = json_string(r); free(r); return out;
}

/* PoP: gse_canonical_participant @ gateway/session.py:_canonical_participant */
static json_t *gse_canonical_participant(gse_ctx_t *c, const json_t *args) {
    json_t *src = gse_make_source(json_obj_get(args, "source"));
    json_t *alt = json_obj_get(src, "user_id_alt");
    json_t *uid = json_obj_get(src, "user_id");
    json_t *pid = gse_truthy(alt) ? alt : uid;
    const char *plat = json_get_str(src, "platform", "");
    if (gse_truthy(pid) && strcmp(plat, "whatsapp") == 0) {
        json_t *a = json_object();
        json_set_hdr(a, "value", gse_str(pid) ? gse_str(pid) : "");
        json_t *r = gse_call(c, "canonical_whatsapp_identifier", a);
        if (gse_is_raise(r)) { json_free(src); return r; }
        if (gse_truthy(r)) { json_free(src); return r; }
        json_free(r);
    }
    json_t *ret = pid ? json_copy(pid) : json_null();
    json_free(src);
    return ret;
}

/* PoP: gse_should_redact_pii @ gateway/session.py:_should_redact_pii */
static json_t *gse_should_redact_pii(gse_ctx_t *c, const json_t *args) {
    bool enabled = gse_truthy(json_obj_get(args, "enabled"));
    const char *plat = json_get_str(args, "platform", "");
    if (!enabled || gse_pii_safe(plat)) return json_bool(enabled);
    /* try: from gateway.platform_registry import platform_registry (silent);
     *      entry = platform_registry.get(platform.value);
     *      return bool(entry and entry.pii_safe)
     * except Exception: return False */
    json_t *a = json_object();
    json_set_hdr(a, "platform", plat);
    json_t *entry = gse_call(c, "platform_registry.get", a);
    if (gse_is_raise(entry)) {
        if (gse_is_base_exc(entry)) return entry;
        json_free(entry);
        return json_bool(false);
    }
    bool res = false;
    if (gse_truthy(entry) && entry->type == JSON_OBJECT)
        res = gse_truthy(json_obj_get(entry, "pii_safe"));
    json_free(entry);
    return json_bool(res);
}

/* PoP: gse_label_shared_sender @ gateway/session.py:label_shared_sender */
static json_t *gse_label_shared_sender(gse_ctx_t *c, const json_t *args) {
    json_t *src = gse_make_source(json_obj_get(args, "source"));
    json_t *text = json_obj_get(args, "text");
    json_t *user_name = json_obj_get(src, "user_name");
    bool gspu = json_get_bool(args, "group_sessions_per_user", true);
    bool tspu = json_get_bool(args, "thread_sessions_per_user", false);
    bool shared = false;
    if (gse_truthy(user_name)) {
        json_t *a = json_object();
        json_set(a, "source", json_copy(src));
        json_set(a, "group_sessions_per_user", json_bool(gspu));
        json_set(a, "thread_sessions_per_user", json_bool(tspu));
        json_t *r = gse_call(c, "is_shared_multi_user_session", a);
        if (gse_is_raise(r)) { json_free(src); return r; }
        shared = gse_truthy(r);
        json_free(r);
    }
    if (!gse_truthy(user_name) || !shared) {
        json_free(src);
        return text ? json_copy(text) : json_null();
    }
    /* from gateway.platforms.helpers import tag_safe_display_name (silent) */
    json_t *a = json_object();
    json_set_hdr(a, "name", gse_str(user_name) ? gse_str(user_name) : "");
    json_t *name = gse_call(c, "tag_safe_display_name", a);
    if (gse_is_raise(name)) { json_free(src); return name; }
    char *namestr = gse_strdup(gse_str(name) ? gse_str(name) : "");
    json_free(name);
    const char *plat = json_get_str(src, "platform", "");
    json_t *uid = json_obj_get(src, "user_id");
    if (strcmp(plat, "slack") == 0 && gse_truthy(uid)) {
        char *n2 = gse_fmt("%s | Slack user <@%s>", namestr, gse_str(uid) ? gse_str(uid) : "");
        free(namestr);
        namestr = n2;
    }
    const char *t = gse_str(text) ? gse_str(text) : "";
    char *r = gse_fmt("[%s] %s", namestr, t);
    json_t *out = json_string(r);
    free(r); free(namestr); json_free(src);
    return out;
}

/* PoP: gse_slack_platform_notes @ gateway/session.py:_slack_platform_notes */
static json_t *gse_slack_platform_notes(gse_ctx_t *c, const json_t *args) {
    json_t *r = gse_call(c, "_slack_tools_loaded", json_object());
    if (gse_is_raise(r)) return r;
    bool loaded = gse_truthy(r);
    json_free(r);
    json_t *lines = json_array();
    json_append(lines, json_string(""));
    json_append(lines, json_string(loaded ? GSE_SLACK_TOOLS_NOTE : GSE_SLACK_NO_TOOLS_NOTE));
    json_t *ctxobj = json_obj_get(args, "context");
    bool shared = gse_truthy(ctxobj ? json_obj_get(ctxobj, "shared_multi_user_session") : NULL);
    if (shared) json_append(lines, json_string(GSE_SLACK_SHARED_NOTE));
    return lines;
}

/* PoP: gse_discord_platform_notes @ gateway/session.py:_discord_platform_notes */
static json_t *gse_discord_platform_notes(gse_ctx_t *c, const json_t *args) {
    json_t *r = gse_call(c, "_discord_tools_loaded", json_object());
    if (gse_is_raise(r)) return r;
    bool loaded = gse_truthy(r);
    json_free(r);
    json_t *lines = json_array();
    json_t *ctxobj = json_obj_get(args, "context");
    json_t *src = gse_make_source(ctxobj ? json_obj_get(ctxobj, "source") : NULL);
    if (loaded) {
        json_append(lines, json_string(""));
        json_append(lines, json_string(GSE_DISCORD_IDS_HEADER));
        json_t *guild = json_obj_get(src, "guild_id");
        if (gse_truthy(guild)) {
            char *g = gse_fmt("  - Guild: `%s`", gse_str(guild));
            json_append(lines, json_string(g)); free(g);
        }
        json_t *thread = json_obj_get(src, "thread_id");
        json_t *parent = json_obj_get(src, "parent_chat_id");
        if (gse_truthy(thread) && gse_truthy(parent)) {
            char *p = gse_fmt("  - Parent channel: `%s`", gse_str(parent));
            json_append(lines, json_string(p)); free(p);
            char *t = gse_fmt("  - Thread: `%s%s", gse_str(thread), GSE_THREAD_SUFFIX);
            json_append(lines, json_string(t)); free(t);
        } else {
            json_t *chat = json_obj_get(src, "chat_id");
            char *ch = gse_fmt("  - Channel: `%s`", gse_str(chat) ? gse_str(chat) : "");
            json_append(lines, json_string(ch)); free(ch);
        }
        json_append(lines, json_string(GSE_DISCORD_TRIG));
    } else {
        json_append(lines, json_string(""));
        json_append(lines, json_string(GSE_DISCORD_NO_TOOLS_NOTE));
    }
    json_append(lines, json_string(""));
    json_append(lines, json_string(GSE_VOICE_NOTE));
    json_free(src);
    return lines;
}

/* ── _RouteDecision.schedule_reset core (composed by _apply_route_checks) ── */
static void gse_schedule_reset_core(json_t *decision, const char *reason,
                                    bool had_activity, const json_t *ended) {
    json_set(decision, "reset_reason", json_string(reason ? reason : ""));
    json_set(decision, "reset_had_activity", json_bool(had_activity));
    json_t *sid = ended ? json_obj_get(ended, "session_id") : NULL;
    json_set(decision, "prev_session_id", sid ? json_copy(sid) : json_null());
}

/* PoP: gse_schedule_reset @ gateway/session.py:schedule_reset */
static json_t *gse_schedule_reset(gse_ctx_t *c, const json_t *args) {
    json_t *decision = gse_sub_ensure(c, "decision");
    const char *reason = json_get_str(args, "reason", "");
    bool had = gse_truthy(json_obj_get(args, "had_activity"));
    json_t *ended = json_obj_get(args, "ended");
    gse_schedule_reset_core(decision, reason, had, ended);
    return json_null();
}

/* PoP: gse_lazy @ gateway/session.py:_lazy */
static json_t *gse_lazy(gse_ctx_t *c, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    json_t *cur = json_obj_get(c->state, name);
    if (cur && cur->type != JSON_NULL) return json_copy(cur);
    json_t *r = gse_call(c, "factory", json_object());
    if (gse_is_raise(r)) return r;
    json_set(c->state, name, r ? json_copy(r) : json_null());
    return r ? r : json_null();
}

/* PoP: gse_has_active_processes_safe @ gateway/session.py:_has_active_processes_safe */
static json_t *gse_has_active_processes_safe(gse_ctx_t *c, const json_t *args) {
    json_t *fn = json_obj_get(c->state, "_has_active_processes_fn");
    if (!fn) return gse_self_attr_error("_has_active_processes_fn");
    if (gse_is_null(fn)) return json_bool(false);
    const char *session_key = json_get_str(args, "session_key", "");
    const char *context = json_get_str(args, "context", "");
    json_t *a = json_object();
    json_set_hdr(a, "session_key", session_key);
    json_t *r = gse_call(c, "_has_active_processes_fn", a);
    if (gse_is_raise(r)) {
        if (gse_is_base_exc(r)) return r;
        /* except Exception: logger.warning(...); return True */
        json_t *la = json_object();
        json_set_hdr(la, "msg",
            "has_active_processes_fn raised during %s for %s; keeping session alive: %s");
        json_t *arr = json_array();
        json_append(arr, json_string(context));
        json_append(arr, json_string(session_key));
        json_append(arr, json_string(json_get_str(r, "message", "")));
        json_set(la, "args", arr);
        json_t *lr = gse_call(c, "logger.warning", la);
        json_free(r);
        if (gse_is_raise(lr)) return lr;
        json_free(lr);
        return json_bool(true);
    }
    bool res = gse_truthy(r);
    json_free(r);
    return json_bool(res);
}

/* PoP: gse_lookup_by_session_key @ gateway/session.py:lookup_by_session_key */
static json_t *gse_lookup_by_session_key(gse_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    if (!key || !key[0]) return json_null();
    json_t *err = NULL;
    json_t *entry = gse_entry_locked(c, key, &err);
    if (err) return err;
    return entry ? json_copy(entry) : json_null();
}

/* PoP: gse_set_session_yolo @ gateway/session.py:set_session_yolo */
static json_t *gse_set_session_yolo(gse_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    json_t *enabled = json_obj_get(args, "enabled");
    /* _update_entry contract: entry = _entry_locked; if entry is None or
     * mutate(entry) is False -> False; else _save() -> True. */
    json_t *err = NULL;
    json_t *entry = gse_entry_locked(c, key, &err);
    if (err) return err;
    if (!entry) return json_bool(false);
    /* _apply: if entry.yolo is enabled: return False; entry.yolo = enabled */
    json_t *yolo = json_obj_get(entry, "yolo");
    if (gse_identical(yolo, enabled)) return json_bool(false);
    json_set(entry, "yolo", enabled ? json_copy(enabled) : json_null());
    json_t *sr = gse_call(c, "_save", json_object());
    if (gse_is_raise(sr)) return sr;
    json_free(sr);
    return json_bool(true);
}

/* PoP: gse_release_route @ gateway/session.py:release_route */
static json_t *gse_release_route(gse_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    const char *sid = json_get_str(args, "session_id", "");
    json_t *err = NULL;
    json_t *entry = gse_entry_locked(c, key, &err);
    if (err) return err;
    if (!entry) return json_bool(false);
    if (!gse_str_eq(json_obj_get(entry, "session_id"), sid)) return json_bool(false);
    json_t *entries = gse_sub(c, "_entries");
    if (entries) json_obj_del(entries, key);
    json_t *sr = gse_call(c, "_save", json_object());
    if (gse_is_raise(sr)) return sr;
    json_free(sr);
    return json_bool(true);
}

/* PoP: gse_purge_profile_routing @ gateway/session.py:purge_profile_routing */
static json_t *gse_purge_profile_routing(gse_ctx_t *c, const json_t *args) {
    char *name = gse_strip(gse_str(json_obj_get(args, "profile")));
    if (!name[0]) { free(name); return json_number(0); }
    char *ns = gse_fmt("agent:%s:", name);
    free(name);
    json_t *entries = gse_sub(c, "_entries");
    json_t *dropped = json_array();
    if (entries) {
        for (size_t i = 0; i < entries->c.count; i++) {
            const char *key = entries->c.keys[i];
            if (gse_startswith(key, ns)) json_append(dropped, json_string(key));
        }
    }
    size_t count = json_len(dropped);
    for (size_t i = 0; i < count; i++)
        if (entries) json_obj_del(entries, gse_str(json_get(dropped, i)));
    json_free(dropped);
    if (count > 0) {
        json_t *sr = gse_call(c, "_save", json_object());
        if (gse_is_raise(sr)) { free(ns); return sr; }
        json_free(sr);
    }
    free(ns);
    return json_number((double)count);
}

/* PoP: gse_rekey_profile_routing @ gateway/session.py:rekey_profile_routing */
static json_t *gse_rekey_profile_routing(gse_ctx_t *c, const json_t *args) {
    char *old = gse_strip(gse_str(json_obj_get(args, "old_name")));
    char *new = gse_strip(gse_str(json_obj_get(args, "new_name")));
    if (!old[0] || !new[0] || strcmp(old, new) == 0) {
        free(old); free(new); return json_number(0);
    }
    char *old_ns = gse_fmt("agent:%s:", old);
    char *new_ns = gse_fmt("agent:%s:", new);
    json_t *entries = gse_sub(c, "_entries");
    /* moving = [key for key in _entries if key.startswith(old_ns)] */
    json_t *moving = json_array();
    if (entries) {
        for (size_t i = 0; i < entries->c.count; i++) {
            const char *key = entries->c.keys[i];
            if (gse_startswith(key, old_ns)) json_append(moving, json_string(key));
        }
    }
    /* collisions: first new key already present */
    char *collision = NULL;
    size_t old_ns_len = strlen(old_ns);
    for (size_t i = 0; i < json_len(moving); i++) {
        const char *key = gse_str(json_get(moving, i));
        char *nk = gse_fmt("%s%s", new_ns, key + old_ns_len);
        if (entries && json_obj_get(entries, nk)) { collision = nk; break; }
        free(nk);
    }
    if (collision) {
        char *ro = gse_py_repr(old), *rn = gse_py_repr(new), *rc = gse_py_repr(collision);
        char *m = gse_fmt("profile routing collision while renaming %s to %s: %s already exists",
                          ro, rn, rc);
        json_t *e = gse_err("ValueError", m);
        free(m); free(ro); free(rn); free(rc); free(collision);
        json_free(moving); free(old_ns); free(new_ns); free(old); free(new);
        return e;
    }
    size_t count = json_len(moving);
    for (size_t i = 0; i < count; i++) {
        const char *key = gse_str(json_get(moving, i));
        char *nk = gse_fmt("%s%s", new_ns, key + old_ns_len);
        json_t *entry = entries ? json_obj_get(entries, key) : NULL;
        json_t *new_entry = entry ? json_copy(entry) : json_object();
        json_set(new_entry, "session_key", json_string(nk));
        json_t *origin = entry ? json_obj_get(entry, "origin") : NULL;
        if (origin && !gse_is_null(origin)
            && gse_str_eq(json_obj_get(origin, "profile"), old)) {
            json_t *no = json_copy(origin);
            json_set(no, "profile", json_string(new));
            json_set(new_entry, "origin", no);
        }
        if (entries) {
            json_obj_del(entries, key);
            json_set(entries, nk, new_entry);
        } else {
            json_free(new_entry);
        }
        free(nk);
    }
    json_free(moving);
    if (count > 0) {
        json_t *sr = gse_call(c, "_save", json_object());
        if (gse_is_raise(sr)) { free(old_ns); free(new_ns); free(old); free(new); return sr; }
        json_free(sr);
    }
    free(old_ns); free(new_ns); free(old); free(new);
    return json_number((double)count);
}

/* PoP: gse_remove_by_session_id @ gateway/session.py:remove_by_session_id */
static json_t *gse_remove_by_session_id(gse_ctx_t *c, const json_t *args) {
    const char *sid = json_get_str(args, "session_id", "");
    if (!sid || !sid[0]) return json_number(0);
    json_t *er = gse_call(c, "_ensure_loaded_locked", json_object());
    if (gse_is_raise(er)) return er;
    json_free(er);
    json_t *entries = gse_sub(c, "_entries");
    json_t *dropped = json_array();
    if (entries) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *entry = entries->c.items[i];
            if (gse_str_eq(json_obj_get(entry, "session_id"), sid))
                json_append(dropped, json_string(entries->c.keys[i]));
        }
    }
    size_t count = json_len(dropped);
    for (size_t i = 0; i < count; i++)
        if (entries) json_obj_del(entries, gse_str(json_get(dropped, i)));
    json_free(dropped);
    if (count > 0) {
        json_t *hints = json_obj_get(c->state, "_session_owner_hints");
        if (hints && hints->type != JSON_NULL) json_obj_del(hints, sid);
        json_t *sr = gse_call(c, "_save", json_object());
        if (gse_is_raise(sr)) return sr;
        json_free(sr);
        /* logger.info("SessionStore removed %d routing entr%s for deleted session %s",
         *             len(dropped), "y" if len==1 else "ies", session_id) */
        json_t *la = json_object();
        json_set_hdr(la, "msg", "SessionStore removed %d routing entr%s for deleted session %s");
        json_t *arr = json_array();
        json_append(arr, json_number((double)count));
        json_append(arr, json_string(count == 1 ? "y" : "ies"));
        json_append(arr, json_string(sid));
        json_set(la, "args", arr);
        json_t *lr = gse_call(c, "logger.info", la);
        if (gse_is_raise(lr)) return lr;
        json_free(lr);
    }
    return json_number((double)count);
}

/* PoP: gse_restore_route @ gateway/session.py:restore_route */
static json_t *gse_restore_route(gse_ctx_t *c, const json_t *args) {
    json_t *entry_arg = gse_make_entry(json_obj_get(args, "entry"));
    const char *key = json_get_str(entry_arg, "session_key", "");
    const char *esid = json_get_str(entry_arg, "session_id", "");
    const char *expected = json_get_str(args, "expected_session_id", "");
    json_t *err = NULL;
    json_t *current = gse_entry_locked(c, key, &err);
    if (err) { json_free(entry_arg); return err; }
    if (!current || !gse_str_eq(json_obj_get(current, "session_id"), expected)) {
        json_free(entry_arg);
        return json_bool(false);
    }
    json_t *entries = gse_sub_ensure(c, "_entries");
    json_set(entries, key, json_copy(entry_arg));
    json_t *sr = gse_call(c, "_save", json_object());
    if (gse_is_raise(sr)) { json_free(entry_arg); return sr; }
    json_free(sr);
    /* if self._persistence_for_key(entry.session_key): _reopen_session_row(...) */
    json_t *pa = json_object();
    json_set_hdr(pa, "session_key", key);
    json_t *pr = gse_call(c, "_persistence_for_key", pa);
    if (gse_is_raise(pr)) { json_free(entry_arg); return pr; }
    bool has = gse_truthy(pr);
    json_free(pr);
    if (has) {
        json_t *ra = json_object();
        json_set_hdr(ra, "session_key", key);
        json_set_hdr(ra, "session_id", esid);
        json_set_hdr(ra, "log_prefix", "");
        json_t *rr = gse_call(c, "_reopen_session_row", ra);
        if (gse_is_raise(rr)) { json_free(entry_arg); return rr; }
        json_free(rr);
    }
    json_t *aa = json_object();
    json_set_hdr(aa, "session_key", key);
    json_set(aa, "entry", json_copy(entry_arg));
    json_t *ar = gse_call(c, "_announce_route", aa);
    if (gse_is_raise(ar)) { json_free(entry_arg); return ar; }
    json_free(ar);
    json_free(entry_arg);
    return json_bool(true);
}

/* PoP: gse_replace_route_locked @ gateway/session.py:_replace_route_locked */
static json_t *gse_replace_route_locked(gse_ctx_t *c, const json_t *args) {
    const char *key = json_get_str(args, "session_key", "");
    const char *sid = json_get_str(args, "session_id", "");
    const char *now = json_get_str(args, "now", "");
    json_t *old = gse_make_entry(json_obj_get(args, "old_entry"));
    json_t *fields = json_obj_get(args, "fields");
    /* Method-level **fields binding: a fields key colliding with a positional
     * parameter raises at the CALL binding, BEFORE the body runs (no seam).
     * First colliding key in fields insertion order wins. */
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *fk = fields->c.keys[i];
            if (strcmp(fk, "session_key") == 0 || strcmp(fk, "old_entry") == 0
                || strcmp(fk, "session_id") == 0 || strcmp(fk, "now") == 0) {
                char *m = gse_fmt("SessionStore._replace_route_locked() got multiple values for argument '%s'", fk);
                json_t *e = gse_err("TypeError", m); free(m);
                json_free(old); return e;
            }
        }
    }
    json_t *new_entry = gse_make_entry(NULL);
    json_set(new_entry, "session_key", json_string(key));
    json_set(new_entry, "session_id", json_string(sid));
    json_set(new_entry, "created_at", json_string(now));
    json_set(new_entry, "updated_at", json_string(now));
    json_set(new_entry, "origin", json_copy(json_obj_get(old, "origin")));
    json_set(new_entry, "platform", json_copy(json_obj_get(old, "platform")));
    json_set(new_entry, "chat_type", json_copy(json_obj_get(old, "chat_type")));
    json_set(new_entry, "transport_profile", json_copy(json_obj_get(old, "transport_profile")));
    /* SessionEntry-level **fields binding: unknown kwarg -> unexpected keyword.
     * Explicit-kwarg collisions (origin/platform/chat_type/transport_profile/
     * created_at/updated_at) carry the oracle exec-module prefix in their live
     * message and are OUTSIDE the verified fixture domain (modeled best-effort). */
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++) {
            const char *fk = fields->c.keys[i];
            if (gse_in_list(GSE_RRL_EXPLICIT, sizeof(GSE_RRL_EXPLICIT)/sizeof(GSE_RRL_EXPLICIT[0]), fk)) {
                char *m = gse_fmt("SessionEntry.__init__() got multiple values for keyword argument '%s'", fk);
                json_t *e = gse_err("TypeError", m); free(m);
                json_free(old); json_free(new_entry); return e;
            }
            if (!gse_in_list(GSE_ENTRY_FIELDS, sizeof(GSE_ENTRY_FIELDS)/sizeof(GSE_ENTRY_FIELDS[0]), fk)) {
                char *m = gse_fmt("SessionEntry.__init__() got an unexpected keyword argument '%s'", fk);
                json_t *e = gse_err("TypeError", m); free(m);
                json_free(old); json_free(new_entry); return e;
            }
            json_set(new_entry, fk, json_copy(fields->c.items[i]));
        }
    }
    json_t *entries = gse_sub_ensure(c, "_entries");
    json_set(entries, key, json_copy(new_entry));
    json_t *sr = gse_call(c, "_save", json_object());
    if (gse_is_raise(sr)) { json_free(old); json_free(new_entry); return sr; }
    json_free(sr);
    /* _rows_pending.add AFTER the save: a raising save leaves nothing pending. */
    gse_set_add(gse_set_ensure(c, "_rows_pending"), sid);
    json_free(old);
    return new_entry;
}

/* PoP: gse_route_recover @ gateway/session.py:_route_recover */
static json_t *gse_route_recover(gse_ctx_t *c, const json_t *args) {
    json_t *decision = gse_sub_ensure(c, "decision");
    const char *key = json_get_str(args, "session_key", "");
    const char *now = json_get_str(args, "now", "");
    json_t *src = gse_make_source(json_obj_get(args, "source"));
    json_t *qa = json_object();
    json_set_hdr(qa, "session_key", key);
    json_set(qa, "source", json_copy(src));
    json_set_hdr(qa, "now", now);
    json_t *qr = gse_call(c, "_query_recoverable_session", qa);
    if (gse_is_raise(qr)) { json_free(src); return qr; }
    if (gse_is_null(qr)) { json_free(qr); json_free(src); return json_null(); }
    json_t *recovered = gse_make_entry(qr);
    json_free(qr);
    const char *rsid = json_get_str(recovered, "session_id", "");
    json_t *ra = json_object();
    json_set_hdr(ra, "session_key", key);
    json_set_hdr(ra, "session_id", rsid);
    json_set_hdr(ra, "log_prefix", "");
    json_t *rr = gse_call(c, "_reopen_session_row", ra);
    if (gse_is_raise(rr)) { json_free(recovered); json_free(src); return rr; }
    json_free(rr);
    /* decision.entry = self._entries.setdefault(session_key, recovered) */
    json_t *entries = gse_sub_ensure(c, "_entries");
    json_t *cur = json_obj_get(entries, key);
    if (!cur) {
        json_set(entries, key, json_copy(recovered));
        cur = json_obj_get(entries, key);
    }
    json_set(decision, "entry", cur ? json_copy(cur) : json_null());
    json_set(decision, "needs_save", json_bool(true));
    json_free(recovered);
    json_free(src);
    return json_null();
}

/* PoP: gse_route_create @ gateway/session.py:_route_create */
static json_t *gse_route_create(gse_ctx_t *c, const json_t *args) {
    json_t *decision = gse_sub_ensure(c, "decision");
    const char *key = json_get_str(args, "session_key", "");
    const char *now = json_get_str(args, "now", "");
    bool force_new = gse_truthy(json_obj_get(args, "force_new"));
    json_t *src = gse_make_source(json_obj_get(args, "source"));
    /* session_id = _new_session_id(now) */
    json_t *na = json_object();
    json_set_hdr(na, "now", now);
    json_t *nr = gse_call(c, "_new_session_id", na);
    if (gse_is_raise(nr)) { json_free(src); return nr; }
    char *sid = gse_strdup(gse_str(nr) ? gse_str(nr) : "");
    json_free(nr);
    /* candidate = SessionEntry(...) */
    json_t *cand = gse_make_entry(NULL);
    json_set(cand, "session_key", json_string(key));
    json_set(cand, "session_id", json_string(sid));
    json_set(cand, "created_at", json_string(now));
    json_set(cand, "updated_at", json_string(now));
    json_set(cand, "origin", json_copy(src));
    json_set(cand, "display_name", json_copy(json_obj_get(src, "chat_name")));
    json_set(cand, "platform", json_copy(json_obj_get(src, "platform")));
    json_set(cand, "chat_type", json_copy(json_obj_get(src, "chat_type")));
    json_t *dr = json_obj_get(decision, "reset_reason");
    json_set(cand, "was_auto_reset", json_bool(!gse_is_null(dr)));
    json_set(cand, "auto_reset_reason", dr ? json_copy(dr) : json_null());
    json_set(cand, "reset_had_activity", json_copy(json_obj_get(decision, "reset_had_activity")));
    json_set(cand, "prev_session_id", json_copy(json_obj_get(decision, "prev_session_id")));
    /* transport_profile = transport_profile_of(source) */
    json_t *ta = json_object();
    json_set(ta, "source", json_copy(src));
    json_t *tr = gse_call(c, "transport_profile_of", ta);
    if (gse_is_raise(tr)) { json_free(cand); free(sid); json_free(src); return tr; }
    json_set(cand, "transport_profile", tr ? json_copy(tr) : json_null());
    json_free(tr);
    /* create_kwargs = self._session_create_kwargs(...) */
    json_t *ca = json_object();
    json_set_hdr(ca, "session_id", sid);
    json_set_hdr(ca, "session_key", key);
    json_set(ca, "origin", json_copy(src));
    json_set(ca, "source_value", json_copy(json_obj_get(src, "platform")));
    json_set(ca, "display_name", json_copy(json_obj_get(src, "chat_name")));
    json_set(ca, "parent_session_id", json_copy(json_obj_get(decision, "prev_session_id")));
    json_t *ckr = gse_call(c, "_session_create_kwargs", ca);
    if (gse_is_raise(ckr)) { json_free(cand); free(sid); json_free(src); return ckr; }
    /* with lock: current = _entries.get(key); publish candidate if vacant or
     * (force_new and current is observed). */
    json_t *entries = gse_sub_ensure(c, "_entries");
    json_t *cur = json_obj_get(entries, key);
    bool observed_same = gse_truthy(json_obj_get(args, "observed_same"));
    bool replace = (!cur) || (force_new && observed_same);
    if (replace) {
        json_set(entries, key, json_copy(cand));
        gse_set_add(gse_set_ensure(c, "_rows_pending"), sid);
        cur = json_obj_get(entries, key);
    }
    json_set(decision, "entry", cur ? json_copy(cur) : json_null());
    json_set(decision, "needs_save", json_bool(true));
    json_free(cand);
    json_free(src);
    free(sid);
    /* return create_kwargs if current is candidate else None */
    if (replace) return ckr;
    json_free(ckr);
    return json_null();
}

/* PoP: gse_apply_route_checks @ gateway/session.py:_apply_route_checks */
static json_t *gse_apply_route_checks(gse_ctx_t *c, const json_t *args) {
    json_t *decision = gse_make_decision(NULL);
    const char *key = json_get_str(args, "session_key", "");
    bool force_new = gse_truthy(json_obj_get(args, "force_new"));
    bool touch = gse_truthy(json_obj_get(args, "touch_activity"));
    const char *now = json_get_str(args, "now", "");
    json_t *src = gse_make_source(json_obj_get(args, "source"));
    json_t *er = gse_call(c, "_ensure_loaded_locked", json_object());
    if (gse_is_raise(er)) { json_free(decision); json_free(src); return er; }
    json_free(er);
    if (force_new) { json_free(src); return decision; }
    json_t *entries = gse_sub(c, "_entries");
    json_t *entry = entries ? json_obj_get(entries, key) : NULL;
    if (!entry) {
        json_set(decision, "needs_recover", json_bool(true));
        json_free(src);
        return decision;
    }
    json_t *checks = json_obj_get(args, "checks");
    json_t *snapshot_sid = checks ? json_obj_get(checks, "session_id") : NULL;
    json_t *canonical = checks ? json_obj_get(checks, "canonical_id") : NULL;
    /* healed = self._heal_compression_tip_locked(entry, snapshot_sid, canonical) */
    json_t *ha = json_object();
    json_set(ha, "entry", json_copy(entry));
    json_set(ha, "original_session_id", snapshot_sid ? json_copy(snapshot_sid) : json_null());
    json_set(ha, "canonical_session_id", canonical ? json_copy(canonical) : json_null());
    json_t *hr = gse_call(c, "_heal_compression_tip_locked", ha);
    if (gse_is_raise(hr)) { json_free(decision); json_free(src); return hr; }
    bool healed = gse_truthy(hr);
    json_free(hr);
    bool checked = gse_eq_str(json_obj_get(entry, "session_id"), snapshot_sid);
    bool stale_hit = false;
    json_t *reset_reason = NULL;
    if (checked && checks) {
        stale_hit = gse_truthy(json_obj_get(checks, "is_stale"));
        reset_reason = json_obj_get(checks, "reset_reason");
    }
    if (stale_hit) {
        json_t *la = json_object();
        json_set_hdr(la, "msg",
            "gateway.session: routing key %r -> %s is ended in state.db but still live in "
            "sessions.json; dropping stale entry and recovering/recreating the session "
            "(#54878)");
        json_t *arr = json_array();
        json_append(arr, json_string(key));
        json_append(arr, json_copy(json_obj_get(entry, "session_id")));
        json_set(la, "args", arr);
        json_t *lr = gse_call(c, "logger.warning", la);
        if (gse_is_raise(lr)) { json_free(decision); json_free(src); return lr; }
        json_free(lr);
    }
    if (stale_hit || gse_truthy(reset_reason)) {
        if (gse_truthy(reset_reason)) {
            json_t *lpt = json_obj_get(entry, "last_prompt_tokens");
            bool had = lpt && lpt->type == JSON_NUMBER && lpt->num_val > 0;
            gse_schedule_reset_core(decision, gse_str(reset_reason), had, entry);
        }
        if (entries) json_obj_del(entries, key);
        json_set(decision, "needs_recover", json_bool(true));
    } else {
        if (touch) json_set(entry, "updated_at", json_string(now));
        if (touch && gse_is_null(json_obj_get(entry, "origin")))
            json_set(entry, "origin", json_copy(src));
        json_set(decision, "needs_save", json_bool(touch || healed));
        json_set(decision, "metadata_only_save", json_bool(touch && !healed));
        if (touch && gse_is_null(json_obj_get(entry, "transport_profile"))) {
            json_t *ta = json_object();
            json_set(ta, "source", json_copy(src));
            json_t *tr = gse_call(c, "transport_profile_of", ta);
            if (gse_is_raise(tr)) { json_free(decision); json_free(src); return tr; }
            json_set(entry, "transport_profile", tr ? json_copy(tr) : json_null());
            json_free(tr);
        }
        /* decision.entry aliases the live entry: copy AFTER all its mutations so
         * the transport_profile fill is reflected (live holds a reference). */
        json_set(decision, "entry", json_copy(entry));
    }
    json_free(src);
    return decision;
}