/*
 * slack_adapter_flow2.c -- FLOW cores, wave 2, of
 * plugins/platforms/slack/adapter.py (hermes-agent): the Slack plugin
 * adapter's REMAINING unclaimed decision chains after slack_adapter_flow
 * (28 claimed leaves): error-shape classifiers (SlackApiError response
 * codes, transient aiohttp transport vs permanent TLS), Block-Kit table
 * cell collection / table projection / rich-text extraction / URL recovery,
 * proxy application, timestamp sort keys, bounded-cache eviction
 * (oldest-dict / oldest-ts), workspace metadata + id scoping, config
 * truthiness, native task-card gating + chunk coercion, stream-relation
 * classification, lazy attributes, channel-team memory, slash-context
 * pop/purge (ContextVar + monotonic TTL), client resolution, ignored-channel
 * gating, API-error diagnostics and user-token warnings.
 *
 * Decision sequences only: every live I/O / injected callable / claimed
 * sibling (_int_or_zero, _first_truthy, _render_inline_elements,
 * _attachment_label, _render_slack_table_block when seamed,
 * _slack_ignored_channels, logger.warning, time.monotonic, factory,
 * ts_getter) enters through ONE universal seam: sa2_call(ctx, name, args).
 * The harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name","args","ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and sa2_-prefixed.
 *
 * Representation contract (shared with tests/oracle_slack_adapter_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object.
 * - Attribute carriers (non-dict objects) travel as {"__obj__": {attrs}};
 *   a plain JSON object IS a Python dict. Sets travel as {"__set__":
 *   [members]} (C keeps members sorted; the oracle canon sorts sets).
 *   Tuple INPUTS travel as {"__tuple__": [items]}; tuple RESULTS are plain
 *   JSON arrays (the oracle canon projects tuples to arrays).
 * - Exception doubles (args.exc) are {"__exc__": {type, message, ...attrs}};
 *   the oracle materializes REAL exception instances (builtin classes when
 *   the kind names one, per-case aiohttp double classes, else synthetic
 *   Exception subclasses). isinstance flows walk the FULL builtin exception
 *   hierarchy table below (generated from the quarry CPython 3.14.9) plus
 *   the per-case aiohttp class spec in state.aiohttp.
 * - state.slash_command_contexts is an ORDERED [{"key":[...],"value":{...}}]
 *   pair list (a dict with tuple keys live); state.slash_user_id is the
 *   _slash_user_id ContextVar value.
 * - logger seams record {"msg": <format literal>, "args": [<raw args>]} --
 *   %r/%s apply only at emission, args are recorded VERBATIM.
 * - None-valued seam ARG keys are dropped on both sides.
 * - Fixture corpus (ASCII scope): strip/lower/split fold ASCII whitespace
 *   (0x09-0x0d, 0x1c-0x1f, 0x20) only; str() of non-strings covers
 *   str/int/bool/None only (containers excluded); time.monotonic scripts
 *   carry NON-INTEGRAL floats (float identity matters for the subtraction
 *   TypeError messages); _discard_oldest_by_thread_ts carriers are dicts
 *   only (set iteration order is hash-randomized live, so ordered traces
 *   would be nondeterministic); dict-key flows use str keys/args only.
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) PASS THROUGH every except-Exception site.
 * - Fail-closed: a missing/exhausted seam script is a RuntimeError raise
 *   spec recorded in the trace before propagation.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SA2_TABLE_MAX_CHARS 20000
#define SA2_SLASH_CTX_TTL 120.0
#define SA2_CHANNEL_TEAM_MAX 10000

/* -- universal seam context -- */
typedef struct sa2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} sa2_ctx_t;

/* -- basic helpers -- */
static bool sa2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static bool sa2_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0 ||
                    strcmp(kind, "KeyboardInterrupt") == 0 ||
                    strcmp(kind, "GeneratorExit") == 0 ||
                    strcmp(kind, "CancelledError") == 0);
}

static bool sa2_raise_passes(const json_t *r) {
    return sa2_is_raise(r) && sa2_is_base_exc(json_get_str(r, "__raise__", ""));
}

static bool sa2_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__obj__") != NULL;
}

static json_t *sa2_obj_attrs(const json_t *v) {
    return sa2_is_obj(v) ? json_obj_get(v, "__obj__") : NULL;
}

static bool sa2_is_set(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__set__") != NULL;
}

static bool sa2_is_tuple_marker(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__tuple__") != NULL;
}

/* A plain Python dict carrier (not an attribute/set/tuple double). */
static bool sa2_is_dict(const json_t *v) {
    return v && v->type == JSON_OBJECT && !sa2_is_obj(v) &&
           !sa2_is_set(v) && !sa2_is_tuple_marker(v);
}

/* Python truthiness: None/False/0/""/empty containers are falsy; attribute
 * doubles (SimpleNamespace) are always truthy; set/tuple doubles key on
 * their member count. */
static bool sa2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count > 0;
        case JSON_OBJECT: {
            if (sa2_is_obj(v)) return true;
            const json_t *m = json_obj_get(v, "__set__");
            if (!m) m = json_obj_get(v, "__tuple__");
            if (m) return m->type == JSON_ARRAY && m->c.count > 0;
            return v->c.count > 0;
        }
        default: return false;
    }
}

static const char *sa2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static json_t *sa2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}

static json_t *sa2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    return sa2_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *sa2_call(sa2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set(ev, "name", json_string(name));
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = sa2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = sa2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = sa2_make_exc(raise);
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* -- string helpers (Python str semantics on the ASCII fixture corpus) -- */
static char *sa2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool sa2_is_ws(unsigned char c) {
    return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == ' ';
}

static char *sa2_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && sa2_is_ws((unsigned char)s[a])) a++;
    while (b > a && sa2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static size_t sa2_lstrip_offset(const char *s) {
    size_t a = 0;
    while (s[a] && sa2_is_ws((unsigned char)s[a])) a++;
    return a;
}

static char *sa2_rstrip(const char *s) {
    if (!s) s = "";
    size_t b = strlen(s);
    while (b > 0 && sa2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = malloc(b + 1);
    if (o) { memcpy(o, s, b); o[b] = '\0'; }
    return o;
}

/* str.strip(chars): remove any of the given chars from both ends. */
static char *sa2_strip_chars(const char *s, const char *chars) {
    if (!s) s = "";
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && strchr(chars, (unsigned char)s[a])) a++;
    while (b > a && strchr(chars, (unsigned char)s[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (o) { memcpy(o, s + a, b - a); o[b - a] = '\0'; }
    return o;
}

static char *sa2_lower(const char *s) {
    char *o = sa2_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* Python type name for error messages (JSON carriers). */
static const char *sa2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (floor(v->num_val) == v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT:
            if (sa2_is_obj(v)) return "types.SimpleNamespace";
            if (sa2_is_set(v)) return "set";
            if (sa2_is_tuple_marker(v)) return "tuple";
            return "dict";
        default: return "object";
    }
}

/* str() of a JSON value on the ASCII corpus: str/int/bool/None only
 * (containers and non-integral floats are excluded from str()-computed
 * positions by the fixture domain guards). */
static char *sa2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return sa2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return sa2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            if (floor(v->num_val) == v->num_val && fabs(v->num_val) < 1e18)
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
            else
                snprintf(buf, sizeof buf, "%.17g", v->num_val);
            return sa2_strdup(buf);
        }
        case JSON_STRING: return sa2_strdup(v->str_val ? v->str_val : "");
        default: return sa2_strdup("");  /* containers: outside the corpus */
    }
}

static json_t *sa2_attr_err(const json_t *v, const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'",
             sa2_type_name(v), attr);
    return sa2_err("AttributeError", msg);
}

/* self double is a SimpleNamespace: missing-attribute messages name it. */
static json_t *sa2_ns_attr_err(const char *attr) {
    char msg[256];
    snprintf(msg, sizeof msg,
             "'types.SimpleNamespace' object has no attribute '%s'", attr);
    return sa2_err("AttributeError", msg);
}

/* v[key] on a non-subscriptable / wrong-index-type carrier. idx_type is the
 * TYPE NAME of the index (the live messages name the index type, not the
 * key text); every ported call site indexes with the str literal "ts". */
