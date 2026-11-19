/* conversation_sessions_flow.c — FLOW + leaves port of
 * persistence/conversation/sessions.py (LIVE authority).
 *
 * Ports the module's pure vocabulary leaves, the dataclass validators
 * (__post_init__), the row/listing projection methods (as_session_row /
 * _row_cells / _marker_cells / _head_cells / as_list_row / as_cron_row /
 * as_mapping) and the three error constructors. The SessionRepository
 * protocol verbs are `...` interface stubs in the live module (no body) and
 * are NOT ported here.
 *
 * FLOW cores (decision sequences around an injected dependency) ride the
 * universal seam cs_io_t.call:
 *   - "json.loads"                  (parse_model_config, is_corrupt_model_config)
 *   - "parse_model_config"          (session_yolo_enabled)
 *   - "is_automatic_end_reason"     (is_explicit_close_reason)
 *   - "SessionRecord._row_cells"    (as_session_row, _head_cells)
 *   - "SessionRecord._marker_cells" (as_session_row, _head_cells)
 *   - "SessionListEntry._head_cells"(as_list_row, as_cron_row)
 * Each seam call appends {"call":name,...args} to io->trace (the ordered
 * decision sequence) and returns the scripted result. The seamed siblings are
 * ALSO ported as leaves below (their own direct cases exercise the real
 * transcription); the oracle drives the LIVE defs the same way.
 *
 * Python semantics honored: `or` falsy chains, `is None` identity vs `if x`
 * truthiness, str.strip / re \s Unicode White_Space set (incl \x1c-\x1f),
 * code-point (not byte) length/slicing, dict insertion-order merges, int()
 * truncation toward zero, bool-as-int, f-string !r repr delimiter selection,
 * exact ValueError messages, exception subclass catching (JSONDecodeError ⊂
 * ValueError; RecursionError ⊂ RuntimeError). BaseException kinds (SystemExit /
 * KeyboardInterrupt / GeneratorExit / CancelledError) PASS THROUGH every except
 * site. Missing/exhausted scripts FAIL CLOSED with a RuntimeError
 * "no script for <name>" raise (recorded, then propagated).
 *
 * Representation contract (shared with tests/oracle_conversation_sessions_flow.py):
 *   - record/self doubles are attribute dicts (JSON objects); the oracle drives
 *     SimpleNamespace doubles. Enum attributes (lineage_kind, title_source)
 *     carry their .value string; the oracle materializes real enum members.
 *   - SessionId is NewType(str) -> its str value; str(sid) == sid.
 *   - integral floats collapse to ints on both sides; libjson emits %.0f for
 *     integral doubles, %.17g otherwise (round-trips through json.loads).
 *   - uncaught exception -> {"__raise__":"<ClassName>","message":str(exc)}.
 *   - error constructors return {"class","args","str","attrs"} projections.
 *   - scripts: absent key -> fail-closed RuntimeError; a bare value reused;
 *     a JSON array consumed one result per call (last repeats NOT assumed —
 *     exhaustion fails closed); {"raise":{type,message,...}} raises;
 *     {"ret":v} returns v. Null-valued seam arg keys are KEPT on both sides.
 *
 * Fixture domain (documented): ASCII strings/numbers/bools/null for str()/int()/
 * float()/repr() operands. sanitize_title additionally exercises its BMP
 * invisible/whitespace code-point classes (U+200B..U+FFFB) and the fixed
 * ellipsis U+2026 / replacement U+FFFD outputs. Lone surrogates cannot ride
 * valid UTF-8 JSON: libjson normalizes them to U+FFFD at parse time, which is
 * confluent with the live surrogate->U+FFFD substitution (same cleaned output).
 * str() of containers is outside the domain (Python repr spacing differs from
 * compact JSON); such operands never reach cs_jpystr in the fixtures.
 *
 * libjson-only, self-contained: the harness #includes
 * "persistence/conversation_sessions_flow.c". Ownership: seam results and
 * function returns are caller-owned (json_free). io->trace belongs to the
 * harness. Args passed to cs_seam are CONSUMED (copied into the trace event,
 * then freed). Borrowed inputs are never freed. NULL json_t* models None.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

/* ── live module constants (byte-pinned to sessions.py) ───────────────────── */
#define CS_MAX_TITLE_LENGTH 100
#define CS_ACTIVITY_DESCRIPTION_MAX 120
#define CS_ACTIVITY_PROVENANCE_UNKNOWN "unknown"
#define CS_END_REASON_COMPRESSION "compression"
#define CS_YOLO_KEY "yolo_mode"
#define CS_RECENT_BROWSE_CANDIDATE_CAP 2048
#define CS_RECENT_BROWSE_LINEAGE_CAP 8192
#define CS_ELLIPSIS "\xe2\x80\xa6"   /* U+2026 */
#define CS_REPLACEMENT "\xef\xbf\xbd" /* U+FFFD */

