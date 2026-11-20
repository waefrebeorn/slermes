/* skills_hub_flow2.c — FLOW + leaves port of the UNCLAIMED remainder of
 * hermes_cli/skills_hub.py (the claimed siblings live in
 * hermes_cli/commands.c / partial_compress.c / hermes_cli/skills_hub.c and are
 * SEAMED or transcribed elsewhere; this wave ports the small shared helpers,
 * identifier/source resolution helpers and slash-argument parsing helpers).
 *
 * Ported targets (all UNCLAIMED by a strict PoP grep at authoring time):
 *   _trust_cell, _truncate, _ident_col, _row, _slug_key, _identifier_slug,
 *   _exact_name_hits, _hub_rate_limited, _hub_rejected_credentials,
 *   _upstream_repo, _opt_value, _int_or, _opt_int, _first_positional.
 * Internal composition is REAL: _exact_name_hits -> _slug_key/_identifier_slug,
 * _opt_int -> _opt_value + _int_or.  The oracle AST-extracts the same defs into
 * one namespace so the composition is exercised on both sides.
 *
 * Representation contract (shared with tests/oracle_skills_hub_flow2.py):
 *   - tuple -> array; dict keeps insertion order; None -> JSON null.
 *   - integral floats collapse to ints (libjson prints %.0f, the oracle norm()
 *     folds integral floats to int).
 *   - object doubles (source rows / bundles / auth / metadata / github) are
 *     plain JSON objects read with json_obj_get; fields named by direct
 *     attribute read (r.name, r.identifier, bundle.source) are present in every
 *     fixture (documented domain; an absent one is not exercised).
 *   - a full builtin exception hierarchy (models_flow2 shape) is carried for
 *     the except paths; sh2_int_or raises+catches ValueError internally.
 *   - fixture domain: text operands are ASCII (byte index == codepoint index
 *     for _truncate); _truncate widths are integers; _int_or text is a string
 *     of ASCII digits with optional sign and ASCII whitespace; _opt_value /
 *     _first_positional args are strings; .strip() operands are ASCII.
 *
 * libjson-only. Self-contained: the harness #includes "cli/skills_hub_flow2.c".
 * "sh2_" prefix (the flow1 cli helpers already own cpy_/sh_/others).
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

/* ── context + small helpers ─────────────────────────────────────────────── */

typedef struct sh2_ctx {
    json_t *io;        /* borrowed: seam scripts (none used in this wave) */
    json_t *env;       /* borrowed: scripted constants (none used) */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} sh2_ctx;

static bool sh2_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool sh2_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool sh2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool sh2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool sh2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool sh2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool sh2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool sh2_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:  return false;
        case JSON_BOOL:  return v->bool_val;
        case JSON_NUMBER:return v->num_val != 0.0;
        case JSON_STRING:return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT:return v->c.count > 0;
        default:         return false;
    }
}
static const char *sh2_jstr(const json_t *v) {
    return (sh2_is_str(v) && v->str_val) ? v->str_val : "";
}
static double sh2_num(const json_t *v) { return sh2_is_num(v) ? v->num_val : 0.0; }
static json_t *sh2_get(const json_t *obj, const char *key) {
    return sh2_is_obj(obj) ? json_obj_get(obj, key) : NULL;
}

/* ── exception plumbing (FULL builtin hierarchy) ────────────────────────── */