static json_t *sa2_subscript_err(const json_t *v, const char *idx_type) {
    char msg[256];
    if (v && v->type == JSON_STRING) {
        snprintf(msg, sizeof msg, "string indices must be integers, not '%s'", idx_type);
        return sa2_err("TypeError", msg);
    }
    if (v && v->type == JSON_ARRAY) {
        snprintf(msg, sizeof msg, "list indices must be integers or slices, not %s", idx_type);
        return sa2_err("TypeError", msg);
    }
    snprintf(msg, sizeof msg, "'%s' object is not subscriptable", sa2_type_name(v));
    return sa2_err("TypeError", msg);
}

/* now - ts with a non-numeric operand (Python binary-op TypeError). */
static json_t *sa2_sub_type_err(const json_t *a, const json_t *b) {
    char msg[256];
    snprintf(msg, sizeof msg, "unsupported operand type(s) for -: '%s' and '%s'",
             sa2_type_name(a), sa2_type_name(b));
    return sa2_err("TypeError", msg);
}

/* -- small string buffer -- */
typedef struct sa2_sb { char *s; size_t n, cap; } sa2_sb_t;

static void sa2_sb_init(sa2_sb_t *b) { b->s = NULL; b->n = 0; b->cap = 0; }

static void sa2_sb_puts(sa2_sb_t *b, const char *s) {
    if (!s) s = "";
    size_t l = strlen(s);
    if (b->n + l + 1 > b->cap) {
        size_t nc = (b->cap ? b->cap * 2 : 64);
        while (nc < b->n + l + 1) nc *= 2;
        char *ns = realloc(b->s, nc);
        if (!ns) return;
        b->s = ns; b->cap = nc;
    }
    memcpy(b->s + b->n, s, l);
    b->n += l;
    b->s[b->n] = '\0';
}

static char *sa2_sb_take(sa2_sb_t *b) {
    if (!b->s) return sa2_strdup("");
    return b->s;
}

/* -- parts list (ordered strings) -- */
typedef struct sa2_parts { char **v; size_t n, cap; } sa2_parts_t;

static void sa2_parts_init(sa2_parts_t *p) { p->v = NULL; p->n = 0; p->cap = 0; }

static void sa2_parts_add(sa2_parts_t *p, char *s) {
    if (p->n == p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 8;
        char **nv = realloc(p->v, nc * sizeof(char *));
        if (!nv) { free(s); return; }
        p->v = nv; p->cap = nc;
    }
    p->v[p->n++] = s;
}

static void sa2_parts_free(sa2_parts_t *p) {
    for (size_t i = 0; i < p->n; i++) free(p->v[i]);
    free(p->v);
}

static char *sa2_join(sa2_parts_t *p, const char *sep, bool skip_empty) {
    sa2_sb_t b; sa2_sb_init(&b);
    bool first = true;
    for (size_t i = 0; i < p->n; i++) {
        if (skip_empty && (!p->v[i] || !p->v[i][0])) continue;
        if (!first) sa2_sb_puts(&b, sep);
        sa2_sb_puts(&b, p->v[i]);
        first = false;
    }
    return sa2_sb_take(&b);
}

/* -- sets (sorted string members inside {"__set__": [...]}) -- */
static json_t *sa2_set_new(void) {
    json_t *s = json_object();
    json_set(s, "__set__", json_array());
    return s;
}

static json_t *sa2_set_members(const json_t *s) {
    return sa2_is_set(s) ? json_obj_get(s, "__set__") : NULL;
}

static size_t sa2_set_len(const json_t *s) {
    const json_t *m = sa2_set_members(s);
    return m ? json_len(m) : 0;
}

static bool sa2_set_has(const json_t *s, const char *v) {
    const json_t *m = sa2_set_members(s);
    if (!m) return false;
    for (size_t i = 0; i < json_len(m); i++) {
        const char *t = sa2_str(json_get(m, i));
        if (t && strcmp(t, v) == 0) return true;
    }
    return false;
}

/* add a string member, keeping sorted order (dedup). */
static void sa2_set_add(json_t *s, const char *v) {
    json_t *m = sa2_set_members(s);
    if (!m || !v) return;
    size_t n = json_len(m), pos = n;
    for (size_t i = 0; i < n; i++) {
        const char *t = sa2_str(json_get(m, i));
        int cmp = strcmp(t ? t : "", v);
        if (cmp == 0) return;
        if (cmp > 0) { pos = i; break; }
    }
    /* insert at pos: append then shift */
    json_append(m, json_string(v));
    for (size_t i = n; i > pos; i--) {
        json_t *tmp = m->c.items[i];
        m->c.items[i] = m->c.items[i - 1];
        m->c.items[i - 1] = tmp;
    }
}

/* -- args builders (None-valued keys are dropped on both sides) -- */
static json_t *sa2_args_str(const char *key, const char *val) {
    json_t *a = json_object();
    if (val) json_set(a, key, json_string(val));
    return a;
}

static json_t *sa2_args_node(const char *key, const json_t *val) {
    json_t *a = json_object();
    if (val && val->type != JSON_NULL) json_set(a, key, json_copy(val));
    return a;
}