static const char *CS_AUTOMATIC_END_REASONS[] = {
    "agent_close", "idle_timeout", "lru_evict", "startup_orphan_reap",
    "superseded_by_resume", "tui_shutdown", "ws_disconnect", "ws_orphan_reap"
};
static const char *CS_BOUNDARY_END_REASONS[] = {
    "daily", "idle", "new_session", "resume_pending_expired", "session_reset",
    "session_switch", "suspended"
};
static const char *CS_ACTIVITY_PROVENANCES[] = {
    "agent.compression", "agent.compression_cooldown", "agent.compression_timeout",
    "agent.compression_turnhold", "unknown"
};
static const char *CS_SESSION_ROW_COLUMNS[] = {
    "id", "runtime_revision", "runtime_generation", "source", "created_source",
    "user_id", "session_key", "chat_id", "chat_type", "thread_id", "display_name",
    "origin_json", "expiry_finalized", "model", "model_config", "system_prompt",
    "system_prompt_hash", "parent_session_id", "started_at", "ended_at", "end_reason",
    "message_count", "tool_call_count", "input_tokens", "output_tokens",
    "cache_read_tokens", "cache_write_tokens", "reasoning_tokens", "cwd", "git_branch",
    "git_repo_root", "git_metadata_generation", "billing_provider", "billing_base_url",
    "billing_mode", "estimated_cost_usd", "actual_cost_usd", "cost_status", "cost_source",
    "pricing_version", "title", "title_source", "last_activity_at",
    "last_activity_description", "last_activity_provenance", "api_call_count",
    "handoff_state", "handoff_platform", "handoff_error",
    "compression_failure_cooldown_until", "compression_failure_error",
    "compression_fallback_streak", "compression_ineffective_count",
    "compression_recovery_deadline", "compression_overload_streak", "profile_name",
    "transport_profile", "rewind_count", "archived", "auto_archived", "pinned",
    "hidden", "last_read_at", "tool_names", "transcript_revision", "prompt_cache_clock"
};
#define CS_N_COLUMNS (sizeof(CS_SESSION_ROW_COLUMNS)/sizeof(CS_SESSION_ROW_COLUMNS[0]))
static const char *CS_COMPACT_EXCLUDED_COLUMNS[] = {
    "git_metadata_generation", "system_prompt", "system_prompt_hash"
};

#define CS_CANONICAL_TITLE_LOCKED_MSG \
    "This is the bot's canonical Bot Chat \xe2\x80\x94 its name is its identity, " \
    "and renaming it would orphan the conversation. To start fresh, create a new " \
    "bot instead."

/* ── universal seam dispatcher + trace ────────────────────────────────────── */
typedef struct cs_io_t {
    json_t *(*call)(struct cs_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} events (harness-owned) */
    void   *ctx;     /* harness backend (scripted results) */
} cs_io_t;

/* ── Python-faithful micro helpers ────────────────────────────────────────── */
static bool cs_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool cs_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool cs_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *cs_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool cs_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static char *cs_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *cs_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return cs_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return NULL; }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2); va_end(ap2);
    return buf;
}
static bool cs_in_set(const char *s, const char *const *set, size_t n) {
    if (!s) return false;
    for (size_t i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return true;
    return false;
}
#define CS_IN(s, arr) cs_in_set((s), (arr), sizeof(arr)/sizeof((arr)[0]))

/* str() projection of a scalar JSON value (containers out of domain). */
static void cs_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}
static char *cs_jpystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return cs_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return cs_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return cs_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64]; double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else cs_float_repr(d, buf, sizeof(buf));
            return cs_strdup(buf);
        }
        default: return json_serialize(v); /* out of domain */
    }
}
/* Python type name of a JSON value (for AttributeError / TypeError messages). */
static const char *cs_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}
/* repr() of an ASCII string: delimiter minimizes escapes (skill rule). */
static char *cs_py_repr(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char delim = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3;
    char *out = malloc(cap); if (!out) return NULL;
    size_t o = 0; out[o++] = delim;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)delim || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c); }
        else out[o++] = (char)c;
    }
    out[o++] = delim; out[o] = '\0';
    return out;
}

/* ── UTF-8 code-point helpers ─────────────────────────────────────────────── */
/* Decode one code point at *p; advance *p. Returns cp, or 0xFFFD on malformed. */
static unsigned int cs_utf8_next(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned char c = s[0];
    if (c < 0x80) { *p = s + 1; return c; }
    unsigned int cp; int n;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
    else { *p = s + 1; return 0xFFFD; }
    for (int i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *p = s + 1; return 0xFFFD; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *p = s + n; return cp;
}
static size_t cs_utf8_encode(unsigned int cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F)); return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}
static size_t cs_char_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s; size_t n = 0;
    while (*p) { cs_utf8_next(&p); n++; }
    return n;
}
/* Unicode White_Space property (str.strip / re \s set, incl \x1c-\x1f). */
static bool cs_is_ws_cp(unsigned int cp) {
    switch (cp) {
        case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
        case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x20:
        case 0x85: case 0xA0: case 0x1680:
        case 0x2028: case 0x2029: case 0x202F: case 0x205F: case 0x3000:
            return true;
        default: return cp >= 0x2000 && cp <= 0x200A;
    }
}
/* str.strip(): remove leading/trailing White_Space code points. malloc'd. */
static char *cs_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *start = p;
    while (*p) {
        const unsigned char *q = p; unsigned int cp = cs_utf8_next(&q);
        if (!cs_is_ws_cp(cp)) break;
        p = q;
    }
    start = p;
    /* find end: last non-ws code point boundary */
    const unsigned char *end = p; const unsigned char *last_nonws = p;
    while (*p) {
        const unsigned char *q = p; unsigned int cp = cs_utf8_next(&q);
        if (!cs_is_ws_cp(cp)) last_nonws = q;
        p = q;
    }
    end = last_nonws;
    size_t n = (size_t)(end - start);
    char *out = malloc(n + 1); if (!out) return NULL;
    memcpy(out, start, n); out[n] = '\0';
    return out;
}
/* title control class [\x00-\x08\x0b\x0c\x0e-\x1f\x7f] */
static bool cs_title_control_cp(unsigned int cp) {
    return (cp <= 0x08) || cp == 0x0B || cp == 0x0C ||
           (cp >= 0x0E && cp <= 0x1F) || cp == 0x7F;
}
/* title invisible class [\u200b-\u200f\u2028-\u202e\u2060-\u2069\ufeff\ufffc\ufff9-\ufffb] */
static bool cs_title_invisible_cp(unsigned int cp) {
    return (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x2028 && cp <= 0x202E) ||
           (cp >= 0x2060 && cp <= 0x2069) || cp == 0xFEFF || cp == 0xFFFC ||
           (cp >= 0xFFF9 && cp <= 0xFFFB);
}
static bool cs_surrogate_cp(unsigned int cp) { return cp >= 0xD800 && cp <= 0xDFFF; }