static bool sh2_exc_pending(const sh2_ctx *ctx) { return ctx && ctx->exc != NULL; }
static void sh2_exc_clear(sh2_ctx *ctx) {
    if (ctx && ctx->exc) { json_free(ctx->exc); ctx->exc = NULL; }
}
static void sh2_raise(sh2_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg ? msg : ""));
}
static const char *sh2_exc_class(const sh2_ctx *ctx) {
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *sh2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"FileNotFoundError", "OSError"}, {"PermissionError", "OSError"},
        {"IsADirectoryError", "OSError"}, {"NotADirectoryError", "OSError"},
        {"FileExistsError", "OSError"}, {"InterruptedError", "OSError"},
        {"BlockingIOError", "OSError"}, {"ChildProcessError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"TimeoutError", "OSError"},
        {"JSONDecodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeError", "ValueError"},
        {"ZeroDivisionError", "ArithmeticError"}, {"OverflowError", "ArithmeticError"},
        {"IndexError", "LookupError"}, {"KeyError", "LookupError"},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"Exception", "BaseException"},
        {"RuntimeError", "Exception"}, {"NotImplementedError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"ValueError", "Exception"}, {"TypeError", "Exception"},
        {"AttributeError", "Exception"}, {"NameError", "Exception"},
        {"UnboundLocalError", "NameError"}, {"LookupError", "Exception"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OSError", "Exception"}, {"StopIteration", "Exception"},
        {"StopAsyncIteration", "Exception"}, {"AssertionError", "Exception"},
        {"SyntaxError", "Exception"}, {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"}, {"SystemError", "Exception"},
        {"ReferenceError", "Exception"}, {"MemoryError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (sh2_str_eq(T[i].c, c)) return T[i].p;
    return NULL;
}
static bool sh2_exc_matches(const sh2_ctx *ctx, const char *const *classes, size_t n) {
    if (!sh2_exc_pending(ctx)) return false;
    const char *cls = sh2_exc_class(ctx);
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < n; i++)
            if (sh2_str_eq(cls, classes[i])) return true;
        const char *p = sh2_exc_parent(cls);
        if (!p) return false;
        cls = p;
    }
    return false;
}
static bool sh2_exc_is_exception(const sh2_ctx *ctx) {
    static const char *E[] = {"Exception"};
    return sh2_exc_matches(ctx, E, 1);
}
static bool sh2_exc_is_value_error(const sh2_ctx *ctx) {
    static const char *E[] = {"ValueError"};
    return sh2_exc_matches(ctx, E, 1);
}

/* ── universal seam (kept for parity; unused by this wave) ──────────────── */

static json_t *sh2_script_result(sh2_ctx *ctx, const json_t *script) {
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
/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *sh2_call(sh2_ctx *ctx, const char *name, json_t *args) {
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
                snprintf(msg, sizeof msg, "script exhausted: %s", name);
                json_set(ctx->exc, "msg", json_string(msg));
                chosen = ctx->exc;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        }
    }
    if (chosen != ctx->exc) ret = sh2_script_result(ctx, chosen);
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

/* ═════════════════════ ported defs ════════════════════════════════════════ */

/* _TRUST_STYLE = {"builtin":"bright_cyan","trusted":"green",
 *                  "community":"yellow","local":"dim"}  (default "dim") */
static const char *sh2_trust_style(const char *tl) {
    if (sh2_str_eq(tl, "builtin"))   return "bright_cyan";
    if (sh2_str_eq(tl, "trusted"))   return "green";
    if (sh2_str_eq(tl, "community")) return "yellow";
    if (sh2_str_eq(tl, "local"))     return "dim";
    return "dim";
}
/* PoP: sh2_trust_cell @ hermes_cli/skills_hub.py:_trust_cell */
static json_t *sh2_trust_cell(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *tl  = sh2_jstr(sh2_get(in, "trust_level"));
    const char *src = sh2_jstr(sh2_get(in, "source"));
    json_t *offv = sh2_get(in, "official_label");
    const char *off = offv ? sh2_jstr(offv) : "official";
    const char *label = sh2_str_eq(src, "official") ? off : tl;
    char buf[1024];
    snprintf(buf, sizeof buf, "[%s]%s[/]", sh2_trust_style(tl), label);
    return json_string(buf);
}

/* text[:width] + ("..." if len(text) > width else "") — ASCII domain. */
/* PoP: sh2_truncate @ hermes_cli/skills_hub.py:_truncate */
static json_t *sh2_truncate(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *text = sh2_jstr(sh2_get(in, "text"));
    long w = (long)sh2_num(sh2_get(in, "width"));
    size_t len = strlen(text);
    size_t cut;
    if (w >= 0) cut = ((size_t)w < len) ? (size_t)w : len;
    else { long e = (long)len + w; cut = e > 0 ? (size_t)e : 0; }
    bool more = ((long)len > w);
    char buf[8192];
    snprintf(buf, sizeof buf, "%.*s%s", (int)cut, text, more ? "..." : "");
    return json_string(buf);
}

/* ("Identifier", {"style":style,"overflow":"fold","no_wrap":False}) */
/* PoP: sh2_ident_col @ hermes_cli/skills_hub.py:_ident_col */
static json_t *sh2_ident_col(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    const char *style = sh2_jstr(sh2_get(in, "style"));
    json_t *r = json_array();
    json_append(r, json_string("Identifier"));
    json_t *d = json_object();
    json_set(d, "style", json_string(style));
    json_set(d, "overflow", json_string("fold"));
    json_set(d, "no_wrap", json_bool(false));
    json_append(r, d);
    return r;
}

/* {f: getattr(r, f) for f in fields} — all fields present in fixture domain. */
/* PoP: sh2_row @ hermes_cli/skills_hub.py:_row */
static json_t *sh2_row(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *r = sh2_get(in, "r");
    json_t *fields = sh2_get(in, "fields");
    json_t *out = json_object();
    if (!sh2_is_arr(fields)) return out;
    for (size_t i = 0; i < fields->c.count; i++) {
        const char *f = sh2_jstr(fields->c.items[i]);
        json_t *v = sh2_get(r, f);
        json_set(out, f, v ? json_copy(v) : json_null());
    }
    return out;
}

/* re.sub(r"[^a-z0-9]+", "", str(text or "").lower()) — ASCII fold. */
static json_t *sh2_slug_key_impl(const char *s) {
    char buf[8192];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n + 1 < sizeof buf; p++) {
        unsigned char c = (unsigned char)tolower(*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) buf[n++] = (char)c;
    }
    buf[n] = '\0';
    return json_string(buf);
}
/* PoP: sh2_slug_key @ hermes_cli/skills_hub.py:_slug_key */
static json_t *sh2_slug_key(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    return sh2_slug_key_impl(sh2_jstr(sh2_get(in, "text")));
}