/* -- class constants (self.<CONST>, fixture-overridable via state) -- */
static double sa2_class_num(sa2_ctx_t *ctx, const char *name, double def) {
    const json_t *v = json_obj_get(ctx->state, name);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

/* -- full builtin exception hierarchy (CPython 3.14.9, generated from the
 * quarry interpreter; aliases EnvironmentError/IOError/WindowsError map to
 * OSError, ExceptionGroup carries its second base, asyncio.CancelledError
 * is a BaseException subclass). parent2 == "" means single inheritance. -- */
typedef struct sa2_exc_parent { const char *name, *parent1, *parent2; } sa2_exc_parent_t;

static const sa2_exc_parent_t SA2_EXC_TABLE[] = {
    {"ArithmeticError", "Exception", ""},
    {"AssertionError", "Exception", ""},
    {"AttributeError", "Exception", ""},
    {"BaseException", "object", ""},
    {"BaseExceptionGroup", "BaseException", ""},
    {"BlockingIOError", "OSError", ""},
    {"BrokenPipeError", "ConnectionError", ""},
    {"BufferError", "Exception", ""},
    {"BytesWarning", "Warning", ""},
    {"CancelledError", "BaseException", ""},
    {"ChildProcessError", "OSError", ""},
    {"ConnectionAbortedError", "ConnectionError", ""},
    {"ConnectionError", "OSError", ""},
    {"ConnectionRefusedError", "ConnectionError", ""},
    {"ConnectionResetError", "ConnectionError", ""},
    {"DeprecationWarning", "Warning", ""},
    {"EOFError", "Exception", ""},
    {"EncodingWarning", "Warning", ""},
    {"EnvironmentError", "OSError", ""},
    {"Exception", "BaseException", ""},
    {"ExceptionGroup", "BaseExceptionGroup", "Exception"},
    {"FileExistsError", "OSError", ""},
    {"FileNotFoundError", "OSError", ""},
    {"FloatingPointError", "ArithmeticError", ""},
    {"FutureWarning", "Warning", ""},
    {"GeneratorExit", "BaseException", ""},
    {"IOError", "OSError", ""},
    {"ImportError", "Exception", ""},
    {"ImportWarning", "Warning", ""},
    {"IndentationError", "SyntaxError", ""},
    {"IndexError", "LookupError", ""},
    {"InterruptedError", "OSError", ""},
    {"IsADirectoryError", "OSError", ""},
    {"KeyError", "LookupError", ""},
    {"KeyboardInterrupt", "BaseException", ""},
    {"LookupError", "Exception", ""},
    {"MemoryError", "Exception", ""},
    {"ModuleNotFoundError", "ImportError", ""},
    {"NameError", "Exception", ""},
    {"NotADirectoryError", "OSError", ""},
    {"NotImplementedError", "RuntimeError", ""},
    {"OSError", "Exception", ""},
    {"OverflowError", "ArithmeticError", ""},
    {"PendingDeprecationWarning", "Warning", ""},
    {"PermissionError", "OSError", ""},
    {"ProcessLookupError", "OSError", ""},
    {"PythonFinalizationError", "RuntimeError", ""},
    {"RecursionError", "RuntimeError", ""},
    {"ReferenceError", "Exception", ""},
    {"ResourceWarning", "Warning", ""},
    {"RuntimeError", "Exception", ""},
    {"RuntimeWarning", "Warning", ""},
    {"StopAsyncIteration", "Exception", ""},
    {"StopIteration", "Exception", ""},
    {"SyntaxError", "Exception", ""},
    {"SyntaxWarning", "Warning", ""},
    {"SystemError", "Exception", ""},
    {"SystemExit", "BaseException", ""},
    {"TabError", "IndentationError", ""},
    {"TimeoutError", "OSError", ""},
    {"TypeError", "Exception", ""},
    {"UnboundLocalError", "NameError", ""},
    {"UnicodeDecodeError", "UnicodeError", ""},
    {"UnicodeEncodeError", "UnicodeError", ""},
    {"UnicodeError", "ValueError", ""},
    {"UnicodeTranslateError", "UnicodeError", ""},
    {"UnicodeWarning", "Warning", ""},
    {"UserWarning", "Warning", ""},
    {"ValueError", "Exception", ""},
    {"Warning", "Exception", ""},
    {"WindowsError", "OSError", ""},
    {"ZeroDivisionError", "ArithmeticError", ""},
    {"_IncompleteInputError", "SyntaxError", ""},
};
#define SA2_EXC_TABLE_LEN (sizeof(SA2_EXC_TABLE) / sizeof(SA2_EXC_TABLE[0]))

static const sa2_exc_parent_t *sa2_builtin_entry(const char *name) {
    for (size_t i = 0; i < SA2_EXC_TABLE_LEN; i++)
        if (strcmp(SA2_EXC_TABLE[i].name, name) == 0) return &SA2_EXC_TABLE[i];
    return NULL;
}

/* Collect the ancestor-name chain of an exception kind: per-case aiohttp
 * class spec first (state.aiohttp.classes: name -> parent string), then the
 * builtin table; unknown kinds are synthetic Exception subclasses. */
typedef struct sa2_chain { char names[64][96]; size_t n; } sa2_chain_t;

static bool sa2_chain_has(const sa2_chain_t *c, const char *name) {
    for (size_t i = 0; i < c->n; i++)
        if (strcmp(c->names[i], name) == 0) return true;
    return false;
}

static void sa2_chain_add(sa2_chain_t *c, const char *name) {
    if (!name || !name[0] || c->n >= 64 || sa2_chain_has(c, name)) return;
    snprintf(c->names[c->n], sizeof c->names[0], "%s", name);
    c->n++;
}

static void sa2_chain_walk(sa2_chain_t *c, const char *name, const json_t *classes) {
    if (!name || !name[0] || sa2_chain_has(c, name)) return;
    sa2_chain_add(c, name);
    if (strcmp(name, "object") == 0) return;
    const char *p1 = NULL, *p2 = NULL;
    if (classes) {
        const json_t *e = json_obj_get(classes, name);
        if (e && e->type == JSON_STRING) p1 = e->str_val;
    }
    if (!p1) {
        const sa2_exc_parent_t *be = sa2_builtin_entry(name);
        if (be) { p1 = be->parent1; p2 = be->parent2; }
        else p1 = "Exception";  /* synthetic type(name, (Exception,), {}) */
    }
    if (p1 && strcmp(p1, name) != 0) sa2_chain_walk(c, p1, classes);
    if (p2 && p2[0] && strcmp(p2, name) != 0) sa2_chain_walk(c, p2, classes);
}

/* Is the named aiohttp attr a real class (parent-string spec entry)? */
static bool sa2_class_is_type(const json_t *classes, const char *name) {
    if (!classes) return false;
    const json_t *e = json_obj_get(classes, name);
    return e && e->type == JSON_STRING;
}

/* -- shared pure kernels -- */

/* _collect_slack_table_cell_text: text leaves of a raw/rich-text subtree. */
static void sa2_cell_visit(const json_t *node, sa2_parts_t *p) {
    if (!node) return;
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); i++)
            sa2_cell_visit(json_get(node, i), p);
        return;
    }
    if (!sa2_is_dict(node)) return;
    const json_t *text = json_obj_get(node, "text");
    if (text && text->type == JSON_STRING)
        sa2_parts_add(p, sa2_strdup(text->str_val));
    for (size_t i = 0; i < node->c.count; i++)
        sa2_cell_visit(node->c.items[i], p);
}

static char *sa2_cell_text(const json_t *value) {
    sa2_parts_t p; sa2_parts_init(&p);
    sa2_cell_visit(value, &p);
    char *joined = sa2_join(&p, " ", true);
    char *out = sa2_strip(joined);
    free(joined);
    sa2_parts_free(&p);
    return out;
}

/* _trim_oldest_dict_entries kernel: evict oldest-inserted down to half cap. */
static void sa2_trim_dict(json_t *mapping, double max_size) {
    if (!mapping || mapping->type != JSON_OBJECT) return;
    long long n = (long long)mapping->c.count;
    long long m = (long long)max_size;
    if (n <= m) return;
    long long half = (long long)floor((double)m / 2.0);
    long long excess = n - half;
    if (excess > n) excess = n;
    for (long long i = 0; i < excess && mapping->c.count > 0; i++)
        json_obj_del(mapping, mapping->c.keys[0]);
}

/* _slack_timestamp_sort_key kernel: two _int_or_zero seam calls; returns an
 * owned [int, int, str] array or a raise spec. */
static json_t *sa2_sort_key(sa2_ctx_t *ctx, const json_t *ts) {
    if (sa2_is_tuple_marker(ts)) {
        const json_t *m = json_obj_get(ts, "__tuple__");
        if (m && m->type == JSON_ARRAY && json_len(m) == 2)
            ts = json_get(m, 1);
    }
    char *s = sa2_py_str(ts);
    const char *dot = strchr(s, '.');
    char *seconds = dot ? strndup(s, (size_t)(dot - s)) : sa2_strdup(s);
    const char *fraction = dot ? dot + 1 : "";
    size_t fl = strlen(fraction);
    char fracbuf[8];
    size_t take = fl < 6 ? fl : 6;
    memcpy(fracbuf, fraction, take);
    for (size_t i = take; i < 6; i++) fracbuf[i] = '0';
    fracbuf[6] = '\0';
    const char *fracarg = fracbuf[0] ? fracbuf : "0";

    json_t *r1 = sa2_call(ctx, "_int_or_zero", sa2_args_str("value", seconds));
    if (sa2_is_raise(r1)) { free(s); free(seconds); return r1; }
    json_t *r2 = sa2_call(ctx, "_int_or_zero", sa2_args_str("value", fracarg));
    if (sa2_is_raise(r2)) { free(s); free(seconds); json_free(r1); return r2; }
    json_t *out = json_array();
    json_append(out, r1);
    json_append(out, r2);
    json_append(out, json_string(s));
    free(s); free(seconds);
    return out;
}

/* _truthy_config kernel. */
static bool sa2_truthy_value(const json_t *v) {
    if (v && v->type == JSON_STRING) {
        char *t = sa2_strip(v->str_val);
        char *l = sa2_lower(t);
        bool r = strcmp(l, "1") == 0 || strcmp(l, "true") == 0 ||
                 strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
        free(t); free(l);
        return r;
    }
    return sa2_truthy(v);
}

/* _lazy_attr with a BUILTIN factory (dict/set): no seam event, mutates
 * state in place, returns the BORROWED state node. */
static json_t *sa2_lazy_state(sa2_ctx_t *ctx, const char *name, int kind) {
    json_t *value = json_obj_get(ctx->state, name);
    if (value && value->type != JSON_NULL) return value;
    json_t *nv = kind == 0 ? json_object() : sa2_set_new();
    json_set(ctx->state, name, nv);
    return nv;
}

/* _purge_stale_slash_contexts kernel: time.monotonic seam + TTL sweep.
 * Returns NULL on success or a raise spec (no mutation on raise: the live
 * list comprehension evaluates every item before any pop). */
static json_t *sa2_purge(sa2_ctx_t *ctx) {
    json_t *now = sa2_call(ctx, "time.monotonic", json_object());
    if (sa2_is_raise(now)) return now;
    json_t *contexts = json_obj_get(ctx->state, "slash_command_contexts");
    if (!contexts) { json_free(now); return sa2_ns_attr_err("slash_command_contexts"); }
    if (contexts->type != JSON_ARRAY) { json_free(now); return sa2_attr_err(contexts, "items"); }
    double ttl = sa2_class_num(ctx, "_SLASH_CTX_TTL", SA2_SLASH_CTX_TTL);
    size_t n = json_len(contexts);
    bool *stale = calloc(n ? n : 1, sizeof(bool));
    json_t *raise = NULL;
    for (size_t i = 0; i < n; i++) {
        const json_t *pair = json_get(contexts, i);
        const json_t *v = pair ? json_obj_get(pair, "value") : NULL;
        if (!sa2_is_dict(v)) { raise = sa2_subscript_err(v, "str"); break; }
        const json_t *ts = json_obj_get(v, "ts");
        if (!ts) { raise = sa2_err("KeyError", "'ts'"); break; }
        if (!now || now->type != JSON_NUMBER || ts->type != JSON_NUMBER) {
            raise = sa2_sub_type_err(now, ts); break;
        }
        if (now->num_val - ts->num_val > ttl) stale[i] = true;
    }
    if (!raise) {
        for (size_t i = n; i > 0; i--)
            if (stale[i - 1]) json_array_remove(contexts, i - 1);
    }
    free(stale);
    json_free(now);
    return raise;
}