/* ── exception helpers ────────────────────────────────────────────────────── */
static json_t *cs_make_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
static bool cs_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *cs_raise_type(const json_t *v) {
    return json_get_str(v, "__raise__", "");
}
/* single-inheritance parent map for the names that reach an except site. */
static const char *cs_exc_parent(const char *name) {
    if (cs_str_eq(name, "JSONDecodeError")) return "ValueError";
    if (cs_str_eq(name, "UnicodeError")) return "ValueError";
    if (cs_str_eq(name, "UnicodeDecodeError")) return "UnicodeError";
    if (cs_str_eq(name, "UnicodeEncodeError")) return "UnicodeError";
    if (cs_str_eq(name, "UnicodeTranslateError")) return "UnicodeError";
    if (cs_str_eq(name, "RecursionError")) return "RuntimeError";
    if (cs_str_eq(name, "TitleConflict")) return "Conflict";
    if (cs_str_eq(name, "CanonicalTitleLocked")) return "Conflict";
    if (cs_str_eq(name, "Conflict")) return "PersistenceError";
    if (cs_str_eq(name, "PersistenceError")) return "Exception";
    if (cs_str_eq(name, "SessionExists")) return "Conflict";
    if (cs_str_eq(name, "Exception")) return "BaseException";
    if (cs_str_eq(name, "RuntimeError")) return "Exception";
    if (cs_str_eq(name, "ValueError")) return "Exception";
    if (cs_str_eq(name, "TypeError")) return "Exception";
    if (cs_str_eq(name, "AttributeError")) return "Exception";
    if (cs_str_eq(name, "TimeoutError")) return "OSError";
    if (cs_str_eq(name, "OSError")) return "Exception";
    return NULL; /* SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError -> BaseException */
}
static bool cs_is_a(const char *name, const char *base) {
    const char *cur = name;
    for (int depth = 0; cur && depth < 16; depth++) {
        if (cs_str_eq(cur, base)) return true;
        cur = cs_exc_parent(cur);
    }
    /* BaseException kinds terminate at BaseException (never an Exception base). */
    return cs_str_eq(base, "BaseException");
}
/* except (json.JSONDecodeError, TypeError) */
static bool cs_caught_decode(const char *t) {
    return cs_is_a(t, "JSONDecodeError") || cs_is_a(t, "TypeError");
}
/* except (ValueError, RecursionError) */
static bool cs_caught_corrupt(const char *t) {
    return cs_is_a(t, "ValueError") || cs_is_a(t, "RecursionError");
}

/* ── seam call: record {"call":name,**args}, dispatch, consume args ───────── */
static json_t *cs_seam(cs_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "call", json_string(name));
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++)
            json_set(ev, args->c.keys[i], json_copy(args->c.items[i]));
    }
    if (io && io->trace && io->trace->type == JSON_ARRAY)
        json_append(io->trace, ev);
    else
        json_free(ev);
    json_t *r = (io && io->call) ? io->call(io, name, args) : json_null();
    if (args) json_free(args);
    return r ? r : json_null();
}

/* ── attribute double reads (fail-closed AttributeError on absent) ────────── */
static json_t *cs_attr(const json_t *self, const char *key, json_t **err) {
    json_t *v = json_obj_get(self, key);
    if (!v && err && !*err) {
        char *msg = cs_asprintf("'types.SimpleNamespace' object has no attribute '%s'", key);
        *err = cs_make_raise("AttributeError", msg ? msg : "");
        free(msg);
    }
    return v;
}
/* int() of a bool/number node (truncation toward zero). */
static long long cs_to_int(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return 0;
}
static double cs_to_float(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    return 0.0;
}
/* {**a, **b}: copy a, then b's keys (replace in place / append new). */
static json_t *cs_merge(const json_t *a, const json_t *b) {
    json_t *out = json_object();
    if (a && a->type == JSON_OBJECT)
        for (size_t i = 0; i < a->c.count; i++)
            json_set(out, a->c.keys[i], json_copy(a->c.items[i]));
    if (b && b->type == JSON_OBJECT)
        for (size_t i = 0; i < b->c.count; i++)
            json_set(out, b->c.keys[i], json_copy(b->c.items[i]));
    return out;
}
/* dict(extra): extra is a JSON array of [key, value] pairs. */
static json_t *cs_dict_from_pairs(const json_t *extra) {
    json_t *out = json_object();
    if (extra && extra->type == JSON_ARRAY) {
        for (size_t i = 0; i < extra->c.count; i++) {
            const json_t *pair = extra->c.items[i];
            if (pair && pair->type == JSON_ARRAY && pair->c.count == 2) {
                const char *k = cs_str(pair->c.items[0]);
                if (k) json_set(out, k, json_copy(pair->c.items[1]));
            }
        }
    }
    return out;
}
/* guard: a seam result used as a mapping must be a JSON object. */
static json_t *cs_require_mapping(const json_t *v) {
    if (v && v->type == JSON_OBJECT) return NULL;
    char *msg = cs_asprintf("'%s' object is not a mapping", cs_type_name(v));
    json_t *e = cs_make_raise("TypeError", msg ? msg : "");
    free(msg);
    return e;
}

/* ══ 1. is_automatic_end_reason ════════════════════════════════════════════ */
/* PoP: cs_is_automatic_end_reason @ persistence/conversation/sessions.py:is_automatic_end_reason */
static json_t *cs_is_automatic_end_reason(const json_t *reason) {
    bool r = cs_is_str(reason) && CS_IN(cs_str(reason), CS_AUTOMATIC_END_REASONS);
    return json_bool(r);
}

