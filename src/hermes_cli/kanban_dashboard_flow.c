/* kanban_dashboard_flow.c — FLOW-core port of plugins/kanban/dashboard/plugin_api.py
 * (the Kanban dashboard backend API helper layer).
 *
 * Ports the DECISION SEQUENCES of the dashboard's connection/board helpers,
 * serialization helpers, status-verb dispatch and override knobs:
 *   _ws_upgrade_authorized, _normalize_slug_or_400, _resolve_board,
 *   _existing_board_slug, _conn, _require, _require_task, _require_run,
 *   _require_ok, _conflict, _task_dict, _attachment_dict, _placeholders,
 *   _warnings_summary_from_diagnostics, _attach_diagnostics, _links_for,
 *   _link_tasks, _drag_to, _apply_status, _set_priority, _apply_model_override,
 *   _apply_reasoning_effort, _open_parent_refusal, _cap, _load_config_or_empty,
 *   _active_profile_name, _since_param, _ws_board, _ws_board_live,
 *   _board_display_kwargs.
 *
 * All external I/O (kanban_db / kbc / log / conn.execute / in-function imports)
 * is INJECTED through the kd_io_t universal seam dispatcher. Each seam call
 * appends a structured event {"call": name, ...args} to io->trace (the ordered
 * decision sequence) and returns the scripted result. A raised exception rides
 * as {"__raise__": "<Type>", "message": "...", ...}; BaseException kinds
 * (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError) PASS THROUGH
 * every `except Exception` site. An "import:" seam models an in-function import:
 * the dispatcher returns {"__untrace__": true, ...} so the seam retracts the
 * trace token (a live import records nothing), with {"__raise__": spec} on a
 * scripted import failure. Missing/exhausted scripts FAIL CLOSED with a
 * RuntimeError "no script for <name>" raise (recorded, then propagated).
 *
 * Pure internal helpers (_map_errors / _value_error_400 semantics, the
 * _STATUS_HANDLERS dispatch, _warnings_summary aggregation) are transcribed LIVE
 * here and AST-extracted REAL in the oracle, so nested logic is exercised.
 * _set_status_direct is a SEAMED sibling (not ported here).
 *
 * Deterministic subset (documented): ASCII fixture scope — str.strip / len /
 * slicing / repr cover printable ASCII plus the standard whitespace controls;
 * the only non-ASCII literal is the fixed ellipsis "…" in _cap's output. JSON
 * numbers lose int/float identity after parsing: integral doubles serialize as
 * ints on both sides. kanban_db.Task is modeled as its asdict() field object.
 * get_task / get_run returns are modeled in their live domain: null (missing)
 * or a NON-EMPTY record object (a live Task/Run is always truthy); the `if task`
 * truthiness tests therefore read null-vs-record, never an empty record.
 *
 * Ownership: seam results and function returns are caller-owned (json_free).
 * io->trace belongs to the harness. Args passed to kd_seam are consumed (copied
 * into the trace event, then freed). Borrowed inputs are never freed. A NULL
 * json_t* input models Python None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to plugin_api.py) ─────────────────── */
#define KD_RUNNING_DIRECT_MSG \
    "Cannot set status to 'running' directly; use the dispatcher/claim path"
#define KD_DEFAULT_BOARD "default"
/* kd.SEVERITY_ORDER (kanban_diagnostics.py) */
static const char *KD_SEVERITY_ORDER[] = {"warning", "error", "critical"};
#define KD_SEVERITY_ORDER_N 3
/* _cap ellipsis (U+2026) */
#define KD_ELLIPSIS "\xe2\x80\xa6"

/* ── seam dispatcher + trace ──────────────────────────────────────────────── */
typedef struct kd_io_t {
    json_t *(*call)(struct kd_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of {"call":name,...args} events (harness-owned) */
    void   *ctx;     /* harness backend (scripted results) */
} kd_io_t;

static bool kd_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *kd_raise_type(const json_t *v) {
    return json_get_str(v, "__raise__", "");
}
static const char *kd_raise_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}
static json_t *kd_make_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}
/* Build a sentinel from a scripted {"type","message",...} spec, preserving the
 * extra fields HTTPException / SystemExit / ModuleNotFoundError carry. */