/* -- ported functions -- */

/* PoP: sa2_slack_error_is @ plugins/platforms/slack/adapter.py:_slack_error_is */
static json_t *sa2_slack_error_is(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *spec = (exc && exc->type == JSON_OBJECT) ? json_obj_get(exc, "__exc__") : NULL;
    const json_t *response = spec ? json_obj_get(spec, "response") : NULL;
    const char *code = json_get_str(args, "code", "");
    bool result = false;
    if (sa2_truthy(response)) {
        if (sa2_is_dict(response)) {
            const json_t *err = json_obj_get(response, "error");
            const char *es = sa2_str(err);
            result = es && strcmp(es, code) == 0;
        }
        /* truthy non-dict: response.get raises AttributeError, caught by
         * the live except Exception -> False (no seam, no trace event). */
    }
    return json_bool(result);
}

/* PoP: sa2_is_transient_transport_error @ plugins/platforms/slack/adapter.py:_is_transient_transport_error */
static json_t *sa2_is_transient_transport_error(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *spec = (exc && exc->type == JSON_OBJECT) ? json_obj_get(exc, "__exc__") : NULL;
    const char *kind = spec ? json_get_str(spec, "type", "") : "";
    const json_t *aio = json_obj_get(ctx->state, "aiohttp");
    const json_t *classes = (aio && aio->type == JSON_OBJECT) ? json_obj_get(aio, "classes") : NULL;
    bool conn_is_type = sa2_class_is_type(classes, "ClientConnectionError");
    bool ssl_is_type = sa2_class_is_type(classes, "ClientSSLError");
    bool fp_is_type = sa2_class_is_type(classes, "ServerFingerprintMismatch");
    sa2_chain_t chain; chain.n = 0;
    sa2_chain_walk(&chain, kind, classes);
    bool is_timeout = sa2_chain_has(&chain, "TimeoutError");
    bool is_conn = conn_is_type && sa2_chain_has(&chain, "ClientConnectionError");
    bool is_tls = (ssl_is_type && sa2_chain_has(&chain, "ClientSSLError")) ||
                  (fp_is_type && sa2_chain_has(&chain, "ServerFingerprintMismatch"));
    return json_bool(is_timeout || (is_conn && !is_tls));
}

/* PoP: sa2_collect_slack_table_cell_text @ plugins/platforms/slack/adapter.py:_collect_slack_table_cell_text */
static json_t *sa2_collect_slack_table_cell_text(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    char *t = sa2_cell_text(json_obj_get(args, "value"));
    json_t *out = json_string(t);
    free(t);
    return out;
}

/* PoP: sa2_render_slack_table_block @ plugins/platforms/slack/adapter.py:_render_slack_table_block */
static json_t *sa2_render_slack_table_block(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *block = json_obj_get(args, "block");
    double max_chars = json_get_num(args, "max_chars", SA2_TABLE_MAX_CHARS);
    const json_t *rows = sa2_is_dict(block) ? json_obj_get(block, "rows") : NULL;
    if (!rows || rows->type != JSON_ARRAY) return json_string("");
    sa2_parts_t lines; sa2_parts_init(&lines);
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        if (!row || row->type != JSON_ARRAY) continue;
        sa2_parts_t cells; sa2_parts_init(&cells);
        for (size_t j = 0; j < json_len(row); j++)
            sa2_parts_add(&cells, sa2_cell_text(json_get(row, j)));
        char *rendered = sa2_join(&cells, " | ", false);
        sa2_parts_free(&cells);
        char *probe = sa2_strip_chars(rendered, " |");
        if (probe[0]) sa2_parts_add(&lines, rendered);
        else free(rendered);
        free(probe);
    }
    char *text = sa2_join(&lines, "\n", false);
    sa2_parts_free(&lines);
    if (!text[0]) { free(text); return json_string(""); }
    size_t len = strlen(text);
    if ((double)len > max_chars) {
        long long stop = (long long)max_chars - 20;
        if (stop < 0) stop = (stop + (long long)len < 0) ? 0 : stop + (long long)len;
        if (stop > (long long)len) stop = (long long)len;
        char *cut = strndup(text, (size_t)stop);
        char *trimmed = sa2_rstrip(cut);
        sa2_sb_t b; sa2_sb_init(&b);
        sa2_sb_puts(&b, trimmed);
        sa2_sb_puts(&b, "\n[table truncated]");
        free(cut); free(trimmed); free(text);
        char *out = sa2_sb_take(&b);
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    json_t *r = json_string(text);
    free(text);
    return r;
}

/* _extract_text_from_slack_blocks: rich-text walker with seamed inline
 * rendering + seamed table projection. Returns NULL or a raise spec. */
static json_t *sa2_append_line(sa2_parts_t *parts, const char *text,
                               int quote_depth, const char *bullet) {
    if (!text || !text[0]) return NULL;
    char *probe = sa2_strip(text);
    bool empty = !probe[0];
    free(probe);
    if (empty) return NULL;
    sa2_sb_t b; sa2_sb_init(&b);
    for (int i = 0; i < quote_depth; i++) sa2_sb_puts(&b, ">");
    if (quote_depth) sa2_sb_puts(&b, " ");
    sa2_sb_puts(&b, bullet);
    sa2_sb_puts(&b, text);
    char *joined = sa2_sb_take(&b);
    char *trimmed = sa2_rstrip(joined);
    free(joined);
    sa2_parts_add(parts, trimmed);
    return NULL;
}

static json_t *sa2_walk_elements(sa2_ctx_t *ctx, const json_t *elements,
                                 int quote_depth, const char *bullet,
                                 sa2_parts_t *parts);

static json_t *sa2_render_seam(sa2_ctx_t *ctx, const json_t *elements, char **out) {
    json_t *a = json_object();
    json_set(a, "elements", elements ? json_copy(elements) : json_array());
    json_t *r = sa2_call(ctx, "_render_inline_elements", a);
    if (sa2_is_raise(r)) return r;
    *out = sa2_py_str(r);
    json_free(r);
    return NULL;
}