/* str(identifier or "").split("/")[-1] */
static const char *sh2_identifier_slug_impl(const char *s) {
    const char *last = NULL;
    for (const char *p = s; *p; p++) if (*p == '/') last = p;
    return last ? last + 1 : s;
}
/* PoP: sh2_identifier_slug @ hermes_cli/skills_hub.py:_identifier_slug */
static json_t *sh2_identifier_slug(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    return json_string(sh2_identifier_slug_impl(sh2_jstr(sh2_get(in, "identifier"))));
}

/* ASCII str.strip() -> newly allocated, trimmed string. */
static void sh2_ascii_strip(const char *s, char *out, size_t outsz) {
    while (*s && (unsigned char)*s <= 0x20) s++; /* ASCII ws incl \x1c-\x1f */
    size_t n = strlen(s);
    while (n > 0 && (unsigned char)s[n - 1] <= 0x20) n--;
    size_t k = 0;
    for (size_t i = 0; i < n && k + 1 < outsz; i++) out[k++] = s[i];
    out[k] = '\0';
}

/* _exact_name_hits(results, name): exact (stripped/lowered) name first, then
 * separator-insensitive slug of display name or identifier trailing slug. */
/* PoP: sh2_exact_name_hits @ hermes_cli/skills_hub.py:_exact_name_hits */
static json_t *sh2_exact_name_hits(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *results = sh2_get(in, "results");
    char qbuf[8192];
    sh2_ascii_strip(sh2_jstr(sh2_get(in, "name")), qbuf, sizeof qbuf);
    for (char *p = qbuf; *p; p++) *p = (char)tolower((unsigned char)*p);
    json_t *hits = json_array();
    if (!sh2_is_arr(results)) return hits;
    /* pass 1: exact display name */
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *r = results->c.items[i];
        char nbuf[8192];
        sh2_ascii_strip(sh2_jstr(sh2_get(r, "name")), nbuf, sizeof nbuf);
        for (char *p = nbuf; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (sh2_str_eq(nbuf, qbuf)) json_append(hits, json_copy(r));
    }
    if (hits->c.count > 0) return hits;
    /* pass 2: slug key membership */
    json_t *key_node = sh2_slug_key_impl(qbuf);
    const char *key = key_node->str_val;
    if (key[0] == '\0') { json_free(key_node); return hits; }
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *r = results->c.items[i];
        json_t *kn = sh2_slug_key_impl(sh2_jstr(sh2_get(r, "name")));
        json_t *sn = sh2_slug_key_impl(sh2_identifier_slug_impl(sh2_jstr(sh2_get(r, "identifier"))));
        if (sh2_str_eq(key, kn->str_val) || sh2_str_eq(key, sn->str_val))
            json_append(hits, json_copy(r));
        json_free(kn);
        json_free(sn);
    }
    json_free(key_node);
    return hits;
}

/* any(getattr(src,"is_rate_limited",False) or
 *     getattr(getattr(src,"github",None),"is_rate_limited",False) for src in sources) */
/* PoP: sh2_hub_rate_limited @ hermes_cli/skills_hub.py:_hub_rate_limited */
static json_t *sh2_hub_rate_limited(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *sources = sh2_get(in, "sources");
    bool any = false;
    if (sh2_is_arr(sources)) {
        for (size_t i = 0; i < sources->c.count; i++) {
            json_t *src = sources->c.items[i];
            bool a = sh2_truthy(sh2_get(src, "is_rate_limited"));
            bool b = false;
            json_t *gh = sh2_get(src, "github");
            if (sh2_is_obj(gh)) b = sh2_truthy(sh2_get(gh, "is_rate_limited"));
            if (a || b) { any = true; break; }
        }
    }
    return json_bool(any);
}

/* next((r for src in sources
 *   if isinstance(r := getattr(getattr(src,"auth",None),"rejected",None), list) and r), []) */