/* ══ 2. is_explicit_close_reason (FLOW: seams is_automatic_end_reason) ═════ */
/* PoP: cs_is_explicit_close_reason @ persistence/conversation/sessions.py:is_explicit_close_reason */
static json_t *cs_is_explicit_close_reason(cs_io_t *io, const json_t *reason) {
    json_t *args = json_object();
    json_set(args, "reason", reason ? json_copy(reason) : json_null());
    json_t *r = cs_seam(io, "is_automatic_end_reason", args);
    if (cs_raised(r)) return r;
    bool automatic = cs_truthy(r);
    json_free(r);
    if (automatic) return json_bool(false);   /* `not automatic` short-circuits */
    const char *s = cs_str(reason);
    bool ne_compression = !(s && cs_str_eq(s, CS_END_REASON_COMPRESSION));
    bool not_boundary = !(s && CS_IN(s, CS_BOUNDARY_END_REASONS));
    return json_bool(ne_compression && not_boundary);
}

/* ══ 3. bound_activity_description ═════════════════════════════════════════ */
/* PoP: cs_bound_activity_description @ persistence/conversation/sessions.py:bound_activity_description */
static json_t *cs_bound_activity_description(const json_t *description) {
    char *base = cs_truthy(description) ? cs_jpystr(description) : cs_strdup("");
    char *text = cs_strip(base ? base : "");
    free(base);
    if (!text) return json_string("");
    size_t len = cs_char_len(text);
    json_t *out;
    if (len <= CS_ACTIVITY_DESCRIPTION_MAX) {
        out = json_string(text);
    } else {
        /* first (MAX-1) code points + ellipsis */
        const unsigned char *p = (const unsigned char *)text;
        size_t kept = 0; const unsigned char *start = p;
        while (*p && kept < CS_ACTIVITY_DESCRIPTION_MAX - 1) { cs_utf8_next(&p); kept++; }
        size_t n = (size_t)(p - start);
        char *buf = malloc(n + 4);
        if (buf) { memcpy(buf, start, n); memcpy(buf + n, CS_ELLIPSIS, 3); buf[n + 3] = '\0'; }
        out = json_string(buf ? buf : "");
        free(buf);
    }
    free(text);
    return out;
}

/* ══ 4. normalize_activity_provenance ══════════════════════════════════════ */
/* PoP: cs_normalize_activity_provenance @ persistence/conversation/sessions.py:normalize_activity_provenance */
static json_t *cs_normalize_activity_provenance(const json_t *provenance) {
    char *base = cs_truthy(provenance) ? cs_jpystr(provenance) : cs_strdup("");
    char *value = cs_strip(base ? base : "");
    free(base);
    const char *v = value ? value : "";
    json_t *out = json_string(CS_IN(v, CS_ACTIVITY_PROVENANCES) ? v : CS_ACTIVITY_PROVENANCE_UNKNOWN);
    free(value);
    return out;
}

/* ══ 5. workspace_key ══════════════════════════════════════════════════════ */
/* PoP: cs_workspace_key @ persistence/conversation/sessions.py:workspace_key */
static json_t *cs_workspace_key(const json_t *row) {
    const json_t *g = json_obj_get(row, "git_repo_root");
    char *gbase = cs_truthy(g) ? cs_jpystr(g) : cs_strdup("");
    char *gs = cs_strip(gbase ? gbase : ""); free(gbase);
    if (gs && gs[0]) { json_t *out = json_string(gs); free(gs); return out; }
    free(gs);
    const json_t *c = json_obj_get(row, "cwd");
    char *cbase = cs_truthy(c) ? cs_jpystr(c) : cs_strdup("");
    char *cst = cs_strip(cbase ? cbase : ""); free(cbase);
    if (cst && cst[0]) { json_t *out = json_string(cst); free(cst); return out; }
    free(cst);
    return json_null();
}

/* ══ 6. parse_model_config (FLOW: seams json.loads) ════════════════════════ */
/* PoP: cs_parse_model_config @ persistence/conversation/sessions.py:parse_model_config */
static json_t *cs_parse_model_config(cs_io_t *io, const json_t *raw) {
    const json_t *cur = raw;
    json_t *decoded = NULL;
    if (cs_is_str(raw)) {
        char *st = cs_strip(cs_str(raw));
        bool nonblank = st && st[0];
        free(st);
        if (nonblank) {
            json_t *args = json_object();
            json_set(args, "s", json_copy(raw));
            json_t *r = cs_seam(io, "json.loads", args);
            if (cs_raised(r)) {
                if (cs_caught_decode(cs_raise_type(r))) { json_free(r); return json_object(); }
                return r;   /* propagate (incl BaseException) */
            }
            decoded = r; cur = r;
        }
    }
    json_t *out;
    if (cur && cur->type == JSON_OBJECT) out = json_copy(cur);  /* dict(raw) */
    else out = json_object();
    json_free(decoded);
    return out;
}

/* ══ 7. is_corrupt_model_config (FLOW: seams json.loads) ═══════════════════ */
/* PoP: cs_is_corrupt_model_config @ persistence/conversation/sessions.py:is_corrupt_model_config */
static json_t *cs_is_corrupt_model_config(cs_io_t *io, const json_t *raw) {
    if (!cs_is_str(raw)) return json_bool(false);
    char *st = cs_strip(cs_str(raw));
    bool nonblank = st && st[0];
    free(st);
    if (!nonblank) return json_bool(false);
    json_t *args = json_object();
    json_set(args, "s", json_copy(raw));
    json_t *r = cs_seam(io, "json.loads", args);
    if (cs_raised(r)) {
        if (cs_caught_corrupt(cs_raise_type(r))) { json_free(r); return json_bool(true); }
        return r;   /* propagate (TypeError / BaseException) */
    }
    json_free(r);
    return json_bool(false);
}