static json_t *sa2_walk_elements(sa2_ctx_t *ctx, const json_t *elements,
                                 int quote_depth, const char *bullet,
                                 sa2_parts_t *parts) {
    /* every call site reads elem.get("elements", []): a MISSING key defaults
     * to [] (no iteration); a present null is `for elem in None` -> TypeError */
    if (!elements) return NULL;
    if (elements->type == JSON_NULL)
        return sa2_err("TypeError", "'NoneType' object is not iterable");
    if (elements->type != JSON_ARRAY) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", sa2_type_name(elements));
        return sa2_err("TypeError", msg);
    }
    for (size_t i = 0; i < json_len(elements); i++) {
        const json_t *elem = json_get(elements, i);
        if (!sa2_is_dict(elem)) return sa2_attr_err(elem, "get");
        const char *t = json_get_str(elem, "type", "");
        const json_t *els = json_obj_get(elem, "elements");
        if (strcmp(t, "rich_text_section") == 0) {
            char *text = NULL;
            json_t *r = sa2_render_seam(ctx, els, &text);
            if (r) return r;
            sa2_append_line(parts, text, quote_depth, bullet);
            free(text);
        } else if (strcmp(t, "rich_text_quote") == 0) {
            json_t *r = sa2_walk_elements(ctx, els, quote_depth + 1, "", parts);
            if (r) return r;
        } else if (strcmp(t, "rich_text_list") == 0) {
            const char *style = sa2_str(json_obj_get(elem, "style"));
            size_t n = els && els->type == JSON_ARRAY ? json_len(els) : 0;
            for (size_t idx = 0; idx < n; idx++) {
                char item_bullet[32];
                if (style && strcmp(style, "bullet") == 0)
                    snprintf(item_bullet, sizeof item_bullet, "\xe2\x80\xa2 ");
                else
                    snprintf(item_bullet, sizeof item_bullet, "%zu. ", idx + 1);
                json_t *one = json_array();
                json_append(one, json_copy(json_get(els, idx)));
                json_t *r = sa2_walk_elements(ctx, one, quote_depth, item_bullet, parts);
                json_free(one);
                if (r) return r;
            }
        } else if (strcmp(t, "rich_text_preformatted") == 0) {
            sa2_parts_t code_lines; sa2_parts_init(&code_lines);
            json_t *r = NULL;
            size_t n = els && els->type == JSON_ARRAY ? json_len(els) : 0;
            for (size_t ci = 0; ci < n; ci++) {
                const json_t *child = json_get(els, ci);
                if (!sa2_is_dict(child)) { r = sa2_attr_err(child, "get"); break; }
                const json_t *child_els = NULL;
                json_t *wrap = NULL;
                if (strcmp(json_get_str(child, "type", ""), "rich_text_section") == 0) {
                    child_els = json_obj_get(child, "elements");
                } else {
                    wrap = json_array();
                    json_append(wrap, json_copy(child));
                    child_els = wrap;
                }
                char *line = NULL;
                r = sa2_render_seam(ctx, child_els, &line);
                json_free(wrap);
                if (r) break;
                sa2_parts_add(&code_lines, line);
            }
            if (r) { sa2_parts_free(&code_lines); return r; }
            char *code_text = sa2_join(&code_lines, "\n", true);
            sa2_parts_free(&code_lines);
            if (code_text[0]) {
                const char *lang = json_get_str(elem, "language", "");
                sa2_sb_t b; sa2_sb_init(&b);
                sa2_sb_puts(&b, "```");
                sa2_sb_puts(&b, lang);
                sa2_sb_puts(&b, "\n");
                sa2_sb_puts(&b, code_text);
                sa2_sb_puts(&b, "\n```");
                char *line = sa2_sb_take(&b);
                sa2_append_line(parts, line, quote_depth, bullet);
                free(line);
            }
            free(code_text);
        } else {
            json_t *one = json_array();
            json_append(one, json_copy(elem));
            char *text = NULL;
            json_t *r = sa2_render_seam(ctx, one, &text);
            json_free(one);
            if (r) return r;
            sa2_append_line(parts, text, quote_depth, bullet);
            free(text);
        }
    }
    return NULL;
}

/* PoP: sa2_extract_text_from_slack_blocks @ plugins/platforms/slack/adapter.py:_extract_text_from_slack_blocks */
static json_t *sa2_extract_text_from_slack_blocks(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *blocks = json_obj_get(args, "blocks");
    if (!sa2_truthy(blocks)) return json_string("");
    if (blocks->type != JSON_ARRAY) {
        char msg[256];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", sa2_type_name(blocks));
        return sa2_err("TypeError", msg);
    }
    sa2_parts_t parts; sa2_parts_init(&parts);
    json_t *raise = NULL;
    for (size_t i = 0; i < json_len(blocks); i++) {
        const json_t *block = json_get(blocks, i);
        const json_t *src = sa2_truthy(block) ? block : NULL;
        if (src && !sa2_is_dict(src)) { raise = sa2_attr_err(src, "get"); break; }
        const char *bt = src ? sa2_str(json_obj_get(src, "type")) : NULL;
        if (bt && strcmp(bt, "rich_text") == 0) {
            raise = sa2_walk_elements(ctx, json_obj_get(block, "elements"), 0, "", &parts);
            if (raise) break;
        } else if (bt && strcmp(bt, "table") == 0) {
            json_t *r = sa2_call(ctx, "_render_slack_table_block",
                                 sa2_args_node("block", block));
            if (sa2_is_raise(r)) { raise = r; break; }
            if (sa2_truthy(r)) {
                char *t = sa2_py_str(r);
                sa2_parts_add(&parts, t);
            }
            json_free(r);
        }
    }
    if (raise) { sa2_parts_free(&parts); return raise; }
    char *out = sa2_join(&parts, "\n", false);
    sa2_parts_free(&parts);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: sa2_extract_urls_from_slack_blocks @ plugins/platforms/slack/adapter.py:_extract_urls_from_slack_blocks */
static void sa2_url_walk(const json_t *node, sa2_parts_t *found, sa2_parts_t *seen) {
    if (sa2_is_dict(node)) {
        static const char *keys[] = {"url", "image_url", "external_url"};
        for (size_t k = 0; k < 3; k++) {
            const char *v = sa2_str(json_obj_get(node, keys[k]));
            if (!v) continue;
            if (strncmp(v, "http://", 7) != 0 && strncmp(v, "https://", 8) != 0) continue;
            bool dup = false;
            for (size_t i = 0; i < seen->n; i++)
                if (strcmp(seen->v[i], v) == 0) { dup = true; break; }
            if (!dup) {
                sa2_parts_add(seen, sa2_strdup(v));
                sa2_parts_add(found, sa2_strdup(v));
            }
        }
        for (size_t i = 0; i < node->c.count; i++)
            sa2_url_walk(node->c.items[i], found, seen);
    } else if (node && node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); i++)
            sa2_url_walk(json_get(node, i), found, seen);
    }
}

static json_t *sa2_extract_urls_from_slack_blocks(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *blocks = json_obj_get(args, "blocks");
    json_t *out = json_array();
    if (!sa2_truthy(blocks)) return out;
    sa2_parts_t found, seen;
    sa2_parts_init(&found); sa2_parts_init(&seen);
    sa2_url_walk(blocks, &found, &seen);
    for (size_t i = 0; i < found.n; i++)
        json_append(out, json_string(found.v[i]));
    sa2_parts_free(&found); sa2_parts_free(&seen);
    return out;
}

/* PoP: sa2_apply_slack_proxy @ plugins/platforms/slack/adapter.py:_apply_slack_proxy */
static json_t *sa2_apply_slack_proxy(sa2_ctx_t *ctx, const json_t *args) {
    json_t *client = json_obj_get(ctx->state, "client");
    const json_t *proxy_url = json_obj_get(args, "proxy_url");
    if (sa2_is_obj(client)) {
        json_t *attrs = sa2_obj_attrs(client);
        if (attrs && json_has(attrs, "proxy"))
            json_set(attrs, "proxy", proxy_url ? json_copy(proxy_url) : json_null());
    }
    return json_null();
}

/* PoP: sa2_slack_timestamp_sort_key @ plugins/platforms/slack/adapter.py:_slack_timestamp_sort_key */
static json_t *sa2_slack_timestamp_sort_key(sa2_ctx_t *ctx, const json_t *args) {
    return sa2_sort_key(ctx, json_obj_get(args, "ts"));
}

/* PoP: sa2_trim_oldest_dict_entries @ plugins/platforms/slack/adapter.py:_trim_oldest_dict_entries */
static json_t *sa2_trim_oldest_dict_entries(sa2_ctx_t *ctx, const json_t *args) {
    json_t *mapping = json_obj_get(ctx->state, "mapping");
    sa2_trim_dict(mapping, json_get_num(args, "max_size", 0));
    return json_null();
}

typedef struct sa2_row {
    char *key; long long a, b; char *s; size_t idx;
} sa2_row_t;

static int sa2_row_cmp(const void *pa, const void *pb) {
    const sa2_row_t *x = pa, *y = pb;
    if (x->a != y->a) return x->a < y->a ? -1 : 1;
    if (x->b != y->b) return x->b < y->b ? -1 : 1;
    int c = strcmp(x->s, y->s);
    if (c) return c;
    return x->idx < y->idx ? -1 : (x->idx > y->idx ? 1 : 0);
}