static json_t *kd_raise_from_spec(const json_t *spec) {
    const char *t = json_get_str(spec, "type", "Exception");
    const char *m = json_get_str(spec, "message", "");
    json_t *e = kd_make_raise(t, m);
    const char *extra[] = {"status_code", "detail", "code", "name"};
    for (size_t i = 0; i < sizeof(extra)/sizeof(extra[0]); i++) {
        json_t *v = json_obj_get(spec, extra[i]);
        if (v) json_set(e, extra[i], json_copy(v));
    }
    return e;
}
/* HTTPException(status_code, detail): str(e) == "<status>: <detail>". */
static json_t *kd_http_exc(int status, const char *detail) {
    char *msg = NULL;
    const char *d = detail ? detail : "";
    int n = snprintf(NULL, 0, "%d: %s", status, d);
    msg = malloc((size_t)n + 1);
    if (msg) snprintf(msg, (size_t)n + 1, "%d: %s", status, d);
    json_t *e = kd_make_raise("HTTPException", msg ? msg : "");
    free(msg);
    json_set(e, "status_code", json_number(status));
    json_set(e, "detail", json_string(d));
    return e;
}
static bool kd_is_base_exc(const char *type) {
    return strcmp(type, "SystemExit") == 0 || strcmp(type, "KeyboardInterrupt") == 0 ||
           strcmp(type, "GeneratorExit") == 0 || strcmp(type, "CancelledError") == 0;
}
static bool kd_is_value_error(const char *type) {
    return strcmp(type, "ValueError") == 0 || strcmp(type, "UnicodeError") == 0 ||
           strcmp(type, "UnicodeDecodeError") == 0 || strcmp(type, "UnicodeEncodeError") == 0;
}

/* Record {"call":name,**args}, dispatch, consume args. An "__untrace__" result
 * (in-function import) retracts the trace token so the live silent import
 * matches; a scripted import failure also carries "__raise__". */
static json_t *kd_seam(kd_io_t *io, const char *name, json_t *args) {
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
    if (!r) return json_null();
    if (r->type == JSON_OBJECT && json_obj_get(r, "__untrace__")) {
        if (io && io->trace && io->trace->type == JSON_ARRAY && io->trace->c.count > 0) {
            size_t last = io->trace->c.count - 1;
            json_free(io->trace->c.items[last]);
            io->trace->c.count = last;
        }
        json_t *spec = json_obj_get(r, "__raise__");
        json_t *out = spec ? kd_raise_from_spec(spec) : json_null();
        json_free(r);
        return out;
    }
    return r;
}

/* ── Python-faithful micro helpers ────────────────────────────────────────── */
static bool kd_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool kd_truthy(const json_t *v) {
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
static const char *kd_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static char *kd_strdup(const char *s) {
    size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p;
}
static char *kd_asprintf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return kd_strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (!buf) { va_end(ap2); return NULL; }
    vsnprintf(buf, (size_t)n + 1, fmt, ap2); va_end(ap2);
    return buf;
}
typedef struct { char *s; size_t len, cap; } kd_buf;
static void kd_buf_init(kd_buf *b) { b->cap = 64; b->s = malloc(b->cap); b->s[0] = '\0'; b->len = 0; }
static void kd_buf_push(kd_buf *b, const char *t) {
    size_t n = strlen(t);
    if (b->len + n + 1 > b->cap) { while (b->len + n + 1 > b->cap) b->cap *= 2; b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->len, t, n + 1); b->len += n;
}
static void kd_buf_char(kd_buf *b, char c) { char t[2] = {c, 0}; kd_buf_push(b, t); }

/* ASCII str.strip() (both ends). Caller frees. */
static bool kd_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *kd_strip(const char *s) {
    if (!s) return kd_strdup("");
    size_t n = strlen(s); size_t a = 0, b = n;
    while (a < b && kd_is_space(s[a])) a++;
    while (b > a && kd_is_space(s[b-1])) b--;
    char *out = malloc(b - a + 1); memcpy(out, s + a, b - a); out[b-a] = '\0';
    return out;
}
/* Python len(str) — codepoint count (ASCII scope: == byte count). */
static size_t kd_py_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
/* First n codepoints of s (Python s[:n]). Caller frees. */
static char *kd_slice(const char *s, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t count = 0;
    while (*p && count < n) { if ((*p & 0xC0) != 0x80) count++; p++; }
    size_t bytes = (size_t)(p - (const unsigned char *)s);
    char *out = malloc(bytes + 1); memcpy(out, s, bytes); out[bytes] = '\0';
    return out;
}
/* Python repr(str) for ASCII strings. Caller frees. */
static char *kd_py_repr_str(const char *s) {
    bool has_sq = strchr(s, '\'') != NULL, has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    kd_buf b; kd_buf_init(&b); kd_buf_char(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { kd_buf_char(&b, '\\'); kd_buf_char(&b, (char)c); }
        else if (c == '\n') kd_buf_push(&b, "\\n");
        else if (c == '\r') kd_buf_push(&b, "\\r");
        else if (c == '\t') kd_buf_push(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) { char *h = kd_asprintf("\\x%02x", c); kd_buf_push(&b, h); free(h); }
        else kd_buf_char(&b, (char)c);
    }
    kd_buf_char(&b, q);
    return b.s;
}
/* Stringify a JSON scalar the way Python str() does for str/int (ident slot). */
static char *kd_ident_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return kd_strdup("None");
    if (v->type == JSON_STRING) return kd_strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) return kd_asprintf("%.0f", v->num_val);
    if (v->type == JSON_BOOL) return kd_strdup(v->bool_val ? "True" : "False");
    return kd_strdup("");
}
/* Python int(str) base-10 parse (sign + digits, underscores between digits).
 * Returns true on success and stores the value; false == ValueError. */