/* ══ 8. session_yolo_enabled (FLOW: seams parse_model_config) ══════════════ */
/* PoP: cs_session_yolo_enabled @ persistence/conversation/sessions.py:session_yolo_enabled */
static json_t *cs_session_yolo_enabled(cs_io_t *io, const json_t *model_config) {
    json_t *args = json_object();
    json_set(args, "raw", model_config ? json_copy(model_config) : json_null());
    json_t *r = cs_seam(io, "parse_model_config", args);
    if (cs_raised(r)) return r;
    if (!r || r->type != JSON_OBJECT) {
        /* live .get on a non-mapping raises AttributeError */
        char *msg = cs_asprintf("'%s' object has no attribute 'get'", cs_type_name(r));
        json_t *e = cs_make_raise("AttributeError", msg ? msg : "");
        free(msg); json_free(r); return e;
    }
    const json_t *v = json_obj_get(r, CS_YOLO_KEY);
    json_t *out = json_bool(cs_truthy(v));
    json_free(r);
    return out;
}

/* ══ 9. sanitize_title ═════════════════════════════════════════════════════ */
/* PoP: cs_sanitize_title @ persistence/conversation/sessions.py:sanitize_title */
static json_t *cs_sanitize_title(const json_t *title) {
    if (!cs_truthy(title)) return json_null();
    const char *s = cs_str(title);
    if (!s) s = "";
    /* pass 1: surrogate->FFFD, remove control + invisible (disjoint classes). */
    size_t cap = strlen(s) * 4 + 4;
    char *cleaned = malloc(cap); if (!cleaned) return json_null();
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp = cs_utf8_next(&p);
        if (cs_surrogate_cp(cp)) { memcpy(cleaned + o, CS_REPLACEMENT, 3); o += 3; continue; }
        if (cs_title_control_cp(cp) || cs_title_invisible_cp(cp)) continue;
        o += cs_utf8_encode(cp, cleaned + o);
    }
    cleaned[o] = '\0';
    /* pass 2: collapse \s+ runs to a single space. */
    char *collapsed = malloc(o + 2); if (!collapsed) { free(cleaned); return json_null(); }
    size_t k = 0; bool in_ws = false;
    p = (const unsigned char *)cleaned;
    while (*p) {
        const unsigned char *q = p; unsigned int cp = cs_utf8_next(&q);
        if (cs_is_ws_cp(cp)) {
            if (!in_ws) { collapsed[k++] = ' '; in_ws = true; }
        } else {
            in_ws = false;
            size_t n = (size_t)(q - p);
            memcpy(collapsed + k, p, n); k += n;
        }
        p = q;
    }
    collapsed[k] = '\0';
    free(cleaned);
    /* pass 3: strip ends. */
    char *final = cs_strip(collapsed);
    free(collapsed);
    if (!final || !final[0]) { free(final); return json_null(); }
    size_t len = cs_char_len(final);
    if (len > CS_MAX_TITLE_LENGTH) {
        char *msg = cs_asprintf("Title too long (%zu chars, max %d)", len, CS_MAX_TITLE_LENGTH);
        json_t *e = cs_make_raise("ValueError", msg ? msg : "");
        free(msg); free(final); return e;
    }
    json_t *out = json_string(final);
    free(final);
    return out;
}

/* ══ 10. TitleConflict.__init__ ════════════════════════════════════════════ */
/* PoP: cs_title_conflict_init @ persistence/conversation/sessions.py:TitleConflict.__init__ */
static json_t *cs_title_conflict_init(const json_t *title, const json_t *holder) {
    char *t = cs_jpystr(title);
    char *h = cs_jpystr(holder);
    char *msg = cs_asprintf("Title '%s' is already in use by session %s", t, h);
    json_t *out = json_object();
    json_set(out, "class", json_string("TitleConflict"));
    json_t *args = json_array(); json_append(args, json_string(msg ? msg : ""));
    json_set(out, "args", args);
    json_set(out, "str", json_string(msg ? msg : ""));
    json_t *attrs = json_object();
    json_set(attrs, "title", title ? json_copy(title) : json_null());
    json_set(attrs, "holder", holder ? json_copy(holder) : json_null());
    json_set(attrs, "entity", json_string("session_title"));
    json_set(attrs, "entity_id", title ? json_copy(title) : json_null());
    json_set(out, "attrs", attrs);
    free(t); free(h); free(msg);
    return out;
}

/* ══ 11. CanonicalTitleLocked.__init__ ═════════════════════════════════════ */
/* PoP: cs_canonical_title_locked_init @ persistence/conversation/sessions.py:CanonicalTitleLocked.__init__ */
static json_t *cs_canonical_title_locked_init(const json_t *session_id) {
    char *sid = cs_jpystr(session_id);
    json_t *out = json_object();
    json_set(out, "class", json_string("CanonicalTitleLocked"));
    json_t *args = json_array(); json_append(args, json_string(CS_CANONICAL_TITLE_LOCKED_MSG));
    json_set(out, "args", args);
    json_set(out, "str", json_string(CS_CANONICAL_TITLE_LOCKED_MSG));
    json_t *attrs = json_object();
    json_set(attrs, "session_id", session_id ? json_copy(session_id) : json_null());
    json_set(attrs, "entity", json_string("session"));
    json_set(attrs, "entity_id", json_string(sid));
    json_set(out, "attrs", attrs);
    free(sid);
    return out;
}