/* PoP: sa2_discard_oldest_by_thread_ts @ plugins/platforms/slack/adapter.py:_discard_oldest_by_thread_ts */
static json_t *sa2_discard_oldest_by_thread_ts(sa2_ctx_t *ctx, const json_t *args) {
    json_t *entries = json_obj_get(ctx->state, "entries");
    double count = json_get_num(args, "count", 0);
    bool use_getter = sa2_truthy(json_obj_get(args, "ts_getter"));
    if (count <= 0) return json_null();
    if (!entries || entries->type != JSON_OBJECT) return sa2_ns_attr_err("entries");
    size_t n = entries->c.count;
    sa2_row_t *rows = calloc(n ? n : 1, sizeof(sa2_row_t));
    json_t *raise = NULL;
    for (size_t i = 0; i < n; i++) {
        const char *key = entries->c.keys[i];
        rows[i].key = sa2_strdup(key);
        rows[i].idx = i;
        json_t *tsval;
        if (use_getter) {
            tsval = sa2_call(ctx, "ts_getter", sa2_args_str("e", key));
            if (sa2_is_raise(tsval)) { raise = tsval; break; }
        } else {
            tsval = json_string(key);
        }
        json_t *sk = sa2_sort_key(ctx, tsval);
        json_free(tsval);
        if (sa2_is_raise(sk)) { raise = sk; break; }
        rows[i].a = (long long)json_number_value(json_get(sk, 0));
        rows[i].b = (long long)json_number_value(json_get(sk, 1));
        rows[i].s = sa2_strdup(sa2_str(json_get(sk, 2)));
        json_free(sk);
    }
    if (!raise) {
        qsort(rows, n, sizeof(sa2_row_t), sa2_row_cmp);
        long long remove = (long long)count;
        if (remove > (long long)n) remove = (long long)n;
        for (long long i = 0; i < remove; i++)
            json_obj_del(entries, rows[i].key);
    }
    for (size_t i = 0; i < n; i++) { free(rows[i].key); free(rows[i].s); }
    free(rows);
    return raise ? raise : json_null();
}

/* PoP: sa2_metadata_team_id @ plugins/platforms/slack/adapter.py:_metadata_team_id */
static json_t *sa2_first_truthy_seam(sa2_ctx_t *ctx, const json_t *mapping,
                                     const char *const *keys, size_t nkeys) {
    json_t *a = json_object();
    json_set(a, "mapping", mapping ? json_copy(mapping) : json_null());
    json_t *ks = json_array();
    for (size_t i = 0; i < nkeys; i++) json_append(ks, json_string(keys[i]));
    json_set(a, "keys", ks);
    return sa2_call(ctx, "_first_truthy", a);
}

static json_t *sa2_metadata_team_id(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *metadata = json_obj_get(args, "metadata");
    if (!sa2_truthy(metadata)) return json_string("");
    static const char *keys1[] = {"scope_id", "slack_team_id", "team_id", "team", "guild_id", "workspace_id"};
    json_t *r = sa2_first_truthy_seam(ctx, metadata, keys1, 6);
    if (sa2_is_raise(r)) return r;
    if (sa2_truthy(r)) {
        char *s = sa2_py_str(r);
        json_free(r);
        json_t *out = json_string(s);
        free(s);
        return out;
    }
    json_free(r);
    if (!sa2_is_dict(metadata)) return sa2_attr_err(metadata, "get");
    const json_t *source = json_obj_get(metadata, "source");
    if (sa2_is_dict(source)) {
        static const char *keys2[] = {"scope_id", "slack_team_id", "team_id", "guild_id"};
        r = sa2_first_truthy_seam(ctx, source, keys2, 4);
        if (sa2_is_raise(r)) return r;
        if (sa2_truthy(r)) {
            char *s = sa2_py_str(r);
            json_free(r);
            json_t *out = json_string(s);
            free(s);
            return out;
        }
        json_free(r);
    } else if (source && source->type != JSON_NULL) {
        const json_t *value = NULL;
        if (sa2_is_obj(source)) {
            const json_t *attrs = sa2_obj_attrs(source);
            value = json_obj_get(attrs, "scope_id");
            if (!sa2_truthy(value)) value = json_obj_get(attrs, "guild_id");
        }
        if (sa2_truthy(value)) {
            char *s = sa2_py_str(value);
            json_t *out = json_string(s);
            free(s);
            return out;
        }
    }
    return json_string("");
}

/* PoP: sa2_workspace_event_id @ plugins/platforms/slack/adapter.py:_workspace_event_id */
static json_t *sa2_workspace_event_id(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *team = json_obj_get(args, "team_id");
    const json_t *ev = json_obj_get(args, "event_id");
    char *es = sa2_py_str(ev);
    if (sa2_truthy(team)) {
        char *ts = sa2_py_str(team);
        sa2_sb_t b; sa2_sb_init(&b);
        sa2_sb_puts(&b, ts);
        sa2_sb_puts(&b, ":");
        sa2_sb_puts(&b, es);
        free(ts); free(es);
        char *s = sa2_sb_take(&b);
        json_t *out = json_string(s);
        free(s);
        return out;
    }
    json_t *out = json_string(es);
    free(es);
    return out;
}

/* PoP: sa2_workspace_message_marker @ plugins/platforms/slack/adapter.py:_workspace_message_marker */
static json_t *sa2_workspace_message_marker(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *team = json_obj_get(args, "team_id");
    const json_t *mid = json_obj_get(args, "message_id");
    char *ms = sa2_py_str(mid);
    if (sa2_truthy(team)) {
        char *ts = sa2_py_str(team);
        json_t *out = json_array();
        json_append(out, json_string(ts));
        json_append(out, json_string(ms));
        free(ts); free(ms);
        return out;
    }
    json_t *out = json_string(ms);
    free(ms);
    return out;
}

/* PoP: sa2_truthy_config @ plugins/platforms/slack/adapter.py:_truthy_config */
static json_t *sa2_truthy_config(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_bool(sa2_truthy_value(json_obj_get(args, "value")));
}

/* PoP: sa2_task_update_chunk @ plugins/platforms/slack/adapter.py:_task_update_chunk */
static json_t *sa2_task_update_chunk(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *task = json_obj_get(args, "task");
    if (!sa2_is_dict(task)) return sa2_attr_err(task, "get");
    const json_t *sn = json_obj_get(task, "status");
    char *status = sa2_truthy(sn) ? sa2_py_str(sn) : sa2_strdup("in_progress");
    if (strcmp(status, "in_progress") != 0 && strcmp(status, "complete") != 0 &&
        strcmp(status, "error") != 0) {
        free(status);
        status = sa2_strdup("in_progress");
    }
    const json_t *idn = json_obj_get(task, "id");
    const json_t *tidn = json_obj_get(task, "task_id");
    const json_t *idsrc = sa2_truthy(idn) ? idn : (sa2_truthy(tidn) ? tidn : NULL);
    char *task_id = idsrc ? sa2_py_str(idsrc) : sa2_strdup("task");
    const json_t *titlen = json_obj_get(task, "title");
    char *title_full = sa2_truthy(titlen) ? sa2_py_str(titlen) : sa2_strdup(task_id);
    size_t tl = strlen(title_full);
    if (tl > 256) title_full[256] = '\0';
    json_t *out = json_object();
    json_set(out, "type", json_string("task_update"));
    json_set(out, "id", json_string(task_id));
    json_set(out, "title", json_string(title_full));
    json_set(out, "status", json_string(status));
    free(status); free(task_id); free(title_full);
    return out;
}

/* PoP: sa2_stream_relation @ plugins/platforms/slack/adapter.py:_stream_relation */
static json_t *sa2_stream_relation(sa2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *sent = json_get_str(args, "sent", "");
    const char *text = json_get_str(args, "text", "");
    char *core = sa2_strip(sent);
    json_t *out = json_array();
    if (!core[0]) {
        json_append(out, json_string("unrelated"));
        json_append(out, json_string(""));
        free(core);
        return out;
    }
    size_t sl = strlen(sent), cl = strlen(core);
    const char *rel = "unrelated";
    const char *delta = "";
    if (strncmp(text, sent, sl) == 0) {
        delta = text + sl;
        rel = delta[0] ? "extends" : "equal";
    } else {
        size_t lead = sa2_lstrip_offset(text);
        if (strncmp(text + lead, core, cl) == 0) {
            delta = text + lead + cl;
            char *d = sa2_strip(delta);
            rel = d[0] ? "extends" : "equal";
            free(d);
        }
    }
    json_append(out, json_string(rel));
    json_append(out, json_string(delta));
    free(core);
    return out;
}

/* PoP: sa2_lazy_attr @ plugins/platforms/slack/adapter.py:_lazy_attr */
static json_t *sa2_lazy_attr(sa2_ctx_t *ctx, const json_t *args) {
    const char *name = json_get_str(args, "name", "");
    json_t *value = json_obj_get(ctx->state, name);
    if (value && value->type != JSON_NULL) return json_copy(value);
    json_t *r = sa2_call(ctx, "factory", json_object());
    if (sa2_is_raise(r)) return r;
    json_set(ctx->state, name, json_copy(r));
    return r;
}