/* PoP: sh2_hub_rejected_credentials @ hermes_cli/skills_hub.py:_hub_rejected_credentials */
static json_t *sh2_hub_rejected_credentials(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *sources = sh2_get(in, "sources");
    if (sh2_is_arr(sources)) {
        for (size_t i = 0; i < sources->c.count; i++) {
            json_t *auth = sh2_get(sources->c.items[i], "auth");
            json_t *r = sh2_get(auth, "rejected");
            if (sh2_is_arr(r) && r->c.count > 0) return json_copy(r);
        }
    }
    return json_array();
}

/* (getattr(bundle,"metadata",None) or {}).get("upstream_repo")
 *   if bundle.source == "official" else None */
/* PoP: sh2_upstream_repo @ hermes_cli/skills_hub.py:_upstream_repo */
static json_t *sh2_upstream_repo(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *bundle = sh2_get(in, "bundle");
    if (!sh2_str_eq(sh2_jstr(sh2_get(bundle, "source")), "official")) return json_null();
    json_t *meta = sh2_get(bundle, "metadata");
    if (!sh2_is_obj(meta)) return json_null();
    json_t *v = json_obj_get(meta, "upstream_repo");
    return v ? json_copy(v) : json_null();
}

/* hits = [args[i+1] for i,a in enumerate(args) if a==flag and i+1<len(args)]
 * return (hits[-1] if last else hits[0]) if hits else default */
static json_t *sh2_opt_value_impl(const json_t *args, const char *flag,
                                  const char *def, bool last) {
    const char *first = NULL, *found = NULL;
    if (sh2_is_arr(args)) {
        for (size_t i = 0; i < args->c.count; i++) {
            if (i + 1 < args->c.count &&
                sh2_str_eq(sh2_jstr(args->c.items[i]), flag)) {
                const char *v = sh2_jstr(args->c.items[i + 1]);
                if (!first) first = v;
                found = v;
            }
        }
    }
    if (!first) return json_string(def ? def : "");
    return json_string(last ? found : first);
}
/* PoP: sh2_opt_value @ hermes_cli/skills_hub.py:_opt_value */
static json_t *sh2_opt_value(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *dv = sh2_get(in, "default");
    return sh2_opt_value_impl(sh2_get(in, "args"),
                              sh2_jstr(sh2_get(in, "flag")),
                              dv ? sh2_jstr(dv) : "",
                              sh2_truthy(sh2_get(in, "last")));
}

/* int(text) or default on ValueError — strict ASCII integer grammar. */
static bool sh2_int_parse(const char *s, long *out) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' ||
                  *s == '\r' || *s == '\v' || *s == '\f')) s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (*s < '0' || *s > '9') return false;
    long v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' ||
                  *s == '\r' || *s == '\v' || *s == '\f')) s++;
    if (*s != '\0') return false;
    *out = sign * v;
    return true;
}
static json_t *sh2_int_or_impl(sh2_ctx *ctx, const char *text, long def) {
    long v;
    if (sh2_int_parse(text, &v)) return json_number((double)v);
    sh2_raise(ctx, "ValueError", "invalid literal for int()");
    if (sh2_exc_is_value_error(ctx)) { sh2_exc_clear(ctx); return json_number((double)def); }
    return json_null(); /* unreachable */
}
/* PoP: sh2_int_or @ hermes_cli/skills_hub.py:_int_or */
static json_t *sh2_int_or(sh2_ctx *ctx, json_t *in) {
    long def = (long)sh2_num(sh2_get(in, "default"));
    return sh2_int_or_impl(ctx, sh2_jstr(sh2_get(in, "text")), def);
}

/* _opt_int = _int_or(_opt_value(args, flag, str(default), last=True), default) */
/* PoP: sh2_opt_int @ hermes_cli/skills_hub.py:_opt_int */
static json_t *sh2_opt_int(sh2_ctx *ctx, json_t *in) {
    long def = (long)sh2_num(sh2_get(in, "default"));
    char dbuf[64];
    snprintf(dbuf, sizeof dbuf, "%ld", def);
    json_t *v = sh2_opt_value_impl(sh2_get(in, "args"), sh2_jstr(sh2_get(in, "flag")), dbuf, true);
    json_t *r = sh2_int_or_impl(ctx, sh2_jstr(v), def);
    json_free(v);
    return r;
}

/* args[0] if args and not args[0].startswith("--") else None */
/* PoP: sh2_first_positional @ hermes_cli/skills_hub.py:_first_positional */
static json_t *sh2_first_positional(sh2_ctx *ctx, json_t *in) {
    (void)ctx;
    json_t *args = sh2_get(in, "args");
    if (!sh2_is_arr(args) || args->c.count == 0) return json_null();
    json_t *a0 = args->c.items[0];
    const char *s = sh2_jstr(a0);
    if (strncmp(s, "--", 2) == 0) return json_null();
    return json_copy(a0);
}