/* ══ 12. SessionExists.__init__ ════════════════════════════════════════════ */
/* PoP: cs_session_exists_init @ persistence/conversation/sessions.py:SessionExists.__init__ */
static json_t *cs_session_exists_init(const json_t *existing) {
    json_t *err = NULL;
    json_t *sid = cs_attr(existing, "session_id", &err);
    char *sidstr = cs_jpystr(sid);
    json_t *out = json_object();
    json_set(out, "class", json_string("SessionExists"));
    json_t *args = json_array(); json_append(args, json_string("Session already exists"));
    json_set(out, "args", args);
    json_set(out, "str", json_string("Session already exists"));
    json_t *attrs = json_object();
    json_set(attrs, "existing", existing ? json_copy(existing) : json_null());
    json_set(attrs, "entity", json_string("session"));
    json_set(attrs, "entity_id", json_string(sidstr));
    json_set(out, "attrs", attrs);
    free(sidstr);
    if (err) { json_free(out); return err; }
    return out;
}

/* ══ 13. NewSession.__post_init__ ══════════════════════════════════════════ */
/* PoP: cs_new_session_post_init @ persistence/conversation/sessions.py:NewSession.__post_init__ */
static json_t *cs_new_session_post_init(const json_t *self) {
    json_t *err = NULL;
    json_t *sid = cs_attr(self, "session_id", &err); if (err) return err;
    if (!cs_truthy(sid)) return cs_make_raise("ValueError", "NewSession.session_id must be non-empty");
    json_t *source = cs_attr(self, "source", &err); if (err) return err;
    if (!cs_truthy(source)) return cs_make_raise("ValueError", "NewSession.source must be non-empty");
    json_t *lk = cs_attr(self, "lineage_kind", &err); if (err) return err;
    const char *lkv = cs_str(lk);
    if (!(lkv && cs_str_eq(lkv, "none"))) {   /* lineage_kind is not NONE */
        json_t *parent = cs_attr(self, "parent_session_id", &err); if (err) return err;
        if (!cs_truthy(parent)) {
            char *rep = cs_py_repr(lkv ? lkv : "");
            char *msg = cs_asprintf("lineage_kind %s requires parent_session_id", rep ? rep : "''");
            json_t *e = cs_make_raise("ValueError", msg ? msg : "");
            free(rep); free(msg); return e;
        }
    }
    return json_null();
}

/* ══ 14. SessionRecord._row_cells ══════════════════════════════════════════ */
/* PoP: cs_session_record_row_cells @ persistence/conversation/sessions.py:SessionRecord._row_cells */
static json_t *cs_session_record_row_cells(const json_t *self, bool compact) {
    json_t *err = NULL;
    /* cells override literal, in live evaluation order. */
    json_t *cells = json_object();
    json_t *sid = cs_attr(self, "session_id", &err); if (err) { json_free(cells); return err; }
    char *sidstr = cs_jpystr(sid);
    json_set(cells, "id", json_string(sidstr)); free(sidstr);
    json_t *oj = cs_attr(self, "origin_json", &err); if (err) { json_free(cells); return err; }
    json_set(cells, "origin_json", oj ? json_copy(oj) : json_null());
    json_t *ef = cs_attr(self, "expiry_finalized", &err); if (err) { json_free(cells); return err; }
    json_set(cells, "expiry_finalized", json_number((double)cs_to_int(ef)));
    json_t *mcj = cs_attr(self, "model_config_json", &err); if (err) { json_free(cells); return err; }
    json_set(cells, "model_config", mcj ? json_copy(mcj) : json_null());
    json_t *ts = cs_attr(self, "title_source", &err); if (err) { json_free(cells); return err; }
    json_set(cells, "title_source", cs_is_none(ts) ? json_null() : json_string(cs_str(ts) ? cs_str(ts) : ""));
    const char *boolcols[] = {"archived", "auto_archived", "pinned", "hidden"};
    for (size_t i = 0; i < 4; i++) {
        json_t *b = cs_attr(self, boolcols[i], &err); if (err) { json_free(cells); return err; }
        json_set(cells, boolcols[i], json_number((double)cs_to_int(b)));
    }
    json_t *tn = cs_attr(self, "tool_names", &err); if (err) { json_free(cells); return err; }
    json_set(cells, "tool_names", tn ? json_copy(tn) : json_null());
    /* comprehension over SESSION_ROW_COLUMNS in table order. */
    json_t *out = json_object();
    for (size_t i = 0; i < CS_N_COLUMNS; i++) {
        const char *col = CS_SESSION_ROW_COLUMNS[i];
        if (compact && CS_IN(col, CS_COMPACT_EXCLUDED_COLUMNS)) continue;
        json_t *cv = json_obj_get(cells, col);
        if (cv) { json_set(out, col, json_copy(cv)); continue; }
        json_t *av = cs_attr(self, col, &err);
        if (err) { json_free(cells); json_free(out); return err; }
        json_set(out, col, av ? json_copy(av) : json_null());
    }
    json_free(cells);
    return out;
}

/* ══ 15. SessionRecord._marker_cells ═══════════════════════════════════════ */
/* PoP: cs_session_record_marker_cells @ persistence/conversation/sessions.py:SessionRecord._marker_cells */
static json_t *cs_session_record_marker_cells(const json_t *self) {
    json_t *err = NULL;
    json_t *out = json_object();
    json_t *rf = cs_attr(self, "reset_from", &err); if (err) { json_free(out); return err; }
    if (cs_truthy(rf)) json_set(out, "_reset_from", json_copy(rf));
    json_t *bf = cs_attr(self, "branched_from", &err); if (err) { json_free(out); return err; }
    if (cs_truthy(bf)) json_set(out, "_branched_from", json_copy(bf));
    return out;
}