/* PoP: sa2_remember_channel_team @ plugins/platforms/slack/adapter.py:_remember_channel_team */
static json_t *sa2_remember_channel_team(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *cid = json_obj_get(args, "channel_id");
    const json_t *tid = json_obj_get(args, "team_id");
    if (!sa2_truthy(cid) || !sa2_truthy(tid)) return json_null();
    char *c = sa2_py_str(cid);
    char *t = sa2_py_str(tid);
    json_t *channel_teams = sa2_lazy_state(ctx, "_channel_teams", 0);
    json_t *teams = json_obj_get(channel_teams, c);
    if (!sa2_is_set(teams)) {
        teams = sa2_set_new();
        json_set(channel_teams, c, teams);
    }
    sa2_set_add(teams, t);
    json_t *channel_team = json_obj_get(ctx->state, "_channel_team");
    if (!channel_team) { free(c); free(t); return sa2_ns_attr_err("_channel_team"); }
    if (sa2_set_len(teams) == 1) json_set(channel_team, c, json_string(t));
    else json_obj_del(channel_team, c);
    double cap = sa2_class_num(ctx, "_CHANNEL_TEAM_MAX", SA2_CHANNEL_TEAM_MAX);
    sa2_trim_dict(channel_team, cap);
    sa2_trim_dict(channel_teams, cap);
    free(c); free(t);
    return json_null();
}

/* tuple-key equality for slash contexts (structural JSON equality). */
static bool sa2_key_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != JSON_ARRAY || b->type != JSON_ARRAY) return false;
    if (json_len(a) != json_len(b)) return false;
    for (size_t i = 0; i < json_len(a); i++) {
        const json_t *x = json_get(a, i), *y = json_get(b, i);
        if (!x || !y || x->type != y->type) return false;
        switch (x->type) {
            case JSON_NULL: break;
            case JSON_BOOL: if (x->bool_val != y->bool_val) return false; break;
            case JSON_NUMBER: if (x->num_val != y->num_val) return false; break;
            case JSON_STRING:
                if (strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "") != 0)
                    return false;
                break;
            default: return false;  /* nested containers: outside the corpus */
        }
    }
    return true;
}

/* PoP: sa2_pop_slash_context @ plugins/platforms/slack/adapter.py:_pop_slash_context */
static json_t *sa2_pop_slash_context(sa2_ctx_t *ctx, const json_t *args) {
    json_t *pr = sa2_purge(ctx);
    if (pr) return pr;
    const json_t *team_node = json_obj_get(args, "team_id");
    char *team = sa2_truthy(team_node) ? sa2_py_str(team_node) : sa2_strdup("");
    const json_t *uid = json_obj_get(ctx->state, "slash_user_id");
    if (!sa2_truthy(uid)) { free(team); return json_null(); }
    const json_t *chat = json_obj_get(args, "chat_id");
    json_t *key = json_array();
    if (team[0]) json_append(key, json_string(team));
    json_append(key, chat ? json_copy(chat) : json_null());
    json_append(key, json_copy(uid));
    free(team);
    json_t *contexts = json_obj_get(ctx->state, "slash_command_contexts");
    json_t *out = json_null();
    if (contexts && contexts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(contexts); i++) {
            const json_t *pair = json_get(contexts, i);
            const json_t *k = pair ? json_obj_get(pair, "key") : NULL;
            if (sa2_key_eq(k, key)) {
                const json_t *v = json_obj_get(pair, "value");
                out = v ? json_copy(v) : json_null();
                json_array_remove(contexts, i);
                break;
            }
        }
    }
    json_free(key);
    return out;
}

/* PoP: sa2_purge_stale_slash_contexts @ plugins/platforms/slack/adapter.py:_purge_stale_slash_contexts */
static json_t *sa2_purge_stale_slash_contexts(sa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *r = sa2_purge(ctx);
    return r ? r : json_null();
}

/* PoP: sa2_scope_id_for_chat @ plugins/platforms/slack/adapter.py:scope_id_for_chat */
static json_t *sa2_scope_id_for_chat(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *chat = json_obj_get(args, "chat_id");
    const json_t *team = NULL;
    json_t *fallback = NULL;
    json_t *raise = NULL;
    if (sa2_truthy(chat)) {
        const json_t *ct = json_obj_get(ctx->state, "_channel_team");
        if (ct && ct->type != JSON_NULL) {
            if (!sa2_is_dict(ct)) { raise = sa2_attr_err(ct, "get"); goto done; }
        } else ct = NULL;
        char *cs = sa2_py_str(chat);
        team = ct ? json_obj_get(ct, cs) : NULL;
        free(cs);
    } else {
        team = chat;
    }
    if (!sa2_truthy(team) && sa2_truthy(chat)) {
        char *cs = sa2_py_str(chat);
        const json_t *cts = json_obj_get(ctx->state, "_channel_teams");
        if (!cts || cts->type == JSON_NULL) cts = NULL;
        else if (!sa2_is_dict(cts)) { free(cs); raise = sa2_attr_err(cts, "__contains__"); goto done; }
        bool known = cts && json_has(cts, cs);
        if (!known) {
            const json_t *tc = json_obj_get(ctx->state, "_team_clients");
            if (tc && tc->type == JSON_OBJECT && !sa2_is_obj(tc) && tc->c.count == 1) {
                fallback = json_string(tc->c.keys[0]);
                team = fallback;
            }
        }
        free(cs);
    }
done:
    if (raise) { json_free(fallback); return raise; }
    if (sa2_truthy(team)) {
        char *s = sa2_py_str(team);
        json_t *out = json_string(s);
        free(s);
        json_free(fallback);
        return out;
    }
    json_free(fallback);
    return json_null();
}

/* PoP: sa2_get_client @ plugins/platforms/slack/adapter.py:_get_client */
static json_t *sa2_get_client(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *chat = json_obj_get(args, "chat_id");
    const json_t *team = json_obj_get(args, "team_id");
    /* `if team_id and team_id in self._team_clients`: the attr is touched
     * ONLY when team_id is truthy (short-circuit). */
    const char *tk = sa2_str(team);
    if (sa2_truthy(team)) {
        const json_t *tc = json_obj_get(ctx->state, "_team_clients");
        if (!tc) return sa2_ns_attr_err("_team_clients");
        if (tk && json_has(tc, tk))
            return json_copy(json_obj_get(tc, tk));
    }
    const json_t *ct = json_obj_get(ctx->state, "_channel_team");
    if (!ct) return sa2_ns_attr_err("_channel_team");
    const char *ck = sa2_str(chat);
    const json_t *team2 = ck ? json_obj_get(ct, ck) : NULL;
    const char *tk2 = sa2_str(team2);
    if (sa2_truthy(team2)) {
        const json_t *tc = json_obj_get(ctx->state, "_team_clients");
        if (!tc) return sa2_ns_attr_err("_team_clients");
        if (tk2 && json_has(tc, tk2))
            return json_copy(json_obj_get(tc, tk2));
    }
    const json_t *app = json_obj_get(ctx->state, "_app");
    if (!app) return sa2_ns_attr_err("_app");
    if (app->type == JSON_NULL) return sa2_attr_err(app, "client");
    if (!sa2_is_obj(app)) return sa2_attr_err(app, "client");
    const json_t *client = json_obj_get(sa2_obj_attrs(app), "client");
    if (!client) return sa2_attr_err(app, "client");
    return json_copy(client);
}

/* PoP: sa2_is_ignored_channel @ plugins/platforms/slack/adapter.py:_is_ignored_channel */
static bool sa2_member(const json_t *coll, const char *v) {
    const json_t *arr = NULL;
    if (coll && coll->type == JSON_ARRAY) arr = coll;
    else if (sa2_is_set(coll)) arr = sa2_set_members(coll);
    if (!arr) return false;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *t = sa2_str(json_get(arr, i));
        if (t && strcmp(t, v) == 0) return true;
    }
    return false;
}

static json_t *sa2_is_ignored_channel(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *chat = json_obj_get(args, "chat_id");
    if (!sa2_truthy(chat)) return json_bool(false);
    json_t *r = sa2_call(ctx, "_slack_ignored_channels", json_object());
    if (sa2_is_raise(r)) return r;
    char *cs = sa2_py_str(chat);
    char *colon = strchr(cs, ':');
    char *prefix = colon ? strndup(cs, (size_t)(colon - cs)) : sa2_strdup(cs);
    bool found = sa2_member(r, "*") || sa2_member(r, prefix);
    free(cs); free(prefix);
    json_free(r);
    return json_bool(found);
}