static bool kd_parse_int(const char *s, long long *out) {
    const char *p = s;
    while (*p && kd_is_space(*p)) p++;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    const char *start = p;
    long long val = 0; bool any = false, prev_digit = false;
    for (; *p; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') { val = val * 10 + (c - '0'); any = true; prev_digit = true; }
        else if (c == '_') { if (!prev_digit) return false; prev_digit = false; }
        else break;
    }
    if (!any || !prev_digit) return false;
    while (*p && kd_is_space(*p)) p++;
    if (*p != '\0') return false;
    (void)start;
    *out = neg ? -val : val;
    return true;
}
/* Borrowed object field as a NEW node (null when absent). */
static json_t *kd_field(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return v ? json_copy(v) : json_null();
}

/* ── PoP-ported functions ─────────────────────────────────────────────────── */

/* PoP: kd_placeholders @ plugins/kanban/dashboard/plugin_api.py:_placeholders */
json_t *kd_placeholders(kd_io_t *io, const json_t *ids) {
    (void)io;
    size_t n = (ids && ids->type == JSON_ARRAY) ? ids->c.count : 0;
    kd_buf b; kd_buf_init(&b);
    for (size_t i = 0; i < n; i++) { if (i) kd_buf_char(&b, ','); kd_buf_char(&b, '?'); }
    json_t *r = json_string(b.s); free(b.s); return r;
}

/* PoP: kd_cap @ plugins/kanban/dashboard/plugin_api.py:_cap */
json_t *kd_cap(kd_io_t *io, const json_t *s, double n) {
    (void)io;
    const char *raw = kd_str(s);
    char *stripped = kd_strip(raw ? raw : "");
    size_t len = kd_py_len(stripped);
    json_t *r;
    if (len <= (size_t)n) { r = json_string(stripped); }
    else {
        char *head = kd_slice(stripped, (size_t)n);
        char *joined = kd_asprintf("%s%s", head, KD_ELLIPSIS);
        r = json_string(joined); free(head); free(joined);
    }
    free(stripped);
    return r;
}

/* PoP: kd_attachment_dict @ plugins/kanban/dashboard/plugin_api.py:_attachment_dict */
json_t *kd_attachment_dict(kd_io_t *io, const json_t *a) {
    (void)io;
    json_t *d = json_object();
    const char *keys[] = {"id","task_id","filename","content_type","size","uploaded_by","stored_path","created_at"};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++)
        json_set(d, keys[i], kd_field(a, keys[i]));
    return d;
}

/* PoP: kd_board_display_kwargs @ plugins/kanban/dashboard/plugin_api.py:_board_display_kwargs */
json_t *kd_board_display_kwargs(kd_io_t *io, const json_t *p) {
    (void)io;
    json_t *d = json_object();
    const char *keys[] = {"name","description","icon","color"};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++)
        json_set(d, keys[i], kd_field(p, keys[i]));
    return d;
}