/* ══ 16. SessionRecord.as_session_row (FLOW) ═══════════════════════════════ */
/* PoP: cs_session_record_as_session_row @ persistence/conversation/sessions.py:SessionRecord.as_session_row */
static json_t *cs_session_record_as_session_row(cs_io_t *io, const json_t *self) {
    (void)self;
    json_t *a1 = json_object(); json_set(a1, "compact", json_bool(false));
    json_t *r1 = cs_seam(io, "SessionRecord._row_cells", a1);
    if (cs_raised(r1)) return r1;
    json_t *m1 = cs_require_mapping(r1); if (m1) { json_free(r1); return m1; }
    json_t *r2 = cs_seam(io, "SessionRecord._marker_cells", json_object());
    if (cs_raised(r2)) { json_free(r1); return r2; }
    json_t *m2 = cs_require_mapping(r2); if (m2) { json_free(r1); json_free(r2); return m2; }
    json_t *out = cs_merge(r1, r2);
    json_free(r1); json_free(r2);
    return out;
}

/* ══ 17. SessionWindow.__post_init__ ═══════════════════════════════════════ */
/* PoP: cs_session_window_post_init @ persistence/conversation/sessions.py:SessionWindow.__post_init__ */
static json_t *cs_session_window_post_init(const json_t *self) {
    json_t *err = NULL;
    json_t *limit = cs_attr(self, "limit", &err); if (err) return err;
    bool a = (!cs_is_none(limit) && cs_to_float(limit) < 0.0);
    json_t *offset = NULL;
    bool b = false;
    if (!a) {
        offset = cs_attr(self, "offset", &err); if (err) return err;
        b = cs_to_float(offset) < 0.0;
    }
    if (a || b) {
        if (!offset) { offset = cs_attr(self, "offset", &err); if (err) return err; }
        char *ls = cs_jpystr(limit);
        char *os = cs_jpystr(offset);
        char *msg = cs_asprintf("SessionWindow needs limit >= 0 and offset >= 0, got %s, %s", ls, os);
        json_t *e = cs_make_raise("ValueError", msg ? msg : "");
        free(ls); free(os); free(msg); return e;
    }
    return json_null();
}

/* ══ 18. SessionListEntry._head_cells (FLOW) ═══════════════════════════════ */
/* PoP: cs_session_list_entry_head_cells @ persistence/conversation/sessions.py:SessionListEntry._head_cells */
static json_t *cs_session_list_entry_head_cells(cs_io_t *io, const json_t *self, const json_t *extra) {
    json_t *err = NULL;
    json_t *session = cs_attr(self, "session", &err); if (err) return err;
    (void)session;
    json_t *compact = cs_attr(self, "compact", &err); if (err) return err;
    json_t *a1 = json_object();
    json_set(a1, "compact", compact ? json_copy(compact) : json_null());
    json_t *r1 = cs_seam(io, "SessionRecord._row_cells", a1);
    if (cs_raised(r1)) return r1;
    json_t *m1 = cs_require_mapping(r1); if (m1) { json_free(r1); return m1; }
    json_t *extra_d = cs_dict_from_pairs(extra);
    json_t *la = cs_attr(self, "last_active", &err);
    if (err) { json_free(r1); json_free(extra_d); return err; }
    json_t *r2 = cs_seam(io, "SessionRecord._marker_cells", json_object());
    if (cs_raised(r2)) { json_free(r1); json_free(extra_d); return r2; }
    json_t *m2 = cs_require_mapping(r2);
    if (m2) { json_free(r1); json_free(extra_d); json_free(r2); return m2; }
    json_t *preview = cs_attr(self, "preview", &err);
    if (err) { json_free(r1); json_free(extra_d); json_free(r2); return err; }
    /* {**row_cells, **dict(extra), "last_active":..., **markers, "preview":...} */
    json_t *out = cs_merge(r1, extra_d);
    json_set(out, "last_active", la ? json_copy(la) : json_null());
    json_t *merged = cs_merge(out, r2);
    json_set(merged, "preview", preview ? json_copy(preview) : json_null());
    json_free(r1); json_free(extra_d); json_free(r2); json_free(out);
    return merged;
}

/* ══ 19. SessionListEntry.as_list_row (FLOW) ═══════════════════════════════ */
/* PoP: cs_session_list_entry_as_list_row @ persistence/conversation/sessions.py:SessionListEntry.as_list_row */
static json_t *cs_session_list_entry_as_list_row(cs_io_t *io, const json_t *self) {
    json_t *err = NULL;
    json_t *a = json_object(); json_set(a, "extra", json_array());
    json_t *head = cs_seam(io, "SessionListEntry._head_cells", a);
    if (cs_raised(head)) return head;
    json_t *mh = cs_require_mapping(head); if (mh) { json_free(head); return mh; }
    json_t *out = json_copy(head); json_free(head);
    json_t *lrid = cs_attr(self, "lineage_root_id", &err);
    if (err) { json_free(out); return err; }
    if (!cs_is_none(lrid)) {
        char *s = cs_jpystr(lrid);
        json_set(out, "_lineage_root_id", json_string(s)); free(s);
        json_t *lids = cs_attr(self, "lineage_ids", &err);
        if (err) { json_free(out); return err; }
        json_t *arr = json_array();
        if (lids && lids->type == JSON_ARRAY)
            for (size_t i = 0; i < lids->c.count; i++) {
                char *ss = cs_jpystr(lids->c.items[i]);
                json_append(arr, json_string(ss)); free(ss);
            }
        json_set(out, "_lineage_ids", arr);
        json_t *ck = cs_attr(self, "continuation_kind", &err);
        if (err) { json_free(out); return err; }
        json_set(out, "continuation_kind", ck ? json_copy(ck) : json_null());
    } else {
        json_t *lids = cs_attr(self, "lineage_ids", &err);
        if (err) { json_free(out); return err; }
        if (cs_truthy(lids)) {
            json_t *arr = json_array();
            if (lids->type == JSON_ARRAY)
                for (size_t i = 0; i < lids->c.count; i++) {
                    char *ss = cs_jpystr(lids->c.items[i]);
                    json_append(arr, json_string(ss)); free(ss);
                }
            json_set(out, "_lineage_ids", arr);
        }
    }
    json_t *unread = cs_attr(self, "unread", &err);
    if (err) { json_free(out); return err; }
    json_set(out, "unread", unread ? json_copy(unread) : json_null());
    return out;
}

