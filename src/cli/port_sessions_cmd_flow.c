/* port_sessions_cmd_flow.c — hermes_cli/sessions_cmd.py session-management FLOW.
 *
 * Decision-sequence port: the confirmation exit-code chain, dry-run preview
 * and pinned-skip formatting gates, flat-export renderer dispatch, output
 * path resolution, delete/pin/rename flows and the recovery verdict. All
 * store/file/stdin I/O rides the injected universal seam (scf_call); the
 * harness scripts results and records the ordered trace the oracle diffs
 * against LIVE hermes_cli/sessions_cmd.py driven under the same scripts.
 *
 * Representation contract: see include/port_sessions_cmd_flow.h.
 * libjson-only (default oracle deps suffice).
 */
#include "port_sessions_cmd_flow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── string helpers (ASCII fixture domain; Python str semantics) ─────────── */

static char *scf_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static bool scf_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() — both ends, ASCII whitespace fixture domain. */
static char *scf_strip(const char *s) {
    while (*s && scf_is_ws(*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && scf_is_ws(s[n - 1])) n--;
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

static bool scf_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool scf_ends_with(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return m <= n && strcmp(s + n - m, suffix) == 0;
}

/* ── JSON value helpers ──────────────────────────────────────────────────── */

static bool scf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* Python truthiness for a JSON node (NULL pointer == None == falsy). */
static bool scf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* String value of a node ("" for non-strings; fixture domain). */
static const char *scf_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool scf_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool scf_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }

/* f-string interpolation of a nullable value: None -> "None", bool ->
 * "True"/"False", integral number -> "%.0f". Container repr is out of the
 * fixture domain ("" placeholder). */
static char *scf_py_str(const json_t *v) {
    if (scf_is_none(v)) return scf_strdup("None");
    if (v->type == JSON_BOOL) return scf_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return scf_strdup(buf);
    }
    if (v->type == JSON_STRING) return scf_strdup(scf_jstr(v));
    return scf_strdup("");
}

/* ── string builder ──────────────────────────────────────────────────────── */

typedef struct { char *buf; size_t len, cap; } scf_sb;

static void scf_sb_init(scf_sb *sb) { sb->buf = NULL; sb->len = 0; sb->cap = 0; }

static void scf_sb_append(scf_sb *sb, const char *s) {
    size_t n = strlen(s);
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = sb->cap ? sb->cap * 2 : 128;
        while (nc < sb->len + n + 1) nc *= 2;
        char *nb = (char *)realloc(sb->buf, nc);
        if (!nb) return;
        sb->buf = nb; sb->cap = nc;
    }
    memcpy(sb->buf + sb->len, s, n + 1);
    sb->len += n;
}

static void scf_sb_appendf(scf_sb *sb, const char *fmt, ...) {
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    scf_sb_append(sb, tmp);
}

static char *scf_sb_take(scf_sb *sb) {
    if (!sb->buf) return scf_strdup("");
    return sb->buf;
}

/* json_string COPIES its input — hand off the builder buffer without leaking. */
static json_t *scf_sb_json(scf_sb *sb) {
    char *s = scf_sb_take(sb);
    json_t *j = json_string(s);
    free(s);
    return j;
}

/* ── exception plumbing ──────────────────────────────────────────────────── */

bool scf_exc_pending(const scf_ctx *ctx) { return ctx && ctx->exc != NULL; }

void scf_exc_clear(scf_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}

void scf_raise(scf_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}

bool scf_exc_is(const scf_ctx *ctx, const char *cls) {
    return ctx && ctx->exc && scf_str_eq(json_get_str(ctx->exc, "__raise__", ""), cls);
}

/* ── universal seam ──────────────────────────────────────────────────────── */

static json_t *scf_script_result(scf_ctx *ctx, const json_t *script) {
    /* Returns a NEW value (NULL == None); sets ctx->exc for a scripted raise. */
    if (!script || script->type == JSON_NULL) return NULL;
    if (script->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(script, "__raise__");
        if (raise && raise->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(script);
            return NULL;
        }
    }
    return json_copy(script);
}

