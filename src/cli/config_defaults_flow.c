/*
 * config_defaults_flow.c — FLOW port of the hermes_cli config-defaults logic (C11).
 *
 * MODULE RESOLUTION (the named target is a pure-data leaf):
 *   hermes_cli/config_defaults.py defines ZERO functions — it is the pure-data
 *   DEFAULT_CONFIG / OPTIONAL_ENV_VARS table, already ported 1:1 as canonical JSON
 *   in src/hermes_cli/config_defaults.c. The config-defaults LOGIC lives in the two
 *   sibling factory modules it imports, which are the real defining modules credited
 *   by every PoP below (grep PoP first, CRLF-aware):
 *     hermes_cli/config_defaults_auxiliary.py : _aux
 *     hermes_cli/config_defaults_env.py       : _env, _base_url, _category.make,
 *                                               OptionalEnvVars.{arm,_ready,
 *                                               __getitem__,get,__contains__,
 *                                               __iter__,__len__,keys,items,
 *                                               values,copy}
 *   _category itself is CLAIMED (src/hermes_cli/learning_graph_helpers.c:
 *   learning_graph_category @ hermes_cli/config_defaults_env.py:_category) and is
 *   SEAMED here, never re-ported: _base_url calls the module constant _prov, which
 *   is the claimed _category("provider", password=True, advanced=True) closure.
 *   The nested closure body `make` is a distinct, UNCLAIMED def and is ported real
 *   (cd2_make) with its free variables (category/password/advanced/_OMIT) supplied
 *   as inputs. 15 unclaimed functions total.
 *
 * Self-contained, libjson-only. Every live external effect enters through ONE
 * universal seam; the cores perform NO I/O:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON object
 * (the seam records a copy and must not free it); the result is a NEW json_t* the
 * caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's recording
 * stubs emit the identical shape driving LIVE Python with the same scripts.
 *
 * There are NO except clauses in any ported def (_ready uses try/FINALLY only), so
 * every exception — BaseException trio included — PASSES THROUGH verbatim. The FULL
 * builtin exception hierarchy is still carried (cd2_cls_is) for raise-spec class
 * folding parity. With no seam installed the dispatcher is fail-closed (RuntimeError
 * raise spec); the harness script_call is ALSO fail-closed: a MISSING script returns
 * a RuntimeError raise spec (a scripted null is a valid None result).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. The dict
 * methods additionally mutate a cd2_env_state_t (the OptionalEnvVars instance) which
 * the harness projects into the envelope's "state" key.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   fill       self._fill()                 args{}            ret entries-dict|raise
 *              (the armed fill callable; runs holding no lock)
 *   published  self._published(entries)     args{entries}     ret null|raise
 *              (the publish callback; runs under _FILL_LOCK after the entries merge)
 *   prov       _prov(description, prompt, url=None, password=False)
 *              args{description, prompt, url, password}       ret entry-dict|raise
 *              (CLAIMED _category("provider",...) closure _prov; seamed, never
 *               re-ported. url=None projects out of the trace; password=False kept.)
 *
 * Representation contract (fixture domain, identical on both sides):
 *  - ASCII text; POSIX lexical paths; integral JSON numbers read as int.
 *  - OptionalEnvVars instance state projects to
 *    {data:<ordered dict contents>, fill:<marker|null>, published:<marker|null>,
 *     filling_active:<bool>}. fill/published are callables in live Python, projected
 *    to their fixture marker strings (NULL == None). _FILLING.active is the module
 *    threading.local flag, modelled as the filling_active bool. _FILL_LOCK is inert
 *    in the single-threaded fixture domain (always acquirable; no seam).
 *  - the `self._fill is not fill` identity guard always passes in the domain (no
 *    concurrent reader mutates _fill between capture and the lock block); it is
 *    ported faithfully (pointer identity) but never short-circuits here.
 *  - fill() success results are dicts; a NON-dict entries value raises AttributeError
 *    on `entries.items()` (py3.14 message "'<type>' object has no attribute 'items'"),
 *    leaving the table armed and unpublished. fill() may also raise (propagates; the
 *    finally still resets filling_active=False; nothing publishes).
 *  - __getitem__ on a missing key raises KeyError(key); str(KeyError) is repr(key),
 *    modelled as '<key>' for the simple-ASCII (no quote/backslash) key domain.
 *  - make: ABSENT url/help/tools == the _OMIT sentinel (key omitted); a PRESENT null
 *    is written verbatim. password/advanced ABSENT take the closure's bound default;
 *    PRESENT (incl null) overrides. `if password is not None` writes the key only for
 *    a non-None effective password; `if advanced` writes advanced=True only when the
 *    effective advanced is truthy. Key order: description, prompt, [help], [url],
 *    [tools], [password], category, [advanced].
 *  - _aux: reasoning_effort ABSENT defaults True (truthy -> writes ""); PRESENT uses
 *    its truthiness (null/false/0/"" omit). extra keys appended after the standard ones.
 *  - view-returning methods (__iter__/keys/items/values) project to ordered lists
 *    (items -> [key, value] pairs); copy projects to a plain dict; __len__ to an int.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool cd2_jtruth(const json_t *v) {
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
static bool cd2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static char *cd2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
/* Python repr() of a str (ASCII domain): single quotes unless the value contains
 * a single quote and no double quote, then double quotes. Escape backslash, the
 * chosen quote, \n \r \t and other control chars as \xNN. */