/* ══ 20. CronRunEntry.as_cron_row (FLOW) ═══════════════════════════════════ */
/* PoP: cs_cron_run_entry_as_cron_row @ persistence/conversation/sessions.py:CronRunEntry.as_cron_row */
static json_t *cs_cron_run_entry_as_cron_row(cs_io_t *io, const json_t *self) {
    json_t *err = NULL;
    json_t *entry = cs_attr(self, "entry", &err); if (err) return err;
    (void)entry;
    json_t *ceid = cs_attr(self, "cron_execution_id", &err); if (err) return err;
    json_t *pair = json_array();
    json_append(pair, json_string("cron_execution_id"));
    json_append(pair, ceid ? json_copy(ceid) : json_null());
    json_t *extra = json_array(); json_append(extra, pair);
    json_t *a = json_object(); json_set(a, "extra", extra);
    json_t *head = cs_seam(io, "SessionListEntry._head_cells", a);
    if (cs_raised(head)) return head;
    json_t *mh = cs_require_mapping(head); if (mh) { json_free(head); return mh; }
    json_t *cjid = cs_attr(self, "cron_job_id", &err);
    if (err) { json_free(head); return err; }
    json_t *out = json_copy(head); json_free(head);
    json_set(out, "cron_job_id", cjid ? json_copy(cjid) : json_null());
    return out;
}

/* ══ 21. RecentBrowseQuery.__post_init__ (mutates self) ════════════════════ */
/* PoP: cs_recent_browse_query_post_init @ persistence/conversation/sessions.py:RecentBrowseQuery.__post_init__ */
static json_t *cs_recent_browse_query_post_init(json_t *self) {
    json_t *err = NULL;
    json_t *lv = cs_attr(self, "limit", &err); if (err) return err;
    long long limit = cs_to_int(lv); if (limit < 1) limit = 1;
    json_t *cv = cs_attr(self, "candidate_limit", &err); if (err) return err;
    /* int(candidate): cs_to_int truncates a float toward zero; default is int. */
    long long candidate = !cs_is_none(cv) ? cs_to_int(cv) : (limit * 8 > 128 ? limit * 8 : 128);
    if (candidate > CS_RECENT_BROWSE_CANDIDATE_CAP) candidate = CS_RECENT_BROWSE_CANDIDATE_CAP;
    if (candidate < limit) candidate = limit;
    json_t *lnv = cs_attr(self, "lineage_limit", &err); if (err) return err;
    long long lineage = !cs_is_none(lnv) ? cs_to_int(lnv)
                        : (candidate * 8 < CS_RECENT_BROWSE_LINEAGE_CAP ? candidate * 8 : CS_RECENT_BROWSE_LINEAGE_CAP);
    if (lineage > CS_RECENT_BROWSE_LINEAGE_CAP) lineage = CS_RECENT_BROWSE_LINEAGE_CAP;
    if (lineage < candidate) lineage = candidate;
    json_set(self, "limit", json_number((double)limit));
    json_t *tv = cs_attr(self, "timeout_seconds", &err); if (err) return err;
    double timeout = cs_to_float(tv); if (timeout < 0.0) timeout = 0.0;
    json_set(self, "timeout_seconds", json_number(timeout));
    json_set(self, "candidate_limit", json_number((double)candidate));
    json_set(self, "lineage_limit", json_number((double)lineage));
    return json_null();
}

/* ══ 22. RuntimeLock.as_mapping ════════════════════════════════════════════ */
/* PoP: cs_runtime_lock_as_mapping @ persistence/conversation/sessions.py:RuntimeLock.as_mapping */
static json_t *cs_runtime_lock_as_mapping(const json_t *self) {
    json_t *err = NULL;
    json_t *out = json_object();
    json_t *provider = cs_attr(self, "provider", &err); if (err) { json_free(out); return err; }
    json_set(out, "provider", cs_truthy(provider) ? json_copy(provider) : json_string(""));
    json_t *model = cs_attr(self, "model", &err); if (err) { json_free(out); return err; }
    json_set(out, "model", cs_truthy(model) ? json_copy(model) : json_string(""));
    json_t *mo = cs_attr(self, "model_options", &err); if (err) { json_free(out); return err; }
    json_set(out, "model_options", cs_truthy(mo) ? json_copy(mo) : json_object());
    json_t *rs = cs_attr(self, "route_source", &err); if (err) { json_free(out); return err; }
    json_set(out, "route_source", cs_truthy(rs) ? json_copy(rs) : json_string(""));
    json_t *cf = cs_attr(self, "confirmed", &err); if (err) { json_free(out); return err; }
    json_set(out, "confirmed", json_bool(cs_truthy(cf)));
    return out;
}

/* ══ 23. ReplaceSessionConfig.__post_init__ ════════════════════════════════ */
/* PoP: cs_replace_session_config_post_init @ persistence/conversation/sessions.py:ReplaceSessionConfig.__post_init__ */
static json_t *cs_replace_session_config_post_init(const json_t *self) {
    json_t *err = NULL;
    json_t *model = cs_attr(self, "model", &err); if (err) return err;
    if (!cs_is_none(model) && !cs_truthy(model))
        return cs_make_raise("ValueError", "ReplaceSessionConfig.model must be None or non-empty");
    return json_null();
}