json_t *scf_call(scf_ctx *ctx, const char *name, json_t *args) {
    /* args ownership is transferred (NULL -> empty object). Result is NEW. */
    if (!args) args = json_object();
    json_t *ret = NULL;
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *chosen = script;
    if (script && script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = json_object();
                json_set(ctx->exc, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = scf_script_result(ctx, chosen);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (ctx->exc && chosen == ctx->exc) json_set(ev, "ret", json_copy(ctx->exc));
    else if (ctx->exc && chosen && chosen->type == JSON_OBJECT && json_obj_get(chosen, "__raise__"))
        json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    return ret;
}

/* Arg builders: None-valued keys are dropped on BOTH sides ("" is kept). */
static void scf_arg_json(json_t *args, const char *key, json_t *val) {
    /* adopts val */
    if (scf_is_none(val)) { json_free(val); return; }
    json_set(args, key, val);
}

static void scf_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}

static void scf_arg_copy(json_t *args, const char *key, const json_t *val) {
    if (scf_is_none(val)) return;
    json_set(args, key, json_copy(val));
}

/* builtins.print(text) — single positional, default end="\n", stdout. */
static void scf_print(scf_ctx *ctx, const char *text) {
    json_t *a = json_object();
    json_set(a, "text", json_string(text ? text : ""));
    json_set(a, "end", json_string("\n"));
    json_t *r = scf_call(ctx, "print", a);
    json_free(r);
}

/* Direct attribute access on a namespace double: a missing key raises
 * AttributeError exactly like SimpleNamespace; a present null reads None. */
static json_t *scf_attr(scf_ctx *ctx, const json_t *obj, const char *key) {
    if (!scf_is_obj(obj) || !json_has(obj, key)) {
        scf_raise(ctx, "AttributeError");
        return NULL;
    }
    return json_obj_get(obj, key);
}

/* ── path helpers (POSIX fixture domain; os.path == posixpath) ───────────── */

/* PurePosixPath normalization: collapse duplicate slashes and "." components,
 * keep "..", preserve a leading "/"; "" reads ".". */
static char *scf_posix_norm(const char *s) {
    if (!s || !*s) return scf_strdup(".");
    bool absolute = s[0] == '/';
    scf_sb sb; scf_sb_init(&sb);
    size_t n = strlen(s), i = 0;
    bool first = true;
    while (i < n) {
        while (i < n && s[i] == '/') i++;
        size_t start = i;
        while (i < n && s[i] != '/') i++;
        if (i == start) break;
        if (i - start == 1 && s[start] == '.') continue;
        if (!first) scf_sb_append(&sb, "/");
        size_t seglen = i - start;
        char *seg = (char *)malloc(seglen + 1);
        if (seg) { memcpy(seg, s + start, seglen); seg[seglen] = '\0'; }
        scf_sb_append(&sb, seg ? seg : "");
        free(seg);
        first = false;
    }
    char *body = scf_sb_take(&sb);
    scf_sb out; scf_sb_init(&out);
    if (absolute) scf_sb_append(&out, "/");
    scf_sb_append(&out, body);
    free(body);
    char *res = scf_sb_take(&out);
    if (!absolute && !*res) { free(res); return scf_strdup("."); }
    return res;
}

/* pathlib Path(a) / b — normalize, join, normalize (FakePath double). */
static char *scf_path_join(const char *a, const char *b) {
    char *na = scf_posix_norm(a);
    scf_sb sb; scf_sb_init(&sb);
    scf_sb_append(&sb, na);
    scf_sb_append(&sb, "/");
    scf_sb_append(&sb, b ? b : "");
    free(na);
    char *joined = scf_sb_take(&sb);
    char *res = scf_posix_norm(joined);
    free(joined);
    return res;
}

/* posixpath.join(a, b) — NO normalization (a trailing "//" survives). */
static char *scf_os_path_join(const char *a, const char *b) {
    if (!b) b = "";
    if (b[0] == '/') return scf_strdup(b);
    if (!a || !*a) return scf_strdup(b);
    scf_sb sb; scf_sb_init(&sb);
    scf_sb_append(&sb, a);
    if (!scf_ends_with(a, "/")) scf_sb_append(&sb, "/");
    scf_sb_append(&sb, b);
    return scf_sb_take(&sb);
}

/* ── numeric helpers ─────────────────────────────────────────────────────── */

/* int(v) for the fixture domain (number/bool/null): trunc toward zero;
 * null (and out-of-domain shapes) raise TypeError like int(None). */
static long long scf_py_int(scf_ctx *ctx, const json_t *v) {
    if (v && v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    scf_raise(ctx, "TypeError");
    return 0;
}

/* int(v or 0) — the recovery-count coercion. */
static long long scf_int_or_zero(scf_ctx *ctx, const json_t *v) {
    if (!scf_truthy(v)) return 0;
    return scf_py_int(ctx, v);
}

/* f"{n:,}" — thousands grouping for the recovery verdict. */
static char *scf_commas(long long n) {
    char digits[64];
    snprintf(digits, sizeof(digits), "%lld", n < 0 ? -n : n);
    size_t len = strlen(digits);
    size_t groups = (len + 2) / 3;
    size_t cap = len + groups + 2;
    char *out = (char *)malloc(cap);
    if (!out) return scf_strdup("");
    size_t pos = 0;
    if (n < 0) out[pos++] = '-';
    size_t lead = len % 3 ? len % 3 : 3;
    memcpy(out + pos, digits, lead);
    pos += lead;
    for (size_t i = lead; i < len; i += 3) {
        out[pos++] = ',';
        memcpy(out + pos, digits + i, 3);
        pos += 3;
    }
    out[pos] = '\0';
    return out;
}

/* ── PruneCriteria double (the real frozen/slots dataclass, 25 fields) ───── */

typedef struct { const char *name; bool bool_default; } scf_pc_field;
static const scf_pc_field SCF_PC_FIELDS[] = {
    {"last_active_before", false}, {"last_active_after", false},
    {"started_before", false}, {"started_after", false},
    {"title_like", false}, {"end_reason", false}, {"cwd_prefix", false},
    {"min_messages", false}, {"max_messages", false}, {"model_like", false},
    {"provider", false}, {"user_id", false}, {"chat_id", false},
    {"chat_type", false}, {"branch_like", false}, {"min_tokens", false},
    {"max_tokens", false}, {"min_cost", false}, {"max_cost", false},
    {"min_tool_calls", false}, {"max_tool_calls", false}, {"archived", false},
    {"include_pinned", true}, {"lineage_tips_only", true}, {"include_open", true},
};
#define SCF_PC_NFIELDS (sizeof(SCF_PC_FIELDS) / sizeof(SCF_PC_FIELDS[0]))

static bool scf_pc_known(const char *key) {
    for (size_t i = 0; i < SCF_PC_NFIELDS; i++)
        if (scf_str_eq(SCF_PC_FIELDS[i].name, key)) return true;
    return false;
}

/* PruneCriteria(**criteria): defaults filled in declaration order; an
 * unknown keyword raises TypeError exactly like the dataclass __init__. */
static json_t *scf_prune_criteria_new(scf_ctx *ctx, const json_t *criteria) {
    json_t *obj = json_object();
    for (size_t i = 0; i < SCF_PC_NFIELDS; i++) {
        const scf_pc_field *f = &SCF_PC_FIELDS[i];
        json_set(obj, f->name, f->bool_default ? json_bool(false) : json_null());
    }
    if (scf_is_obj(criteria)) {
        for (size_t i = 0; i < criteria->c.count; i++) {
            const char *key = criteria->c.keys[i];
            if (!scf_pc_known(key)) {
                scf_raise(ctx, "TypeError");
                json_free(obj);
                return NULL;
            }
            json_set(obj, key, json_copy(criteria->c.items[i]));
        }
    }
    return obj;
}

/* ── internal unclaimed steps (REAL in the oracle via AST extraction) ────── */

/* _sessions_dir(): get_hermes_home() / "sessions" */
static json_t *scf_sessions_dir(scf_ctx *ctx) {
    json_t *home = scf_call(ctx, "get_hermes_home", NULL);
    if (scf_exc_pending(ctx)) { json_free(home); return NULL; }
    if (scf_is_none(home) || home->type != JSON_STRING) {
        scf_raise(ctx, "TypeError");
        json_free(home);
        return NULL;
    }
    char *p = scf_path_join(home->str_val, "sessions");
    json_free(home);
    json_t *ret = json_string(p);
    free(p);
    return ret;
}

/* _media_root(): str(native_inputs_root()) */
static json_t *scf_media_root(scf_ctx *ctx) {
    json_t *r = scf_call(ctx, "native_inputs_root", NULL);
    if (scf_exc_pending(ctx)) { json_free(r); return NULL; }
    char *s = scf_py_str(r);
    json_free(r);
    json_t *ret = json_string(s);
    free(s);
    return ret;
}

/* ── ported flows/leaves ─────────────────────────────────────────────────── */

static const char *SCF_FILTER_ARGS[] = {
    "older_than", "newer_than", "before", "after", "source", "title", "end_reason",
    "cwd", "min_messages", "max_messages", "model", "provider", "user", "chat_id",
    "chat_type", "branch", "min_tokens", "max_tokens", "min_cost", "max_cost",
    "min_tool_calls", "max_tool_calls",
};
#define SCF_NFILTER_ARGS (sizeof(SCF_FILTER_ARGS) / sizeof(SCF_FILTER_ARGS[0]))

/* PoP: scf_any_filter_args @ hermes_cli/sessions_cmd.py:_any_filter_args */
json_t *scf_any_filter_args(scf_ctx *ctx, const json_t *args) {
    (void)ctx;
    /* any(getattr(args, a, None) is not None for a in _FILTER_ARGS) —
     * false/0/"" are NOT None, so they count; short-circuit on first hit. */
    for (size_t i = 0; i < SCF_NFILTER_ARGS; i++) {
        json_t *v = json_obj_get(args, SCF_FILTER_ARGS[i]);
        if (v && v->type != JSON_NULL) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: scf_prune_args @ hermes_cli/sessions_cmd.py:_prune_args */
json_t *scf_prune_args(scf_ctx *ctx, const json_t *filters) {
    /* criteria = dict(filters); bounds pops older_than_days then source. */
    if (!scf_is_obj(filters)) {
        scf_raise(ctx, "TypeError");
        return NULL;
    }
    json_t *criteria = json_copy(filters);
    json_t *bounds = json_object();
    const char *bound_keys[2] = {"older_than_days", "source"};
    for (int i = 0; i < 2; i++) {
        if (json_has(criteria, bound_keys[i])) {
            json_t *v = json_copy(json_obj_get(criteria, bound_keys[i]));
            json_obj_del(criteria, bound_keys[i]);
            json_set(bounds, bound_keys[i], v);
        }
    }
    json_t *pc = scf_prune_criteria_new(ctx, criteria);
    json_free(criteria);
    if (!pc) { json_free(bounds); return NULL; }
    json_t *ret = json_array();
    json_append(ret, bounds);
    json_append(ret, pc);
    return ret;
}

/* PoP: scf_confirm_or_exit_code @ hermes_cli/sessions_cmd.py:_confirm_or_exit_code */
json_t *scf_confirm_or_exit_code(scf_ctx *ctx, const char *prompt,
                                 const char *declined, const char *unattended) {
    json_t *a = json_object();
    scf_arg_str(a, "prompt", prompt);
    json_t *answer = scf_call(ctx, "confirm_prompt", a);
    if (scf_exc_pending(ctx)) {
        json_free(answer);
        if (scf_exc_is(ctx, "KeyboardInterrupt")) {
            scf_exc_clear(ctx);
            scf_print(ctx, "\nInterrupted; nothing was changed.");
            if (scf_exc_pending(ctx)) return NULL;
            return json_number(130);
        }
        return NULL;
    }
    bool yes = scf_truthy(answer);
    bool none = scf_is_none(answer);
    json_free(answer);
    if (yes) return NULL;
    if (none) {
        scf_sb sb; scf_sb_init(&sb);
        scf_sb_append(&sb, "No answer on a non-interactive stdin; nothing was changed.");
        if (unattended && unattended[0])
            scf_sb_appendf(&sb, " Pass %s to run it unattended.", unattended);
        else
            scf_sb_append(&sb, " Run it from an interactive terminal.");
        char *text = scf_sb_take(&sb);
        scf_print(ctx, text);
        free(text);
        if (scf_exc_pending(ctx)) return NULL;
        return json_number(1);
    }
    scf_print(ctx, declined ? declined : "None");
    if (scf_exc_pending(ctx)) return NULL;
    return json_number(0);
}

/* PoP: scf_not_found @ hermes_cli/sessions_cmd.py:_not_found */
json_t *scf_not_found(scf_ctx *ctx, const json_t *session_id) {
    char *sid = scf_py_str(session_id);
    scf_sb sb; scf_sb_init(&sb);
    scf_sb_appendf(&sb, "No session '%s'. Run: hermes sessions list to find the id.", sid);
    free(sid);
    char *text = scf_sb_take(&sb);
    scf_print(ctx, text);
    free(text);
    if (scf_exc_pending(ctx)) return NULL;
    return json_number(1);
}

/* PoP: scf_print_empty_store @ hermes_cli/sessions_cmd.py:_print_empty_store */
json_t *scf_print_empty_store(scf_ctx *ctx, const char *action, const json_t *args) {
    if (scf_str_eq(action, "stats")) {
        scf_print(ctx, "Total sessions: 0\nTotal messages: 0");
    } else if (scf_str_eq(action, "pinned")) {
        json_t *j = json_obj_get(args, "json");
        scf_print(ctx, scf_truthy(j) ? "[]"
                  : "No pinned sessions. Pin one with: hermes sessions pin <session_id>");
    } else {
        scf_print(ctx, "No sessions found.");
    }
    if (scf_exc_pending(ctx)) return NULL;
    return NULL;
}

/* PoP: scf_delete @ hermes_cli/sessions_cmd.py:_delete */
json_t *scf_delete(scf_ctx *ctx, const char *session_id, const json_t *fences) {
    /* media_root=_media_root() is evaluated BEFORE sessions.delete fires. */
    json_t *mr = scf_media_root(ctx);
    if (scf_exc_pending(ctx)) { json_free(mr); return NULL; }
    json_t *a = json_object();
    scf_arg_str(a, "session_id", session_id);
    json_set(a, "refuse_guarded", json_bool(true));
    scf_arg_json(a, "media_root", mr);
    if (scf_is_obj(fences)) {
        for (size_t i = 0; i < fences->c.count; i++)
            scf_arg_json(a, fences->c.keys[i], json_copy(fences->c.items[i]));
    }
    json_t *removed = scf_call(ctx, "sessions_delete", a);
    if (scf_exc_pending(ctx)) { json_free(removed); return NULL; }
    /* sessions_dir=_sessions_dir() fires get_hermes_home BEFORE the call. */
    json_t *sd = scf_sessions_dir(ctx);
    if (scf_exc_pending(ctx)) { json_free(removed); json_free(sd); return NULL; }
    json_t *aa = json_object();
    scf_arg_str(aa, "profile_dir", json_get_str(ctx->env, "profile_dir", NULL));
    scf_arg_copy(aa, "removed", removed);
    scf_arg_str(aa, "reason", "delete");
    scf_arg_str(aa, "primary", session_id);
    scf_arg_json(aa, "sessions_dir", sd);
    json_t *ar = scf_call(ctx, "after_sessions_removed", aa);
    json_free(ar);
    if (scf_exc_pending(ctx)) { json_free(removed); return NULL; }
    bool ret = scf_truthy(removed);
    json_free(removed);
    return json_bool(ret);
}

/* PoP: scf_cmd_delete @ hermes_cli/sessions_cmd.py:_cmd_delete */
json_t *scf_cmd_delete(scf_ctx *ctx, const json_t *args) {
    json_t *resolved = NULL, *record = NULL, *ret = NULL;
    json_t *sid_node = scf_attr(ctx, args, "session_id");
    if (scf_exc_pending(ctx)) return NULL;
    json_t *ra = json_object();
    scf_arg_copy(ra, "session_id", sid_node);
    resolved = scf_call(ctx, "resolve_id", ra);
    if (scf_exc_pending(ctx)) goto done;
    if (!scf_truthy(resolved)) {
        ret = scf_not_found(ctx, sid_node);
        goto done;
    }
    const char *rid = scf_jstr(resolved);
    json_t *ga = json_object();
    scf_arg_str(ga, "session_id", rid);
    record = scf_call(ctx, "session_get", ga);
    if (scf_exc_pending(ctx)) goto done;
    bool pinned = false;
    if (!scf_is_none(record)) {
        json_t *pv = scf_attr(ctx, record, "pinned");
        if (scf_exc_pending(ctx)) goto done;
        pinned = scf_truthy(pv);
    }
    json_t *yes = scf_attr(ctx, args, "yes");
    if (scf_exc_pending(ctx)) goto done;
    if (!scf_truthy(yes)) {
        scf_sb pb; scf_sb_init(&pb);
        scf_sb_appendf(&pb, "Delete session '%s'%s and all its messages? [y/N] ",
                       rid, pinned ? " (this session is PINNED)" : "");
        char *prompt = scf_sb_take(&pb);
        json_t *refused = scf_confirm_or_exit_code(ctx, prompt, "Cancelled.", "--yes");
        free(prompt);
        if (scf_exc_pending(ctx)) { json_free(refused); goto done; }
        if (!scf_is_none(refused)) { ret = refused; goto done; }
        json_free(refused);
    } else if (pinned) {
        scf_sb wb; scf_sb_init(&wb);
        scf_sb_appendf(&wb, "Warning: deleting a pinned session '%s'.", rid);
        char *warning = scf_sb_take(&wb);
        scf_print(ctx, warning);
        free(warning);
        if (scf_exc_pending(ctx)) goto done;
    }
    json_t *deleted = scf_delete(ctx, rid, NULL);
    if (scf_exc_pending(ctx)) {
        json_free(deleted);
        if (scf_exc_is(ctx, "SessionWriteGuarded")) {
            const char *msg = json_get_str(ctx->exc, "msg", NULL);
            scf_sb eb; scf_sb_init(&eb);
            scf_sb_append(&eb, "Cannot delete active session: ");
            if (msg) {
                scf_sb_append(&eb, msg);
            } else {
                const char *esid = json_get_str(ctx->exc, "session_id", "");
                scf_sb_appendf(&eb, "session '%s' (or a delegate child) has an active "
                               "turn lease or compression lock", esid);
            }
            scf_exc_clear(ctx);
            char *etext = scf_sb_take(&eb);
            scf_print(ctx, etext);
            free(etext);
            if (scf_exc_pending(ctx)) goto done;
            ret = json_number(1);
            goto done;
        }
        if (scf_exc_is(ctx, "AuthorityRefusal")) {
            const char *reason = json_get_str(ctx->exc, "reason", "");
            scf_exc_clear(ctx);
            scf_sb eb; scf_sb_init(&eb);
            if (scf_str_eq(reason, "unknown_execution")) {
                scf_sb_appendf(&eb, "Cannot delete session '%s': a turn was lost across a "
                               "gateway restart. Acknowledge it first with: hermes sessions "
                               "discard %s", rid, rid);
            } else if (scf_str_eq(reason, "session_busy")) {
                scf_sb_appendf(&eb, "Cannot delete session '%s': it has a queued or running "
                               "turn. Wait for it to finish (or stop it), then retry.", rid);
            } else {
                scf_sb_appendf(&eb, "Cannot delete session '%s': %s", rid, reason);
            }
            char *etext = scf_sb_take(&eb);
            scf_print(ctx, etext);
            free(etext);
            if (scf_exc_pending(ctx)) goto done;
            ret = json_number(1);
            goto done;
        }
        goto done; /* other exceptions propagate */
    }
    if (!scf_truthy(deleted)) {
        json_free(deleted);
        ret = scf_not_found(ctx, sid_node);
        goto done;
    }
    json_free(deleted);
    scf_sb db; scf_sb_init(&db);
    scf_sb_appendf(&db, "Deleted session '%s'.", rid);
    char *dtext = scf_sb_take(&db);
    scf_print(ctx, dtext);
    free(dtext);
    if (scf_exc_pending(ctx)) goto done;
done:
    json_free(resolved);
    json_free(record);
    return ret;
}

/* PoP: scf_cmd_rename @ hermes_cli/sessions_cmd.py:_cmd_rename */
json_t *scf_cmd_rename(scf_ctx *ctx, const json_t *args) {
    json_t *resolved = NULL, *ret = NULL;
    char *title = NULL;
    json_t *sid_node = scf_attr(ctx, args, "session_id");
    if (scf_exc_pending(ctx)) return NULL;
    json_t *ra = json_object();
    scf_arg_copy(ra, "session_id", sid_node);
    resolved = scf_call(ctx, "resolve_id", ra);
    if (scf_exc_pending(ctx)) goto done;
    if (!scf_truthy(resolved)) {
        ret = scf_not_found(ctx, sid_node);
        goto done;
    }
    const char *rid = scf_jstr(resolved);
    json_t *title_node = scf_attr(ctx, args, "title");
    if (scf_exc_pending(ctx)) goto done;
    if (!scf_is_arr(title_node)) {
        scf_raise(ctx, "TypeError");
        goto done;
    }
    scf_sb tb; scf_sb_init(&tb);
    for (size_t i = 0; i < title_node->c.count; i++) {
        json_t *part = title_node->c.items[i];
        if (!part || part->type != JSON_STRING) {
            scf_raise(ctx, "TypeError");
            free(scf_sb_take(&tb));
            goto done;
        }
        if (i) scf_sb_append(&tb, " ");
        scf_sb_append(&tb, part->str_val);
    }
    title = scf_sb_take(&tb);
    char *stripped = scf_strip(title);
    bool empty = !stripped[0];
    free(stripped);
    if (empty) {
        scf_print(ctx, "Error: title cannot be empty or whitespace-only.");
        if (scf_exc_pending(ctx)) goto done;
        ret = json_number(1);
        goto done;
    }
    if (strchr(title, '\n') || strchr(title, '\r')) {
        scf_print(ctx, "Error: title cannot contain newlines.");
        if (scf_exc_pending(ctx)) goto done;
        ret = json_number(1);
        goto done;
    }
    json_t *sa = json_object();
    scf_arg_str(sa, "session_id", rid);
    scf_arg_str(sa, "title", title);
    json_t *ok = scf_call(ctx, "set_title", sa);
    if (scf_exc_pending(ctx)) {
        json_free(ok);
        if (scf_exc_is(ctx, "ValueError")) {
            const char *msg = json_get_str(ctx->exc, "msg", "");
            scf_exc_clear(ctx);
            scf_sb eb; scf_sb_init(&eb);
            scf_sb_appendf(&eb, "Error: %s", msg);
            char *etext = scf_sb_take(&eb);
            scf_print(ctx, etext);
            free(etext);
            if (scf_exc_pending(ctx)) goto done;
            ret = json_number(1);
            goto done;
        }
        goto done;
    }
    if (!scf_truthy(ok)) {
        json_free(ok);
        ret = scf_not_found(ctx, sid_node);
        goto done;
    }
    json_free(ok);
    scf_sb rb; scf_sb_init(&rb);
    scf_sb_appendf(&rb, "Session '%s' renamed to: %s", rid, title);
    char *rtext = scf_sb_take(&rb);
    scf_print(ctx, rtext);
    free(rtext);
    if (scf_exc_pending(ctx)) goto done;
done:
    free(title);
    json_free(resolved);
    return ret;
}

/* PoP: scf_cmd_pin @ hermes_cli/sessions_cmd.py:_cmd_pin */
json_t *scf_cmd_pin(scf_ctx *ctx, const json_t *args, bool pinning) {
    json_t *ids = scf_attr(ctx, args, "session_ids");
    if (scf_exc_pending(ctx)) return NULL;
    if (!scf_is_arr(ids)) {
        scf_raise(ctx, "TypeError");
        return NULL;
    }
    long long failures = 0;
    for (size_t i = 0; i < ids->c.count; i++) {
        json_t *raw = ids->c.items[i];
        json_t *ra = json_object();
        scf_arg_copy(ra, "session_id", raw);
        json_t *resolved = scf_call(ctx, "resolve_id", ra);
        if (scf_exc_pending(ctx)) { json_free(resolved); return NULL; }
        bool ok = false;
        if (scf_truthy(resolved)) {
            json_t *pa = json_object();
            scf_arg_str(pa, "session_id", scf_jstr(resolved));
            json_set(pa, "pinned", json_bool(pinning));
            json_t *sp = scf_call(ctx, "set_pinned", pa);
            if (scf_exc_pending(ctx)) { json_free(resolved); json_free(sp); return NULL; }
            ok = scf_truthy(sp);
            json_free(sp);
        }
        if (ok) {
            const char *rid = scf_jstr(resolved);
            json_t *ta = json_object();
            scf_arg_str(ta, "session_id", rid);
            json_t *title = scf_call(ctx, "get_title", ta);
            if (scf_exc_pending(ctx)) { json_free(resolved); json_free(title); return NULL; }
            scf_sb pb; scf_sb_init(&pb);
            scf_sb_appendf(&pb, "%s session '%s'.", pinning ? "Pinned" : "Unpinned", rid);
            if (scf_truthy(title)) {
                char *ts = scf_py_str(title);
                scf_sb_appendf(&pb, "  (%s)", ts);
                free(ts);
            }
            json_free(title);
            char *text = scf_sb_take(&pb);
            scf_print(ctx, text);
            free(text);
            if (scf_exc_pending(ctx)) { json_free(resolved); return NULL; }
        } else {
            json_t *nf = scf_not_found(ctx, raw);
            if (scf_exc_pending(ctx)) { json_free(resolved); json_free(nf); return NULL; }
            failures += 1; /* _not_found returns 1 */
            json_free(nf);
        }
        json_free(resolved);
    }
    return failures ? json_number(1) : NULL;
}

/* PoP: scf_print_dry_run_preview @ hermes_cli/sessions_cmd.py:_print_dry_run_preview */
json_t *scf_print_dry_run_preview(scf_ctx *ctx, const json_t *candidates,
                                  const json_t *filters) {
    if (candidates && !scf_is_arr(candidates)) {
        scf_raise(ctx, "TypeError");
        return NULL;
    }
    size_t n = scf_is_arr(candidates) ? candidates->c.count : 0;
    /* describe_filters(filters) fires during the f-string eval, BEFORE print. */
    json_t *da = json_object();
    scf_arg_copy(da, "filters", filters);
    json_t *desc = scf_call(ctx, "describe_filters", da);
    if (scf_exc_pending(ctx)) { json_free(desc); return NULL; }
    scf_sb hb; scf_sb_init(&hb);
    scf_sb_appendf(&hb, "Would export %zu session(s) (%s).", n, scf_jstr(desc));
    json_free(desc);
    char *header = scf_sb_take(&hb);
    scf_print(ctx, header);
    free(header);
    if (scf_exc_pending(ctx)) return NULL;
    size_t shown = n < 100 ? n : 100;
    for (size_t i = 0; i < shown; i++) {
        json_t *row = candidates->c.items[i];
        json_t *sid = scf_attr(ctx, row, "session_id");
        if (scf_exc_pending(ctx)) return NULL;
        json_t *src = scf_attr(ctx, row, "source");
        if (scf_exc_pending(ctx)) return NULL;
        char *sids = scf_py_str(sid);
        char *srcs = scf_truthy(src) ? scf_py_str(src) : scf_strdup("");
        scf_sb rb; scf_sb_init(&rb);
        scf_sb_appendf(&rb, "  %s  %s", sids, srcs);
        free(sids);
        free(srcs);
        char *text = scf_sb_take(&rb);
        scf_print(ctx, text);
        free(text);
        if (scf_exc_pending(ctx)) return NULL;
    }
    if (n > 100) {
        json_t *ta = json_object();
        json_set(ta, "more", json_number((double)(n - 100)));
        json_t *r = scf_call(ctx, "print_truncated", ta);
        json_free(r);
        if (scf_exc_pending(ctx)) return NULL;
    }
    return NULL;
}

/* PoP: scf_note_pinned_skipped @ hermes_cli/sessions_cmd.py:_note_pinned_skipped */
json_t *scf_note_pinned_skipped(scf_ctx *ctx, const json_t *filters, const char *action) {
    json_t *pa = scf_prune_args(ctx, filters);
    if (scf_exc_pending(ctx)) { json_free(pa); return NULL; }
    json_t *bounds = json_get(pa, 0);
    json_t *criteria = json_get(pa, 1);
    json_t *ca = json_object();
    if (scf_is_obj(bounds)) {
        for (size_t i = 0; i < bounds->c.count; i++)
            scf_arg_copy(ca, bounds->c.keys[i], bounds->c.items[i]);
    }
    json_set(ca, "criteria", json_copy(criteria));
    json_set(ca, "pinned_only", json_bool(true));
    json_free(pa);
    json_t *cnt = scf_call(ctx, "count_prune_matches", ca);
    if (scf_exc_pending(ctx)) { json_free(cnt); return NULL; }
    long long skipped = scf_py_int(ctx, cnt);
    json_free(cnt);
    if (scf_exc_pending(ctx)) return NULL;
    if (!skipped) return NULL;
    const char *suffix = skipped == 1 ? "" : "s";
    const char *verb, *optin;
    if (scf_str_eq(action, "prune")) {
        verb = "deleted";
        optin = "Pass --include-pinned to delete them anyway, or unpin first with "
                "`hermes sessions unpin <id>`.";
    } else {
        verb = "archived";
        optin = "Unpin first with `hermes sessions unpin <id>` to include them.";
    }
    scf_sb nb; scf_sb_init(&nb);
    scf_sb_appendf(&nb, "Note: %lld pinned session%s also match these filters but will NOT "
                   "be %s (pin is a keep flag). %s", skipped, suffix, verb, optin);
    char *text = scf_sb_take(&nb);
    scf_print(ctx, text);
    free(text);
    if (scf_exc_pending(ctx)) return NULL;
    return NULL;
}

/* PoP: scf_output_file_in_dir @ hermes_cli/sessions_cmd.py:_output_file_in_dir */
json_t *scf_output_file_in_dir(scf_ctx *ctx, const json_t *output, const char *default_name) {
    if (!scf_truthy(output)) return output ? json_copy(output) : NULL;
    if (output->type != JSON_STRING) {
        scf_raise(ctx, "AttributeError");
        return NULL;
    }
    const char *o = output->str_val;
    if (scf_str_eq(o, "-")) return json_copy(output);
    /* endswith(("/", os.sep)) with os.sep == "/" (posixpath double). */
    bool trailing = scf_ends_with(o, "/");
    bool isdir = false;
    if (!trailing) {
        json_t *ia = json_object();
        scf_arg_str(ia, "path", o);
        json_t *d = scf_call(ctx, "isdir", ia);
        if (scf_exc_pending(ctx)) { json_free(d); return NULL; }
        isdir = scf_truthy(d);
        json_free(d);
    }
    if (trailing || isdir) {
        json_t *ma = json_object();
        scf_arg_str(ma, "path", o);
        json_set(ma, "exist_ok", json_bool(true));
        json_t *m = scf_call(ctx, "makedirs", ma);
        json_free(m);
        if (scf_exc_pending(ctx)) return NULL;
        char *j = scf_os_path_join(o, default_name);
        json_t *ret = json_string(j);
        free(j);
        return ret;
    }
    return json_copy(output);
}

/* PoP: scf_write_output @ hermes_cli/sessions_cmd.py:_write_output */
json_t *scf_write_output(scf_ctx *ctx, const json_t *output, const char *text,
                         const char *summary) {
    bool to_stdout = !scf_truthy(output)
        || (output->type == JSON_STRING && scf_str_eq(output->str_val, "-"));
    if (to_stdout) {
        json_t *wa = json_object();
        scf_arg_str(wa, "text", text);
        json_t *w = scf_call(ctx, "stdout_write", wa);
        json_free(w);
        return NULL;
    }
    /* open(output, "w", encoding="utf-8") + f.write(text): one effect seam. */
    json_t *fa = json_object();
    scf_arg_str(fa, "path", scf_jstr(output));
    scf_arg_str(fa, "text", text);
    json_t *w = scf_call(ctx, "file_write", fa);
    json_free(w);
    if (scf_exc_pending(ctx)) return NULL;
    scf_print(ctx, summary ? summary : "");
    return NULL;
}

/* PoP: scf_render_only @ hermes_cli/sessions_cmd.py:_render_only */
json_t *scf_render_only(scf_ctx *ctx, const json_t *args, const json_t *sessions) {
    json_t *fmt_node = scf_attr(ctx, args, "format");
    if (scf_exc_pending(ctx)) return NULL;
    json_t *only_node = scf_attr(ctx, args, "only");
    if (scf_exc_pending(ctx)) return NULL;
    const char *fmt = (fmt_node && fmt_node->type == JSON_STRING
                       && scf_str_eq(fmt_node->str_val, "md")) ? "markdown" : "jsonl";
    json_t *ra = json_object();
    scf_arg_copy(ra, "sessions", sessions);
    scf_arg_str(ra, "fmt", fmt);
    scf_arg_copy(ra, "only", only_node);
    json_t *rendered = scf_call(ctx, "render_sessions_export", ra);
    if (scf_exc_pending(ctx)) { json_free(rendered); return NULL; }
    json_t *ca = json_object();
    scf_arg_copy(ca, "sessions", sessions);
    scf_arg_copy(ca, "only", only_node);
    json_t *cnt = scf_call(ctx, "export_record_count", ca);
    if (scf_exc_pending(ctx)) { json_free(rendered); json_free(cnt); return NULL; }
    /* count, noun = export_record_count(...) — a 2-sequence unpack. */
    if (!scf_is_arr(cnt) || cnt->c.count != 2) {
        scf_raise(ctx, scf_is_none(cnt) || !scf_is_arr(cnt) ? "TypeError" : "ValueError");
        json_free(rendered);
        json_free(cnt);
        return NULL;
    }
    json_t *count = cnt->c.items[0];
    json_t *noun = cnt->c.items[1];
    bool one = (count && count->type == JSON_NUMBER && count->num_val == 1.0)
        || (count && count->type == JSON_BOOL && count->bool_val);
    json_t *out_node = scf_attr(ctx, args, "output");
    if (scf_exc_pending(ctx)) { json_free(rendered); json_free(cnt); return NULL; }
    char *cs = scf_py_str(count);
    char *os = scf_py_str(out_node);
    scf_sb sb; scf_sb_init(&sb);
    scf_sb_appendf(&sb, "Exported %s %s%s to %s", cs, scf_jstr(noun), one ? "" : "s", os);
    free(cs);
    free(os);
    json_free(cnt);
    json_t *ret = json_array();
    json_append(ret, rendered);
    json_append(ret, scf_sb_json(&sb));
    return ret;
}

/* PoP: scf_render_html @ hermes_cli/sessions_cmd.py:_render_html */
json_t *scf_render_html(scf_ctx *ctx, const json_t *args, const json_t *sessions) {
    if (!scf_is_arr(sessions)) {
        scf_raise(ctx, "TypeError");
        return NULL;
    }
    size_t n = sessions->c.count;
    bool single = n == 1;
    json_t *content;
    if (single) {
        json_t *ha = json_object();
        scf_arg_copy(ha, "session", sessions->c.items[0]);
        content = scf_call(ctx, "generate_html_export", ha);
    } else {
        json_t *ha = json_object();
        scf_arg_copy(ha, "sessions", sessions);
        content = scf_call(ctx, "generate_multi_session_html_export", ha);
    }
    if (scf_exc_pending(ctx)) { json_free(content); return NULL; }
    json_t *out_node = scf_attr(ctx, args, "output");
    if (scf_exc_pending(ctx)) { json_free(content); return NULL; }
    char *os = scf_py_str(out_node);
    scf_sb sb; scf_sb_init(&sb);
    scf_sb_appendf(&sb, "Exported %zu %s to %s (HTML)", n,
                   single ? "session" : "sessions", os);
    free(os);
    json_t *ret = json_array();
    json_append(ret, content);
    json_append(ret, scf_sb_json(&sb));
    return ret;
}

/* PoP: scf_export_flat @ hermes_cli/sessions_cmd.py:_export_flat */
json_t *scf_export_flat(scf_ctx *ctx, const char *kind, json_t *args) {
    /* _FLAT_EXPORTERS[kind] — unknown kind raises KeyError. */
    int ki;
    if (scf_str_eq(kind, "only")) ki = 0;
    else if (scf_str_eq(kind, "html")) ki = 1;
    else if (scf_str_eq(kind, "jsonl")) ki = 2;
    else { scf_raise(ctx, "KeyError"); return NULL; }
    /* unusable(args) gate — each lambda reads ONLY its own attribute. */
    bool unusable = false;
    const char *message = "";
    if (ki == 0) {
        json_t *fmt = scf_attr(ctx, args, "format");
        if (scf_exc_pending(ctx)) return NULL;
        bool ok_fmt = fmt && fmt->type == JSON_STRING
            && (scf_str_eq(fmt->str_val, "jsonl") || scf_str_eq(fmt->str_val, "md"));
        unusable = !ok_fmt;
        message = "--only user-prompts supports --format jsonl or md.";
    } else {
        json_t *out = scf_attr(ctx, args, "output");
        if (scf_exc_pending(ctx)) return NULL;
        if (ki == 1) {
            unusable = !scf_truthy(out)
                || (out->type == JSON_STRING && scf_str_eq(out->str_val, "-"));
            message = "HTML export requires an output file path.";
        } else {
            unusable = !scf_truthy(out);
            message = "JSONL export requires an output path (use - for stdout).";
        }
    }
    if (unusable) {
        scf_print(ctx, message);
        if (scf_exc_pending(ctx)) return NULL;
        return json_number(1);
    }
    json_t *sessions = scf_call(ctx, "collect", NULL);
    if (scf_exc_pending(ctx)) { json_free(sessions); return NULL; }
    /* isinstance(sessions, int) — bool is an int subclass too; a non-integral
     * JSON number models a float, which isinstance rejects (falls through to
     * the len() TypeError). Integral-float identity is out of fixture domain. */
    if (sessions && (sessions->type == JSON_BOOL
                     || (sessions->type == JSON_NUMBER
                         && sessions->num_val == (double)(long long)sessions->num_val)))
        return sessions;
    if (scf_is_none(sessions)) { json_free(sessions); return NULL; }
    if (!scf_is_arr(sessions)) {
        scf_raise(ctx, "TypeError");
        json_free(sessions);
        return NULL;
    }
    size_t n = sessions->c.count;
    char *name = NULL;
    if (n == 1) {
        json_t *s0 = sessions->c.items[0];
        if (!scf_is_obj(s0)) {
            scf_raise(ctx, "AttributeError");
            json_free(sessions);
            return NULL;
        }
        json_t *idv = json_obj_get(s0, "id"); /* .get("id", "") */
        json_t *fmt = scf_attr(ctx, args, "format");
        if (scf_exc_pending(ctx)) { json_free(sessions); return NULL; }
        json_t *fa = json_object();
        scf_arg_json(fa, "session_id", idv ? json_copy(idv) : json_string(""));
        scf_arg_copy(fa, "fmt", fmt);
        json_t *dn = scf_call(ctx, "default_save_filename", fa);
        if (scf_exc_pending(ctx)) { json_free(sessions); json_free(dn); return NULL; }
        name = scf_strdup(scf_jstr(dn));
        json_free(dn);
    } else {
        json_t *fmt = scf_attr(ctx, args, "format");
        if (scf_exc_pending(ctx)) { json_free(sessions); return NULL; }
        scf_sb nb; scf_sb_init(&nb);
        scf_sb_appendf(&nb, "hermes_sessions.%s", scf_jstr(fmt));
        name = scf_sb_take(&nb);
    }
    /* args.output = _output_file_in_dir(args.output, name) */
    json_t *outv = scf_attr(ctx, args, "output");
    if (scf_exc_pending(ctx)) { free(name); json_free(sessions); return NULL; }
    json_t *newout = scf_output_file_in_dir(ctx, outv, name);
    free(name);
    if (scf_exc_pending(ctx)) { json_free(sessions); json_free(newout); return NULL; }
    json_set(args, "output", newout ? newout : json_null());
    /* _write_output(args.output, *render(args, sessions)) — args.output is
     * evaluated first, then the renderer fires, then the write. */
    json_t *outarg = json_copy(json_obj_get(args, "output"));
    json_t *rendered = NULL;
    if (ki == 2) {
        json_t *ra = json_object();
        json_set(ra, "args", json_copy(args));
        scf_arg_copy(ra, "sessions", sessions);
        rendered = scf_call(ctx, "render_jsonl", ra);
    } else if (ki == 0) {
        rendered = scf_render_only(ctx, args, sessions);
    } else {
        rendered = scf_render_html(ctx, args, sessions);
    }
    if (scf_exc_pending(ctx)) {
        json_free(sessions); json_free(outarg); json_free(rendered);
        return NULL;
    }
    if (!scf_is_arr(rendered) || rendered->c.count != 2) {
        /* *render(...) into a call: any bad unpack is a TypeError. */
        scf_raise(ctx, "TypeError");
        json_free(sessions); json_free(outarg); json_free(rendered);
        return NULL;
    }
    json_t *w = scf_write_output(ctx, outarg, scf_jstr(rendered->c.items[0]),
                                 scf_jstr(rendered->c.items[1]));
    json_free(w);
    json_free(sessions);
    json_free(outarg);
    json_free(rendered);
    return NULL;
}

/* PoP: scf_print_recovery_verdict @ hermes_cli/sessions_cmd.py:_print_recovery_verdict */
json_t *scf_print_recovery_verdict(scf_ctx *ctx, const json_t *report,
                                   const json_t *output, bool allow_partial) {
    if (!scf_is_obj(report)) {
        scf_raise(ctx, "AttributeError");
        return NULL;
    }
    char *os = scf_py_str(output);
    if (scf_truthy(json_obj_get(report, "complete"))) {
        scf_sb cb; scf_sb_init(&cb);
        scf_sb_appendf(&cb, "\xe2\x9c\x93 Recovered database verified at: %s\n"
                       "  The active session database was not changed.\n"
                       "  Review the JSON report before installing this database.", os);
        free(os);
        char *text = scf_sb_take(&cb);
        scf_print(ctx, text);
        free(text);
        if (scf_exc_pending(ctx)) return NULL;
        return json_number(0);
    }
    if (allow_partial && scf_truthy(json_obj_get(report, "verified"))) {
        /* counts = report.get("verification", {}).get("table_counts", {}) —
         * a present null (or non-dict) raises AttributeError on .get. */
        json_t *counts = NULL;
        if (json_has(report, "verification")) {
            json_t *ver = json_obj_get(report, "verification");
            if (!scf_is_obj(ver)) {
                scf_raise(ctx, "AttributeError");
                free(os);
                return NULL;
            }
            if (json_has(ver, "table_counts")) {
                json_t *tc = json_obj_get(ver, "table_counts");
                if (!scf_is_obj(tc)) {
                    scf_raise(ctx, "AttributeError");
                    free(os);
                    return NULL;
                }
                counts = tc;
            }
        }
        scf_sb vb; scf_sb_init(&vb);
        if (scf_truthy(json_obj_get(report, "best_effort"))) {
            scf_sb_appendf(&vb, "\xe2\x9c\x93 BEST-EFFORT page-level salvage verified at: %s\n"
                           "  The source table schemas were unreadable; rows were rebuilt "
                           "from raw pages via sqlite3 .recover and mapped heuristically.", os);
        } else {
            scf_sb_appendf(&vb, "\xe2\x9c\x93 Partial recovery output verified at: %s", os);
        }
        free(os);
        char *vtext = scf_sb_take(&vb);
        scf_print(ctx, vtext);
        free(vtext);
        if (scf_exc_pending(ctx)) return NULL;
        long long sn = scf_int_or_zero(ctx, counts ? json_obj_get(counts, "sessions") : NULL);
        if (scf_exc_pending(ctx)) return NULL;
        long long mn = scf_int_or_zero(ctx, counts ? json_obj_get(counts, "messages") : NULL);
        if (scf_exc_pending(ctx)) return NULL;
        char *sc = scf_commas(sn);
        char *mc = scf_commas(mn);
        scf_sb nb; scf_sb_init(&nb);
        scf_sb_appendf(&nb, "  Recovered %s sessions and %s messages.\n"
                       "  The active session database was not changed.\n"
                       "  This output is incomplete. Review every skipped range and orphan "
                       "count in the JSON report before installing it.", sc, mc);
        free(sc);
        free(mc);
        char *ntext = scf_sb_take(&nb);
        scf_print(ctx, ntext);
        free(ntext);
        if (scf_exc_pending(ctx)) return NULL;
        return json_number(0);
    }
    free(os);
    scf_print(ctx, "\xe2\x9c\x97 Recovery output did not pass every verification check.\n"
              "  Do not install it. Review the JSON report for partial data or errors.");
    if (scf_exc_pending(ctx)) return NULL;
    return json_number(1);
}

/* PoP: scf_repair_prompts_pin_names @ hermes_cli/sessions_cmd.py:_repair_prompts_pin_names */
json_t *scf_repair_prompts_pin_names(scf_ctx *ctx, const json_t *record) {
    json_t *pin = scf_attr(ctx, record, "tool_pin");
    if (scf_exc_pending(ctx)) return NULL;
    json_t *tools = scf_is_obj(pin) ? json_obj_get(pin, "tools") : pin;
    if (!scf_is_arr(tools)) return NULL;
    json_t *names = json_array();
    for (size_t i = 0; i < tools->c.count; i++) {
        json_t *item = tools->c.items[i];
        json_t *name = NULL;
        if (item && item->type == JSON_STRING) {
            name = item;
        } else if (scf_is_obj(item)) {
            json_t *function = json_obj_get(item, "function");
            name = scf_is_obj(function) ? json_obj_get(function, "name") : NULL;
        } else {
            json_free(names);
            return NULL;
        }
        if (!name || name->type != JSON_STRING || !name->str_val || !name->str_val[0]) {
            json_free(names);
            return NULL;
        }
        json_append(names, json_copy(name));
    }
    return names;
}