static char *cd2_repr_str(const char *s) {
    const char *p = s ? s : "";
    bool has_sq = strchr(p, '\'') != NULL;
    bool has_dq = strchr(p, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(p);
    char *out = malloc(n * 4 + 3);
    if (!out) return strdup("");
    size_t k = 0;
    out[k++] = q;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == (unsigned char)q || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else if (c < 0x20 || c == 0x7f) k += (size_t)sprintf(out + k, "\\x%02x", c);
        else out[k++] = (char)c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}
/* Python type name of a JSON value (for AttributeError messages). libjson has no
 * int/float identity: an integral number projects to "int", else "float". */
static const char *cd2_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL:   return "NoneType";
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* ── universal seam ── */
typedef struct cd2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cd2_seams_t;

static json_t *cd2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool cd2_is_raise(const json_t *r) {
    return cd2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *cd2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: cd2_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *cd2_call(const cd2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return cd2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static json_t *cd2_args0(void) { return json_object(); }
static void cd2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
/* set a key to a copy of v, or an explicit JSON null when v is NULL/JSON_NULL.
 * (json_set(obj,key,NULL) is a no-op in libjson — never pass NULL.) */
static void cd2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) for isinstance-style
 *    catch folding. No ported def has an except clause (_ready is try/finally),
 *    so every raise passes through; the full table is carried for raise-spec
 *    class folding parity. ExceptionGroup's second base (Exception) is modelled
 *    by its primary BaseExceptionGroup parent; outside the fixture domain. ── */
typedef struct cd2_exc_pair { const char *child; const char *parent; } cd2_exc_pair_t;
static const cd2_exc_pair_t CD2_EXC_TREE[] = {
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"},
    {"GeneratorExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"SystemExit", "BaseException"},
    {"Exception", "BaseException"},
    {"ArithmeticError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"ModuleNotFoundError", "ImportError"},
    {"LookupError", "Exception"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"UnboundLocalError", "NameError"},
    {"OSError", "Exception"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"Warning", "Exception"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
};
static const char *cd2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(CD2_EXC_TREE) / sizeof(CD2_EXC_TREE[0]); i++)
        if (!strcmp(CD2_EXC_TREE[i].child, c)) return CD2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool cd2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = cd2_exc_parent(c);
    }
    return false;
}

/* ── OptionalEnvVars instance state (the dict subclass + module thread-local) ── */
typedef struct cd2_env_state {
    json_t *data;            /* dict contents (ordered object); owned by the harness */
    const char *fill;        /* self._fill marker, NULL == None (borrowed from input) */
    const char *published;   /* self._published marker, NULL == None */
    bool filling_active;     /* _FILLING.active (module threading.local) */
} cd2_env_state_t;

/* ═══════════════════════ pure entry factories ═══════════════════════ */

/* PoP: cd2_aux @ hermes_cli/config_defaults_auxiliary.py:_aux */
json_t *cd2_aux(const json_t *timeout, const json_t *reasoning_effort, const json_t *extra) {
    /* d = {"provider":"auto","model":"","base_url":"","api_key":"","timeout":timeout,
     *      "extra_body":{}}
     * if reasoning_effort: d["reasoning_effort"]=""   (default True; ABSENT -> True)
     * d.update(extra) */
    json_t *d = json_object();
    cd2_set_str(d, "provider", "auto");
    cd2_set_str(d, "model", "");
    cd2_set_str(d, "base_url", "");
    cd2_set_str(d, "api_key", "");
    cd2_set_node(d, "timeout", timeout);
    json_set(d, "extra_body", json_object());
    bool re = reasoning_effort ? cd2_jtruth(reasoning_effort) : true;
    if (re) cd2_set_str(d, "reasoning_effort", "");
    if (cd2_is_obj(extra))
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(d, extra->c.keys[i], json_copy(extra->c.items[i]));
    return d;
}

/* PoP: cd2_env @ hermes_cli/config_defaults_env.py:_env */
json_t *cd2_env(const json_t *description, const json_t *prompt, const json_t *keys) {
    /* return {"description": description, "prompt": prompt, **keys} */
    json_t *d = json_object();
    cd2_set_node(d, "description", description);
    cd2_set_node(d, "prompt", prompt);
    if (cd2_is_obj(keys))
        for (size_t i = 0; i < keys->c.count; i++)
            json_set(d, keys->c.keys[i], json_copy(keys->c.items[i]));
    return d;
}

/* PoP: cd2_make @ hermes_cli/config_defaults_env.py:_category.make
 * The nested closure body; free variables category/password/advanced (the bound
 * defaults) arrive in `bound`, the call args in `call`. ABSENT url/help/tools ==
 * _OMIT (omitted); PRESENT (incl null) written verbatim. password/advanced ABSENT
 * take the bound default; PRESENT overrides. */
json_t *cd2_make(const json_t *bound, const json_t *call) {
    json_t *d = json_object();
    cd2_set_node(d, "description", json_obj_get(call, "description"));
    cd2_set_node(d, "prompt", json_obj_get(call, "prompt"));
    /* d.update((k,v) for k,v in (("help",help),("url",url),("tools",tools))
     *          if v is not _OMIT)  — PRESENT (json_obj_get != NULL) == not _OMIT */
    static const char *optkeys[] = {"help", "url", "tools"};
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = json_obj_get(call, optkeys[i]);
        if (v) json_set(d, optkeys[i], json_copy(v));
    }
    /* effective password = call.password if PRESENT else bound.password */
    const json_t *pw = json_obj_get(call, "password");
    if (!pw) pw = json_obj_get(bound, "password");
    if (pw && pw->type != JSON_NULL) json_set(d, "password", json_copy(pw));
    /* d["category"] = category */
    cd2_set_node(d, "category", json_obj_get(bound, "category"));
    /* effective advanced = call.advanced if PRESENT else bound.advanced */
    const json_t *adv = json_obj_get(call, "advanced");
    if (!adv) adv = json_obj_get(bound, "advanced");
    if (cd2_jtruth(adv)) json_set(d, "advanced", json_bool(true));
    return d;
}

/* PoP: cd2_base_url @ hermes_cli/config_defaults_env.py:_base_url */
json_t *cd2_base_url(const cd2_seams_t *s, const char *name, const char *prompt_name) {
    /* prompt = f"{prompt_name or name} base URL (leave empty for default)"
     * return _prov(f"{name} base URL override", prompt, None, password=False)
     * _prov is the CLAIMED _category("provider",...) closure -> seam "prov". */
    const char *nm = name ? name : "";
    const char *base = (prompt_name && prompt_name[0]) ? prompt_name : nm;
    char *prompt = cd2_xsf("%s base URL (leave empty for default)", base);
    char *desc = cd2_xsf("%s base URL override", nm);
    json_t *args = json_object();
    cd2_set_str(args, "description", desc);
    cd2_set_str(args, "prompt", prompt);
    cd2_set_node(args, "url", NULL);            /* url=None (projects out of trace) */
    json_set(args, "password", json_bool(false));
    json_t *r = cd2_call(s, "prov", args);
    free(prompt); free(desc);
    return r;   /* verbatim seam result: entry-dict | raise | NULL(None) */
}

/* ═══════════════════════ OptionalEnvVars methods ═══════════════════════ */

/* PoP: cd2_arm @ hermes_cli/config_defaults_env.py:OptionalEnvVars.arm */
json_t *cd2_arm(cd2_env_state_t *st, const char *fill, const char *published) {
    /* self._fill, self._published = fill, published ; returns None. No seam. */
    st->fill = fill;
    st->published = published;
    return NULL;
}

/* PoP: cd2_ready @ hermes_cli/config_defaults_env.py:OptionalEnvVars._ready */
json_t *cd2_ready(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* fill = self._fill
     * if fill is None or getattr(_FILLING,"active",False): return
     * _FILLING.active = True
     * try: entries = fill()
     * finally: _FILLING.active = False
     * with _FILL_LOCK:                       (inert single-threaded)
     *     if self._fill is not fill: return  (identity guard; domain always passes)
     *     for key,value in entries.items(): dict.__setitem__(self,key,value)
     *     self._fill = None
     *     if self._published is not None: self._published(entries) */
    const char *fill0 = st->fill;
    if (!fill0 || st->filling_active) return NULL;   /* early return: no state change */
    st->filling_active = true;
    json_t *entries = cd2_call(s, "fill", cd2_args0());
    st->filling_active = false;                      /* finally: runs even on raise */
    if (cd2_is_raise(entries)) return entries;       /* propagate; lock block skipped */
    /* with _FILL_LOCK */
    if (st->fill != fill0) { json_free(entries); return NULL; }  /* another reader published */
    if (!cd2_is_obj(entries)) {
        /* entries.items() on a non-dict raises AttributeError; table stays armed */
        char *m = cd2_xsf("'%s' object has no attribute 'items'", cd2_type_name(entries));
        json_t *r = cd2_raise("AttributeError", m);
        free(m);
        json_free(entries);
        return r;
    }
    for (size_t i = 0; i < entries->c.count; i++)
        json_set(st->data, entries->c.keys[i], json_copy(entries->c.items[i]));
    st->fill = NULL;                                 /* self._fill = None */
    if (st->published) {
        json_t *args = json_object();
        cd2_set_node(args, "entries", entries);
        json_t *pr = cd2_call(s, "published", args);
        if (cd2_is_raise(pr)) { json_free(entries); return pr; }
        json_free(pr);
    }
    json_free(entries);
    return NULL;
}

/* PoP: cd2_getitem @ hermes_cli/config_defaults_env.py:OptionalEnvVars.__getitem__ */
json_t *cd2_getitem(const cd2_seams_t *s, cd2_env_state_t *st, const char *key) {
    /* self._ready(); return dict.__getitem__(self, key)  (KeyError on missing) */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    const json_t *v = json_obj_get(st->data, key);
    if (!v) {
        char *m = cd2_repr_str(key ? key : "");   /* str(KeyError(key)) == repr(key) */
        json_t *e = cd2_raise("KeyError", m);
        free(m);
        return e;
    }
    return (v->type == JSON_NULL) ? NULL : json_copy(v);
}

/* PoP: cd2_get @ hermes_cli/config_defaults_env.py:OptionalEnvVars.get */
json_t *cd2_get(const cd2_seams_t *s, cd2_env_state_t *st, const char *key,
                const json_t *default_node) {
    /* self._ready(); return dict.get(self, key, default)  (default None) */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    const json_t *v = json_obj_get(st->data, key);
    if (v) return (v->type == JSON_NULL) ? NULL : json_copy(v);
    return (default_node && default_node->type != JSON_NULL) ? json_copy(default_node) : NULL;
}

/* PoP: cd2_contains @ hermes_cli/config_defaults_env.py:OptionalEnvVars.__contains__ */
json_t *cd2_contains(const cd2_seams_t *s, cd2_env_state_t *st, const char *key) {
    /* self._ready(); return dict.__contains__(self, key) */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    return json_bool(json_obj_get(st->data, key) != NULL);
}

/* PoP: cd2_iter @ hermes_cli/config_defaults_env.py:OptionalEnvVars.__iter__ */
json_t *cd2_iter(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict.__iter__(self)  -> projected to ordered key list */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    json_t *a = json_array();
    if (cd2_is_obj(st->data))
        for (size_t i = 0; i < st->data->c.count; i++)
            json_append(a, json_string(st->data->c.keys[i]));
    return a;
}

/* PoP: cd2_len @ hermes_cli/config_defaults_env.py:OptionalEnvVars.__len__ */
json_t *cd2_len(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict.__len__(self) */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    size_t n = cd2_is_obj(st->data) ? st->data->c.count : 0;
    return json_number((double)n);
}

/* PoP: cd2_keys @ hermes_cli/config_defaults_env.py:OptionalEnvVars.keys */
json_t *cd2_keys(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict.keys(self)  -> projected to ordered key list */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    json_t *a = json_array();
    if (cd2_is_obj(st->data))
        for (size_t i = 0; i < st->data->c.count; i++)
            json_append(a, json_string(st->data->c.keys[i]));
    return a;
}

/* PoP: cd2_items @ hermes_cli/config_defaults_env.py:OptionalEnvVars.items */
json_t *cd2_items(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict.items(self)  -> projected to [key, value] pairs */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    json_t *a = json_array();
    if (cd2_is_obj(st->data))
        for (size_t i = 0; i < st->data->c.count; i++) {
            json_t *pair = json_array();
            json_append(pair, json_string(st->data->c.keys[i]));
            json_append(pair, json_copy(st->data->c.items[i]));
            json_append(a, pair);
        }
    return a;
}

/* PoP: cd2_values @ hermes_cli/config_defaults_env.py:OptionalEnvVars.values */
json_t *cd2_values(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict.values(self)  -> projected to ordered value list */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    json_t *a = json_array();
    if (cd2_is_obj(st->data))
        for (size_t i = 0; i < st->data->c.count; i++)
            json_append(a, json_copy(st->data->c.items[i]));
    return a;
}

/* PoP: cd2_copy @ hermes_cli/config_defaults_env.py:OptionalEnvVars.copy */
json_t *cd2_copy(const cd2_seams_t *s, cd2_env_state_t *st) {
    /* self._ready(); return dict(dict.items(self))  -> a plain dict copy */
    json_t *r = cd2_ready(s, st);
    if (cd2_is_raise(r)) return r;
    return cd2_is_obj(st->data) ? json_copy(st->data) : json_object();
}