/* PoP: sa2_native_task_cards_enabled @ plugins/platforms/slack/adapter.py:native_task_cards_enabled */
static json_t *sa2_native_task_cards_enabled(sa2_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *config = json_obj_get(ctx->state, "config");
    if (!config) return sa2_ns_attr_err("config");
    if (!sa2_is_obj(config)) return sa2_attr_err(config, "extra");
    const json_t *attrs = sa2_obj_attrs(config);
    if (!json_has(attrs, "extra")) return sa2_attr_err(config, "extra");
    const json_t *extra = json_obj_get(attrs, "extra");
    if (!sa2_is_dict(extra)) extra = NULL;
    const json_t *streaming = extra ? json_obj_get(extra, "streaming") : NULL;
    const json_t *progress = sa2_is_dict(streaming) ? json_obj_get(streaming, "progress") : NULL;
    const json_t *scopes[2] = {extra, sa2_is_dict(progress) ? progress : NULL};
    for (size_t i = 0; i < 2; i++) {
        const json_t *scope = scopes[i];
        if (!scope) continue;
        const json_t *value = json_has(scope, "native_task_cards")
            ? json_obj_get(scope, "native_task_cards")
            : json_obj_get(scope, "nativeTaskCards");
        if (value && value->type != JSON_NULL)
            return json_bool(sa2_truthy_value(value));
    }
    return json_bool(false);
}

/* {file_label}/{error} template substitution (str.format on the two keys). */
static char *sa2_fmt_template(const char *tmpl, const char *label, const char *error) {
    sa2_sb_t b; sa2_sb_init(&b);
    const char *p = tmpl;
    while (*p) {
        if (strncmp(p, "{file_label}", 12) == 0) { sa2_sb_puts(&b, label); p += 12; }
        else if (strncmp(p, "{error}", 7) == 0) { sa2_sb_puts(&b, error); p += 7; }
        else { char c[2] = {*p, '\0'}; sa2_sb_puts(&b, c); p++; }
    }
    return sa2_sb_take(&b);
}

/* PoP: sa2_describe_slack_api_error @ plugins/platforms/slack/adapter.py:_describe_slack_api_error */
static json_t *sa2_describe_slack_api_error(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *response = json_obj_get(args, "response");
    const json_t *file_obj = json_obj_get(args, "file_obj");
    if (!response || response->type == JSON_NULL || !sa2_is_dict(response))
        return json_null();
    const json_t *errn = json_obj_get(response, "error");
    char *err_raw = sa2_truthy(errn) ? sa2_py_str(errn) : sa2_strdup("");
    char *error = sa2_strip(err_raw);
    free(err_raw);
    if (!error[0]) { free(error); return json_null(); }
    json_t *r = sa2_call(ctx, "_attachment_label", sa2_args_node("file_obj", file_obj));
    if (sa2_is_raise(r)) { free(error); return r; }
    char *label = sa2_py_str(r);
    json_free(r);
    char *out = NULL;
    if (strcmp(error, "missing_scope") == 0) {
        const json_t *nn = json_obj_get(response, "needed");
        const json_t *pn = json_obj_get(response, "provided");
        char *nraw = sa2_truthy(nn) ? sa2_py_str(nn) : sa2_strdup("");
        char *praw = sa2_truthy(pn) ? sa2_py_str(pn) : sa2_strdup("");
        char *needed = sa2_strip(nraw), *provided = sa2_strip(praw);
        free(nraw); free(praw);
        sa2_sb_t b; sa2_sb_init(&b);
        sa2_sb_puts(&b, "Slack attachment access failed for ");
        sa2_sb_puts(&b, label);
        sa2_sb_puts(&b, ". ");
        if (needed[0]) {
            sa2_sb_puts(&b, "Missing scope: ");
            sa2_sb_puts(&b, needed);
            sa2_sb_puts(&b, ".");
        } else {
            sa2_sb_puts(&b, "Missing required Slack scope.");
        }
        if (provided[0]) {
            sa2_sb_puts(&b, " Current bot scopes: ");
            sa2_sb_puts(&b, provided);
            sa2_sb_puts(&b, ".");
        }
        sa2_sb_puts(&b, " Update the Slack app scopes/settings and reinstall the app to the workspace.");
        out = sa2_sb_take(&b);
        free(needed); free(provided);
    } else {
        static const char *g1[] = {"not_authed", "invalid_auth", "account_inactive", "token_revoked"};
        static const char *g2[] = {"file_not_found", "file_deleted"};
        static const char *g3[] = {"access_denied", "file_access_denied", "no_permission",
                                   "not_allowed_token_type", "restricted_action"};
        const char *tmpl = NULL;
        for (size_t i = 0; i < 4; i++) if (strcmp(error, g1[i]) == 0)
            tmpl = "Slack attachment access failed for {file_label} because the bot token is not "
                   "authorized ({error}). Refresh the token/reinstall the app.";
        if (!tmpl) for (size_t i = 0; i < 2; i++) if (strcmp(error, g2[i]) == 0)
            tmpl = "Slack attachment {file_label} is no longer available ({error}).";
        if (!tmpl) for (size_t i = 0; i < 5; i++) if (strcmp(error, g3[i]) == 0)
            tmpl = "Slack attachment access failed for {file_label} because the bot does not have "
                   "permission ({error}). Check workspace permissions/scopes and reinstall if needed.";
        if (tmpl) out = sa2_fmt_template(tmpl, label, error);
    }
    free(label); free(error);
    if (!out) return json_null();
    json_t *ret = json_string(out);
    free(out);
    return ret;
}

/* PoP: sa2_warn_if_not_bot_token @ plugins/platforms/slack/adapter.py:_warn_if_not_bot_token */
#define SA2_USER_TOKEN_WARN_MSG \
    "[Slack] The configured Slack token for workspace %s authenticated as a USER " \
    "(member %s), not a bot \xe2\x80\x94 the auth.test response has no 'bot_id'. This is " \
    "almost certainly a user token (xoxp-...) instead of a Bot User OAuth Token " \
    "(xoxb-...). The bot's identity is now bound to that member's ID, so mentions " \
    "OF THAT PERSON will be misrouted as mentions of the bot (the bot replies to " \
    "messages merely addressed to them). Use the 'Bot User OAuth Token' " \
    "(xoxb-...) from your Slack app's 'OAuth & Permissions' page in " \
    "SLACK_BOT_TOKEN."

static json_t *sa2_warn_if_not_bot_token(sa2_ctx_t *ctx, const json_t *args) {
    const json_t *auth = json_obj_get(args, "auth_response");
    const json_t *team_name = json_obj_get(args, "team_name");
    json_t *outer_raise = NULL;
    json_t *warned = sa2_lazy_state(ctx, "_user_token_warned", 1);
    const char *tk = sa2_str(team_name);
    if (!tk) tk = "";
    if (sa2_set_has(warned, tk)) return json_null();
    const json_t *bot_node = NULL, *user_node = NULL;
    if (sa2_is_dict(auth)) {
        bot_node = json_obj_get(auth, "bot_id");
        user_node = json_obj_get(auth, "user_id");
    } else {
        /* .get raises AttributeError -> inner except -> .data fallback */
        const json_t *data = sa2_is_obj(auth) ? json_obj_get(sa2_obj_attrs(auth), "data") : NULL;
        if (!sa2_truthy(data)) data = NULL;
        if (data && !sa2_is_dict(data)) {
            outer_raise = sa2_attr_err(data, "get");
            goto outer_catch;
        }
        if (data) {
            bot_node = json_obj_get(data, "bot_id");
            user_node = json_obj_get(data, "user_id");
        }
    }
    if (!sa2_truthy(user_node)) return json_null();
    if (!sa2_truthy(bot_node)) {
        sa2_set_add(warned, tk);
        json_t *a = json_object();
        json_set(a, "msg", json_string(SA2_USER_TOKEN_WARN_MSG));
        json_t *la = json_array();
        json_append(la, json_string(tk[0] ? tk : "this workspace"));
        json_append(la, json_copy(user_node));
        json_set(a, "args", la);
        json_t *r = sa2_call(ctx, "logger.warning", a);
        if (sa2_is_raise(r)) {
            if (sa2_raise_passes(r)) return r;
            json_free(r);  /* except Exception: diagnostics never break connect */
        } else {
            json_free(r);
        }
    }
    return json_null();
outer_catch:
    if (outer_raise) {
        if (sa2_raise_passes(outer_raise)) return outer_raise;
        json_free(outer_raise);
    }
    return json_null();
}