/* PoP: kd_warnings_summary @ plugins/kanban/dashboard/plugin_api.py:_warnings_summary_from_diagnostics */
json_t *kd_warnings_summary(kd_io_t *io, const json_t *diags) {
    (void)io;
    if (!kd_truthy(diags)) return json_null();
    json_t *kinds = json_object();
    double count = 0, latest = 0;
    long highest_idx = -1; const char *highest_sev = NULL;
    size_t n = diags->c.count;
    for (size_t i = 0; i < n; i++) {
        json_t *d = diags->c.items[i];
        json_t *cnt = json_obj_get(d, "count");
        double nn = (cnt && cnt->type == JSON_NUMBER) ? cnt->num_val : 1;
        const char *kind = json_get_str(d, "kind", "");
        json_t *prev = json_obj_get(kinds, kind);
        double base = (prev && prev->type == JSON_NUMBER) ? prev->num_val : 0;
        json_set(kinds, kind, json_number(base + nn));
        count += nn;
        json_t *ls = json_obj_get(d, "last_seen_at");
        double lsv = kd_truthy(ls) ? (ls->type == JSON_NUMBER ? ls->num_val : 0) : 0;
        if (lsv > latest) latest = lsv;
        const char *sev = kd_str(json_obj_get(d, "severity"));
        if (sev) {
            for (long k = 0; k < KD_SEVERITY_ORDER_N; k++) {
                if (strcmp(sev, KD_SEVERITY_ORDER[k]) == 0) {
                    if (k > highest_idx) { highest_idx = k; highest_sev = KD_SEVERITY_ORDER[k]; }
                    break;
                }
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "count", json_number(count));
    json_set(out, "kinds", kinds);
    json_set(out, "latest_at", json_number(latest));
    json_set(out, "highest_severity", highest_sev ? json_string(highest_sev) : json_null());
    return out;
}

/* PoP: kd_require_ok @ plugins/kanban/dashboard/plugin_api.py:_require_ok */
json_t *kd_require_ok(kd_io_t *io, const json_t *ok) {
    (void)io;
    if (!kd_truthy(ok)) return kd_http_exc(404, "task not found");
    return json_null();
}

/* PoP: kd_conflict @ plugins/kanban/dashboard/plugin_api.py:_conflict */
json_t *kd_conflict(kd_io_t *io, const char *detail) {
    (void)io;
    return kd_http_exc(409, detail ? detail : "");
}

/* PoP: kd_require @ plugins/kanban/dashboard/plugin_api.py:_require */
json_t *kd_require(kd_io_t *io, const char *getter_name, const char *ident_key,
                   const json_t *ident, const char *label) {
    json_t *args = json_object();
    json_set(args, ident_key, ident ? json_copy(ident) : json_null());
    json_t *obj = kd_seam(io, getter_name, args);
    if (kd_raised(obj)) return obj;
    if (kd_is_none(obj)) {
        json_free(obj);
        char *idstr = kd_ident_str(ident);
        char *detail = kd_asprintf("%s %s not found", label, idstr);
        json_t *e = kd_http_exc(404, detail);
        free(idstr); free(detail);
        return e;
    }
    return obj;
}

/* PoP: kd_require_task @ plugins/kanban/dashboard/plugin_api.py:_require_task */
json_t *kd_require_task(kd_io_t *io, const json_t *task_id) {
    return kd_require(io, "kanban_db.get_task", "task_id", task_id, "task");
}

/* PoP: kd_require_run @ plugins/kanban/dashboard/plugin_api.py:_require_run */
json_t *kd_require_run(kd_io_t *io, const json_t *run_id) {
    return kd_require(io, "kanban_db.get_run", "run_id", run_id, "run");
}

/* PoP: kd_normalize_slug_or_400 @ plugins/kanban/dashboard/plugin_api.py:_normalize_slug_or_400 */
json_t *kd_normalize_slug_or_400(kd_io_t *io, const json_t *slug) {
    json_t *args = json_object();
    json_set(args, "slug", slug ? json_copy(slug) : json_null());
    json_t *r = kd_seam(io, "kanban_db._normalize_board_slug", args);
    if (kd_raised(r)) {
        if (kd_is_value_error(kd_raise_type(r))) {
            json_t *e = kd_http_exc(400, kd_raise_msg(r));
            json_free(r);
            return e;
        }
        return r;   /* non-ValueError (incl. BaseException) propagates */
    }
    return r;
}

/* PoP: kd_existing_board_slug @ plugins/kanban/dashboard/plugin_api.py:_existing_board_slug */
json_t *kd_existing_board_slug(kd_io_t *io, const json_t *slug) {
    json_t *normed = kd_normalize_slug_or_400(io, slug);
    if (kd_raised(normed)) return normed;
    bool missing = kd_is_none(normed) || !kd_truthy(normed);
    if (!missing) {
        json_t *args = json_object();
        json_set(args, "board", json_copy(normed));
        json_t *be = kd_seam(io, "kanban_db.board_exists", args);
        if (kd_raised(be)) { json_free(normed); return be; }
        missing = !kd_truthy(be);
        json_free(be);
    }
    if (missing) {
        json_free(normed);
        char *rs = kd_py_repr_str(kd_str(slug) ? kd_str(slug) : "");
        char *detail = kd_asprintf("board %s does not exist", rs);
        json_t *e = kd_http_exc(404, detail);
        free(rs); free(detail);
        return e;
    }
    return normed;
}

/* PoP: kd_ws_upgrade_authorized @ plugins/kanban/dashboard/plugin_api.py:_ws_upgrade_authorized */
json_t *kd_ws_upgrade_authorized(kd_io_t *io) {
    json_t *imp = kd_seam(io, "import:hermes_cli.web_server_chat", NULL);
    if (kd_raised(imp)) return imp;   /* import not in a try: propagates */
    json_free(imp);
    json_t *a = kd_seam(io, "_ws_auth_ok", json_object());
    if (kd_raised(a)) return a;
    bool ok_a = kd_truthy(a);
    json_free(a);
    if (!ok_a) return json_bool(false);   /* short-circuit */
    json_t *b = kd_seam(io, "_ws_request_is_allowed", json_object());
    if (kd_raised(b)) return b;
    bool ok_b = kd_truthy(b);
    json_free(b);
    return json_bool(ok_a && ok_b);
}

/* PoP: kd_since_param @ plugins/kanban/dashboard/plugin_api.py:_since_param */
json_t *kd_since_param(kd_io_t *io, const json_t *query_params) {
    (void)io;
    json_t *raw = json_obj_get(query_params, "since");
    if (kd_is_none(raw)) return json_null();
    const char *s = kd_str(raw);
    if (!s) return json_null();
    char *stripped = kd_strip(s);
    if (stripped[0] == '\0') { free(stripped); return json_null(); }
    long long val;
    json_t *r;
    if (kd_parse_int(stripped, &val)) {
        if (val < 0) val = 0;
        r = json_number((double)val);
    } else {
        r = json_null();
    }
    free(stripped);
    return r;
}

/* PoP: kd_ws_board @ plugins/kanban/dashboard/plugin_api.py:_ws_board */
json_t *kd_ws_board(kd_io_t *io, const json_t *raw) {
    if (!kd_truthy(raw)) return json_null();
    json_t *args = json_object();
    json_set(args, "slug", json_copy(raw));
    json_t *r = kd_seam(io, "kanban_db._normalize_board_slug", args);
    if (kd_raised(r)) {
        if (kd_is_value_error(kd_raise_type(r))) { json_free(r); return json_null(); }
        return r;
    }
    return r;
}

/* PoP: kd_ws_board_live @ plugins/kanban/dashboard/plugin_api.py:_ws_board_live */
json_t *kd_ws_board_live(kd_io_t *io, const json_t *normed) {
    const char *s = kd_str(normed);
    if (!kd_truthy(normed) || (s && strcmp(s, KD_DEFAULT_BOARD) == 0))
        return normed ? json_copy(normed) : json_null();
    json_t *args = json_object();
    json_set(args, "board", json_copy(normed));
    json_t *be = kd_seam(io, "kanban_db.board_exists", args);
    if (kd_raised(be)) return be;
    json_t *r = kd_truthy(be) ? json_copy(normed) : json_null();
    json_free(be);
    return r;
}

/* PoP: kd_load_config_or_empty @ plugins/kanban/dashboard/plugin_api.py:_load_config_or_empty */
json_t *kd_load_config_or_empty(kd_io_t *io) {
    json_t *imp = kd_seam(io, "import:hermes_cli.config", NULL);
    if (kd_raised(imp)) {
        if (kd_is_base_exc(kd_raise_type(imp))) return imp;
        json_free(imp); return json_object();
    }
    json_free(imp);
    json_t *lc = kd_seam(io, "load_config", json_object());
    if (kd_raised(lc)) {
        if (kd_is_base_exc(kd_raise_type(lc))) return lc;
        json_free(lc); return json_object();
    }
    if (!kd_truthy(lc)) { json_free(lc); return json_object(); }
    return lc;
}

/* PoP: kd_active_profile_name @ plugins/kanban/dashboard/plugin_api.py:_active_profile_name */
json_t *kd_active_profile_name(kd_io_t *io) {
    json_t *imp = kd_seam(io, "import:hermes_cli.profiles", NULL);
    if (kd_raised(imp)) {
        if (kd_is_base_exc(kd_raise_type(imp))) return imp;
        json_free(imp); return json_string("default");
    }
    json_free(imp);
    json_t *g = kd_seam(io, "get_active_profile_name", json_object());
    if (kd_raised(g)) {
        if (kd_is_base_exc(kd_raise_type(g))) return g;
        json_free(g); return json_string("default");
    }
    if (!kd_truthy(g)) { json_free(g); return json_string("default"); }
    return g;
}

/* PoP: kd_resolve_board @ plugins/kanban/dashboard/plugin_api.py:_resolve_board */
json_t *kd_resolve_board(kd_io_t *io, const json_t *board) {
    const char *bs = kd_str(board);
    if (kd_is_none(board) || (bs && bs[0] == '\0')) return json_null();
    json_t *normed = kd_normalize_slug_or_400(io, board);
    if (kd_raised(normed)) return normed;
    const char *ns = kd_str(normed);
    if (kd_truthy(normed) && ns && strcmp(ns, KD_DEFAULT_BOARD) != 0) {
        json_t *args = json_object();
        json_set(args, "board", json_copy(normed));
        json_t *be = kd_seam(io, "kanban_db.board_exists", args);
        if (kd_raised(be)) { json_free(normed); return be; }
        bool exists = kd_truthy(be);
        json_free(be);
        if (!exists) {
            char *rs = kd_py_repr_str(ns);
            char *detail = kd_asprintf("board %s does not exist", rs);
            json_t *e = kd_http_exc(404, detail);
            free(rs); free(detail); json_free(normed);
            return e;
        }
        json_t *args2 = json_object();
        json_set(args2, "board", json_copy(normed));
        json_t *md = kd_seam(io, "kanban_db.read_board_metadata", args2);
        if (kd_raised(md)) { json_free(normed); return md; }
        bool archived = false;
        if (md && md->type == JSON_OBJECT) {
            archived = kd_truthy(json_obj_get(md, "archived"));
        } else {
            /* .get on a non-dict raises AttributeError (propagates). */
            json_t *e = kd_make_raise("AttributeError", "'NoneType' object has no attribute 'get'");
            json_free(md); json_free(normed);
            return e;
        }
        json_free(md);
        if (archived) {
            char *rs = kd_py_repr_str(ns);
            char *detail = kd_asprintf("board %s is archived", rs);
            json_t *e = kd_http_exc(404, detail);
            free(rs); free(detail); json_free(normed);
            return e;
        }
    }
    return normed;
}

/* PoP: kd_conn @ plugins/kanban/dashboard/plugin_api.py:_conn */
json_t *kd_conn(kd_io_t *io, const json_t *board) {
    json_t *args = json_object();
    json_set(args, "board", board ? json_copy(board) : json_null());
    json_t *r = kd_seam(io, "kanban_db.init_db", args);
    if (kd_raised(r)) {
        if (kd_is_base_exc(kd_raise_type(r))) return r;
        /* log.warning("kanban init_db failed: %s", exc) */
        json_t *exc = json_object();
        json_set(exc, "type", json_string(kd_raise_type(r)));
        json_set(exc, "message", json_string(kd_raise_msg(r)));
        json_t *la = json_object();
        json_set(la, "fmt", json_string("kanban init_db failed: %s"));
        json_set(la, "exc", exc);
        json_t *lr = kd_seam(io, "log.warning", la);
        json_free(r);
        if (kd_raised(lr)) return lr;
        json_free(lr);
    } else {
        json_free(r);
    }
    json_t *ca = json_object();
    json_set(ca, "board", board ? json_copy(board) : json_null());
    json_t *conn = kd_seam(io, "kbc.connect", ca);
    if (kd_raised(conn)) return conn;
    return conn;
}

/* PoP: kd_attach_diagnostics @ plugins/kanban/dashboard/plugin_api.py:_attach_diagnostics */
json_t *kd_attach_diagnostics(kd_io_t *io, json_t *task_d, const json_t *diags) {
    if (kd_truthy(diags)) {
        json_set(task_d, "diagnostics", json_copy(diags));
        json_t *w = kd_warnings_summary(io, diags);
        json_set(task_d, "warnings", w);
    }
    return json_null();
}

/* PoP: kd_task_dict @ plugins/kanban/dashboard/plugin_api.py:_task_dict */
json_t *kd_task_dict(kd_io_t *io, const json_t *task, const json_t *latest_summary,
                     const json_t *current_run_started_at) {
    json_t *d = task ? json_copy(task) : json_object();
    json_t *args = json_object();
    json_set(args, "task_id", json_string(json_get_str(task, "id", "")));
    json_t *age = kd_seam(io, "kanban_db.task_age", args);
    if (kd_raised(age)) {
        if (kd_is_base_exc(kd_raise_type(age))) { json_free(d); return age; }
        json_free(age);
        age = json_object();
        json_set(age, "created_age_seconds", json_null());
        json_set(age, "started_age_seconds", json_null());
        json_set(age, "time_to_complete_seconds", json_null());
    }
    json_set(d, "age", age);
    json_set(d, "latest_summary", latest_summary ? json_copy(latest_summary) : json_null());
    json_set(d, "current_run_started_at", current_run_started_at ? json_copy(current_run_started_at) : json_null());
    return d;
}

/* PoP: kd_links_for @ plugins/kanban/dashboard/plugin_api.py:_links_for */
json_t *kd_links_for(kd_io_t *io, const json_t *task_id) {
    const char *sqls[2] = {
        "SELECT parent_id FROM task_links WHERE child_id = ? ORDER BY parent_id",
        "SELECT child_id FROM task_links WHERE parent_id = ? ORDER BY child_id"};
    const char *cols[2] = {"parent_id", "child_id"};
    const char *names[2] = {"parents", "children"};
    json_t *out = json_object();
    for (int k = 0; k < 2; k++) {
        json_t *args = json_object();
        json_set(args, "sql", json_string(sqls[k]));
        json_t *params = json_array();
        json_append(params, task_id ? json_copy(task_id) : json_null());
        json_set(args, "params", params);
        json_t *rows = kd_seam(io, "conn.execute", args);
        if (kd_raised(rows)) { json_free(out); return rows; }
        json_t *ids = json_array();
        if (rows && rows->type == JSON_ARRAY) {
            for (size_t i = 0; i < rows->c.count; i++) {
                json_t *row = rows->c.items[i];
                json_t *v = json_obj_get(row, cols[k]);
                json_append(ids, v ? json_copy(v) : json_null());
            }
        }
        json_free(rows);
        json_set(out, names[k], ids);
    }
    return out;
}

/* PoP: kd_link_tasks @ plugins/kanban/dashboard/plugin_api.py:_link_tasks */
json_t *kd_link_tasks(kd_io_t *io, const json_t *links) {
    json_t *rows = json_array();
    json_t *parents = json_obj_get(links, "parents");
    json_t *children = json_obj_get(links, "children");
    /* dict.fromkeys([*parents, *children]) — dedup preserving first-seen order */
    json_t *seen = json_object();
    const json_t *lists[2] = {parents, children};
    for (int k = 0; k < 2; k++) {
        const json_t *lst = lists[k];
        if (!lst || lst->type != JSON_ARRAY) continue;
        for (size_t i = 0; i < lst->c.count; i++) {
            const char *tid = kd_str(lst->c.items[i]);
            if (!tid) continue;
            if (json_obj_get(seen, tid)) continue;
            json_set(seen, tid, json_bool(true));
            json_t *args = json_object();
            json_set(args, "task_id", json_string(tid));
            json_t *task = kd_seam(io, "kanban_db.get_task", args);
            if (kd_raised(task)) { json_free(rows); json_free(seen); return task; }
            if (kd_truthy(task)) {
                json_t *row = json_object();
                json_set(row, "id", kd_field(task, "id"));
                json_set(row, "title", kd_field(task, "title"));
                json_set(row, "status", kd_field(task, "status"));
                json_append(rows, row);
            }
            json_free(task);
        }
    }
    json_free(seen);
    return rows;
}

/* PoP: kd_drag_to @ plugins/kanban/dashboard/plugin_api.py:_drag_to */
json_t *kd_drag_to(kd_io_t *io, const json_t *task_id, const char *s) {
    json_t *current = NULL;
    if (strcmp(s, "triage") != 0) {
        json_t *args = json_object();
        json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
        current = kd_seam(io, "kanban_db.get_task", args);
        if (kd_raised(current)) return current;
    }
    const char *cstatus = kd_str(json_obj_get(current, "status"));
    if (strcmp(s, "ready") == 0 && kd_truthy(current) && cstatus &&
        (strcmp(cstatus, "blocked") == 0 || strcmp(cstatus, "scheduled") == 0)) {
        json_free(current);
        json_t *args = json_object();
        json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
        json_t *r = kd_seam(io, "kanban_db.unblock_task", args);
        return r;
    }
    if (!kd_is_none(current) && cstatus && strcmp(cstatus, "review") == 0) {
        json_free(current);
        json_t *args = json_object();
        json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
        json_t *r = kd_seam(io, "kanban_db.reopen_review_task", args);
        return r;
    }
    json_free(current);
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    json_set(args, "status", json_string(s));
    json_t *r = kd_seam(io, "_set_status_direct", args);
    return r;
}

/* PoP: kd_apply_status @ plugins/kanban/dashboard/plugin_api.py:_apply_status */
json_t *kd_apply_status(kd_io_t *io, const json_t *task_id, const char *s,
                        const json_t *p, const char *unknown_detail) {
    if (strcmp(s, "running") == 0) return kd_make_raise("_StatusRejected", KD_RUNNING_DIRECT_MSG);
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    if (strcmp(s, "done") == 0) {
        json_set(args, "result", kd_field(p, "result"));
        json_set(args, "summary", kd_field(p, "summary"));
        json_set(args, "metadata", kd_field(p, "metadata"));
        json_set(args, "force", json_bool(true));
        return kd_seam(io, "kanban_db.complete_task", args);
    }
    if (strcmp(s, "blocked") == 0 || strcmp(s, "scheduled") == 0) {
        json_set(args, "reason", kd_field(p, "block_reason"));
        return kd_seam(io, strcmp(s, "blocked") == 0 ? "kanban_db.block_task" : "kanban_db.schedule_task", args);
    }
    if (strcmp(s, "review") == 0) {
        json_set(args, "summary", kd_field(p, "summary"));
        json_set(args, "metadata", kd_field(p, "metadata"));
        json_t *assignee = json_obj_get(p, "assignee");
        json_set(args, "reviewer", kd_truthy(assignee) ? json_copy(assignee) : json_null());
        json_set(args, "force", json_bool(true));
        return kd_seam(io, "kanban_db.request_review", args);
    }
    if (strcmp(s, "ready") == 0 || strcmp(s, "todo") == 0 || strcmp(s, "triage") == 0) {
        json_free(args);
        return kd_drag_to(io, task_id, s);
    }
    json_free(args);
    return kd_make_raise("_StatusRejected", unknown_detail ? unknown_detail : "");
}

/* PoP: kd_set_priority @ plugins/kanban/dashboard/plugin_api.py:_set_priority */
json_t *kd_set_priority(kd_io_t *io, const json_t *task_id, const json_t *priority, const json_t *board) {
    long long pv = 0;
    if (priority && priority->type == JSON_NUMBER) pv = (long long)priority->num_val;
    else if (priority && priority->type == JSON_BOOL) pv = priority->bool_val ? 1 : 0;
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    json_set(args, "priority", json_number((double)pv));
    json_set(args, "board", board ? json_copy(board) : json_null());
    json_t *r = kd_seam(io, "kanban_db.edit_task", args);
    if (kd_raised(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: kd_apply_model_override @ plugins/kanban/dashboard/plugin_api.py:_apply_model_override */
json_t *kd_apply_model_override(kd_io_t *io, const json_t *task_id, const json_t *p) {
    json_t *new_model;
    if (json_get_bool(p, "clear_model_override", false)) {
        new_model = json_null();
    } else {
        json_t *mo = json_obj_get(p, "model_override");
        const char *base = kd_truthy(mo) ? (kd_str(mo) ? kd_str(mo) : "") : "";
        char *stripped = kd_strip(base);
        new_model = stripped[0] ? json_string(stripped) : json_null();
        free(stripped);
    }
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    json_set(args, "model", new_model);
    json_set(args, "provider", kd_field(p, "provider_override"));
    json_t *r = kd_seam(io, "kanban_db.set_model_override", args);
    return r;
}

/* PoP: kd_apply_reasoning_effort @ plugins/kanban/dashboard/plugin_api.py:_apply_reasoning_effort */
json_t *kd_apply_reasoning_effort(kd_io_t *io, const json_t *task_id, const json_t *p) {
    json_t *effort = json_get_bool(p, "clear_reasoning_effort", false)
        ? json_null() : kd_field(p, "reasoning_effort");
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    json_set(args, "effort", effort);
    json_t *r = kd_seam(io, "kanban_db.set_reasoning_effort", args);
    return r;
}

/* PoP: kd_open_parent_refusal @ plugins/kanban/dashboard/plugin_api.py:_open_parent_refusal */
json_t *kd_open_parent_refusal(kd_io_t *io, const json_t *task_id, const char *s) {
    if (strcmp(s, "done") != 0 && strcmp(s, "review") != 0) return json_null();
    json_t *args = json_object();
    json_set(args, "task_id", task_id ? json_copy(task_id) : json_null());
    json_t *bl = kd_seam(io, "kanban_db.unsatisfied_parents", args);
    if (kd_raised(bl)) return bl;
    if (!kd_truthy(bl)) { json_free(bl); return json_null(); }
    kd_buf detail; kd_buf_init(&detail);
    if (bl->type == JSON_ARRAY) {
        for (size_t i = 0; i < bl->c.count; i++) {
            json_t *pair = bl->c.items[i];
            const char *pid = kd_str(json_get(pair, 0));
            const char *status = kd_str(json_get(pair, 1));
            if (i) kd_buf_push(&detail, ", ");
            char *frag = kd_asprintf("%s (%s)", pid ? pid : "", status ? status : "");
            kd_buf_push(&detail, frag); free(frag);
        }
    }
    json_free(bl);
    char *rs = kd_py_repr_str(s);
    const char *tid = kd_str(task_id) ? kd_str(task_id) : "";
    char *msg = kd_asprintf(
        "cannot move %s to %s: unsatisfied parent dependencies: %s; complete the parents first (done or archived)",
        tid, rs, detail.s);
    free(rs); free(detail.s);
    json_t *r = json_string(msg); free(msg);
    return r;
}
