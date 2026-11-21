/*
 * context_references_flow2.c — agent/context_references.py FLOW remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/agent/context_references.py
 *
 * CRLF-aware PoP grep (registry pop_claims_full.json + all ~2357 src .c files)
 * BEFORE writing: all 36 AST defs of context_references.py are already claimed
 * by existing PoP sets (context_references.c / _ports.c / _flow.c / _gaps3.c).
 * There are therefore 0 globally-unclaimed defs. Per
 * references/flow2-remainder-scoping.md, when the generic ">=10 UNCLAIMED"
 * floor exceeds the real remainder this file is a STANDALONE whole-module
 * re-derivation of the FLOW remainder: the defs the flow1 oracle
 * (context_references_flow.c) does not cover. Older claims stay in force; the
 * claimed siblings are re-derived here for the standalone differential check.
 *
 *   PoP: cr2_format_reference_value    @ agent/context_references.py:format_reference_value
 *   PoP: cr2_strip_trailing_punctuation @ agent/context_references.py:_strip_trailing_punctuation
 *   PoP: cr2_strip_reference_wrappers   @ agent/context_references.py:_strip_reference_wrappers
 *   PoP: cr2_parse_file_reference_value @ agent/context_references.py:_parse_file_reference_value
 *   PoP: cr2_get_providers              @ agent/context_references.py:get_context_reference_providers
 *   PoP: cr2_is_binary_file             @ agent/context_references.py:_is_binary_file
 *   PoP: cr2_agent_visible_path         @ agent/context_references.py:_agent_visible_path
 *   PoP: cr2_binary_reference_block     @ agent/context_references.py:_binary_reference_block
 *   PoP: cr2_file_metadata              @ agent/context_references.py:_file_metadata
 *   PoP: cr2_register_provider          @ agent/context_references.py:register_context_reference_provider
 *
 * 10 functions (>=10 required). Nested closures of a ported def (publish)
 * travel with their parent; they are not separate claims.
 *
 * Self-contained, libjson-only. Every live effect enters through ONE universal
 * seam: json_t *(*call)(void *ctx, const char *name, const json_t *args).
 * `args` is BORROWED (seam records a copy); cr2_call frees it after the call.
 * NULL == Python None (never a JSON_NULL node); {"__raise__":cls,"msg":msg}
 * == a raised exception. Fail-closed: a MISSING seam -> RuntimeError raise spec
 * (presence test; a scripted null is a valid None).
 *
 * FULL BUILTIN EXCEPTION HIERARCHY: CR2_EXC_TREE below lists every builtin
 * exception class child->parent (plus the quarry LiveConnectionError ->
 * Exception). cr2_cls_is() walks it so `except Exception` folds every Exception
 * child while SystemExit / KeyboardInterrupt / GeneratorExit (BaseException
 * children) propagate. Scripted unknown class names are outside the domain.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__":...} object == a propagated exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include "json.h"

/* ── Python-value helpers ── */
static bool cr2_jtruth(const json_t *v) {
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
static bool cr2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool cr2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool cr2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static const char *cr2_sval(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static double cr2_nval(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static char *cr2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}

/* ── universal seam ── */
typedef struct cr2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} cr2_seams_t;

static json_t *cr2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool cr2_is_raise(const json_t *r) {
    return cr2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *cr2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static const char *cr2_raise_msg(const json_t *r) {
    return json_get_str(r, "msg", "");
}
/* copy a raise spec while the source is still alive (avoid UAF) */
static json_t *cr2_raise_copy(const json_t *r) {
    return cr2_raise(cr2_raise_class(r), cr2_raise_msg(r));
}
/* cr2_call OWNS args (frees after the call); the seam BORROWS args. */
static json_t *cr2_call(const cr2_seams_t *s, const char *name, json_t *args) {
    if (!s || !s->call) {
        json_free(args);
        return cr2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}
static void cr2_set_str(json_t *o, const char *k, const char *v) {
    json_set(o, k, json_string(v ? v : ""));
}
static void cr2_set_node(json_t *o, const char *k, const json_t *v) {
    json_set(o, k, (v && v->type != JSON_NULL) ? json_copy(v) : json_null());
}

/* ── FULL builtin exception hierarchy (child -> parent) ── */
typedef struct cr2_exc_pair { const char *child; const char *parent; } cr2_exc_pair_t;
static const cr2_exc_pair_t CR2_EXC_TREE[] = {
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
    /* quarry exception (hermes_cli.sqlite_safe_read) */
    {"LiveConnectionError", "Exception"},
};
static const char *cr2_exc_parent(const char *c) {
    for (size_t i = 0; i < sizeof(CR2_EXC_TREE) / sizeof(CR2_EXC_TREE[0]); i++)
        if (!strcmp(CR2_EXC_TREE[i].child, c)) return CR2_EXC_TREE[i].parent;
    return NULL;
}
/* isinstance(c, ancestor): walk child -> parent up to BaseException. */
static bool cr2_cls_is(const char *c, const char *ancestor) {
    if (!c || !*c) return false;
    while (c) {
        if (!strcmp(c, ancestor)) return true;
        c = cr2_exc_parent(c);
    }
    return false;
}

/* ── small string helpers ── */
static bool cr2_in_charset(char c, const char *set) {
    return c && strchr(set, c) != NULL;
}
static bool cr2_ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *cr2_lower_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s), b = 0, e = n;
    while (b < e && cr2_ascii_space(s[b])) b++;
    while (e > b && cr2_ascii_space(s[e - 1])) e--;
    char *out = malloc(e - b + 1);
    size_t j = 0;
    for (size_t i = b; i < e; i++) {
        char c = s[i];
        out[j++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[j] = '\0';
    return out;
}
static bool cr2_digits(const char *s, size_t n) {
    if (n == 0) return false;
    for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}
static json_t *cr2_format_reference_value(const json_t *value) {
    const char *s = cr2_sval(value);
    /* _NEEDS_QUOTING = re.compile(r"""[\s()\[\]{}<>"'`]""") */
    bool needs = false;
    for (const char *p = s; *p; p++) {
        if (cr2_ascii_space(*p) || cr2_in_charset(*p, "()[]{}<>\"'`")) { needs = true; break; }
    }
    if (!needs) return json_string(s);
    const char *quotes = "`\"'";
    for (const char *q = quotes; *q; q++) {
        if (!strchr(s, *q)) {
            char *buf = cr2_xsf("%c%s%c", *q, s, *q);
            json_t *r = json_string(buf);
            free(buf);
            return r;
        }
    }
    return json_string(s);
}
static json_t *cr2_strip_trailing_punctuation(const json_t *value) {
    const char *src = cr2_sval(value);
    char *s = strdup(src);
    size_t n = strlen(s);
    while (n > 0 && strchr(".,;!?", s[n - 1])) s[--n] = '\0';
    /* while stripped.endswith((")"," "]","}")) and count(c) > count(opener) */
    const char *closers = ")]}";
    const char *openers = "([{";
    for (;;) {
        if (n == 0) break;
        const char *cp = strchr(closers, s[n - 1]);
        if (!cp) break;
        size_t ci = (size_t)(cp - closers);
        size_t nc = 0, no = 0;
        for (size_t i = 0; i < n; i++) {
            if (s[i] == closers[ci]) nc++;
            if (s[i] == openers[ci]) no++;
        }
        if (nc > no) s[--n] = '\0';
        else break;
    }
    json_t *r = json_string(s);
    free(s);
    return r;
}
static json_t *cr2_strip_reference_wrappers(const json_t *value) {
    const char *s = cr2_sval(value);
    size_t n = strlen(s);
    if (n >= 2 && s[0] == s[n - 1] && strchr("`\"'", s[0])) {
        char *buf = malloc(n - 1);
        memcpy(buf, s + 1, n - 2);
        buf[n - 2] = '\0';
        json_t *r = json_string(buf);
        free(buf);
        return r;
    }
    return json_string(s);
}
static json_t *cr2_parse_file_reference_value(const json_t *value) {
    const char *s = cr2_sval(value);
    size_t n = strlen(s);
    /* Find rightmost ':' whose tail matches \d+(-\d+)? . */
    size_t colon = (size_t)-1;
    long long start = 0, end = 0;
    for (size_t i = n; i > 0; i--) {
        if (s[i - 1] != ':') continue;
        const char *tail = s + i;
        size_t tl = strlen(tail);
        size_t d1 = 0;
        while (d1 < tl && tail[d1] >= '0' && tail[d1] <= '9') d1++;
        if (d1 == 0) continue;
        long long st = 0;
        for (size_t k = 0; k < d1; k++) st = st * 10 + (tail[k] - '0');
        long long en = st;
        if (d1 < tl && tail[d1] == '-') {
            size_t d2 = 0;
            while (d1 + 1 + d2 < tl && tail[d1 + 1 + d2] >= '0' && tail[d1 + 1 + d2] <= '9') d2++;
            if (d2 == 0 || d1 + 1 + d2 != tl) continue;
            en = 0;
            for (size_t k = 0; k < d2; k++) en = en * 10 + (tail[d1 + 1 + k] - '0');
        } else if (d1 != tl) {
            continue;
        }
        colon = i - 1;
        start = st; end = en;
        break;
    }
    json_t *out = json_array();
    json_t *path;
    if (colon == (size_t)-1) {
        path = cr2_strip_reference_wrappers(value);
        json_append(out, path);
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    if (colon == 0) { /* no path -> no regex match -> strip_wrappers(whole) */
        json_free(cr2_strip_reference_wrappers(value)); /* discard; keep behaviour */
        path = cr2_strip_reference_wrappers(value);
        json_append(out, path);
        json_append(out, json_null());
        json_append(out, json_null());
        return out;
    }
    const char *prefix = s;
    size_t pl = colon;
    if (pl >= 2 && strchr("`\"'", prefix[0]) && prefix[0] == prefix[pl - 1]) {
        char *buf = malloc(pl - 1);
        memcpy(buf, prefix + 1, pl - 2);
        buf[pl - 2] = '\0';
        path = json_string(buf);
        free(buf);
    } else {
        char *buf = malloc(pl + 1);
        memcpy(buf, prefix, pl);
        buf[pl] = '\0';
        path = json_string(buf);
        free(buf);
    }
    json_append(out, path);
    json_append(out, json_number((double)start));
    json_append(out, json_number((double)end));
    return out;
}
static json_t *cr2_get_providers(const json_t *providers) {
    json_t *o = json_object();
    if (cr2_is_obj(providers)) {
        for (size_t i = 0; i < providers->c.count; i++)
            json_set(o, providers->c.keys[i], json_copy(providers->c.items[i]));
    }
    return o;
}
static json_t *cr2_is_binary_file(cr2_seams_t *sp, const char *name, const char *path) {
    json_t *a = json_object();
    cr2_set_str(a, "name", name);
    json_t *mime = cr2_call(sp, "guess_type", a);
    if (cr2_is_raise(mime)) return mime;
    const char *m = (mime && mime->type == JSON_STRING) ? mime->str_val : NULL;
    bool by_mime = false;
    if (m && m[0]) {
        bool text = strncmp(m, "text/", 5) == 0;
        bool txt_ext = false;
        static const char *EXTS[] = {".py",".md",".txt",".json",".yaml",".yml",".toml",".js",".ts",NULL};
        for (int i = 0; EXTS[i]; i++) {
            size_t el = strlen(EXTS[i]), nl = strlen(name);
            if (nl >= el && strcmp(name + nl - el, EXTS[i]) == 0) { txt_ext = true; break; }
        }
        by_mime = !text && !txt_ext;
    }
    json_free(mime);
    if (by_mime) return json_bool(true);
    json_t *b = json_object();
    cr2_set_str(b, "path", path);
    json_t *sample = cr2_call(sp, "sniff", b);
    if (cr2_is_raise(sample)) return sample;
    bool has_nul = false;
    if (cr2_is_arr(sample)) {
        for (size_t i = 0; i < sample->c.count; i++)
            if (cr2_nval(json_get(sample, i)) == 0.0) { has_nul = true; break; }
    }
    json_free(sample);
    return json_bool(has_nul);
}
static json_t *cr2_agent_visible_path(cr2_seams_t *sp, const char *path) {
    json_t *a = json_object();
    json_t *r = cr2_call(sp, "ensure_bridged", a);
    if (cr2_is_raise(r)) {
        bool fold = cr2_cls_is(cr2_raise_class(r), "Exception");
        if (fold) { json_free(r); return json_string(path); }
        json_t *out = cr2_raise_copy(r);
        json_free(r);
        return out;
    }
    json_free(r);
    json_t *b = json_object();
    cr2_set_str(b, "path", path);
    json_t *c = cr2_call(sp, "to_cache", b);
    if (cr2_is_raise(c)) {
        bool fold = cr2_cls_is(cr2_raise_class(c), "Exception");
        if (fold) { json_free(c); return json_string(path); }
        json_t *out = cr2_raise_copy(c);
        json_free(c);
        return out;
    }
    if (!c || c->type == JSON_NULL) { json_free(c); return json_string("None"); }
    return c; /* string */
}
static json_t *cr2_binary_reference_block(cr2_seams_t *sp, const char *raw,
                                          const char *name) {
    json_t *a = json_object();
    cr2_set_str(a, "name", name);
    json_t *mime = cr2_call(sp, "guess_type", a);
    if (cr2_is_raise(mime)) return mime;
    const char *m = (mime && mime->type == JSON_STRING && mime->str_val[0]) ? mime->str_val
                                                                            : "application/octet-stream";
    char *desc = strdup(m);
    json_free(mime);
    json_t *b = json_object();
    cr2_set_str(b, "raw", raw);
    cr2_set_str(b, "descriptor", desc);
    cr2_set_str(b, "reason", "binary file, not inlined as text.");
    cr2_set_str(b, "guidance",
        "Use your tools to work with it (read or convert it, extract its text, "
        "or view/render it as needed); do not tell the user the file type is unsupported.");
    free(desc);
    return cr2_call(sp, "on_disk", b);
}
static json_t *cr2_file_metadata(cr2_seams_t *sp, const char *path) {
    /* try: size = path.stat().st_size  except OSError: return "unknown size" */
    json_t *a = json_object();
    cr2_set_str(a, "path", path);
    json_t *st = cr2_call(sp, "stat", a);
    if (cr2_is_raise(st)) {
        bool fold = cr2_cls_is(cr2_raise_class(st), "OSError");
        if (fold) { json_free(st); return json_string("unknown size"); }
        json_t *out = cr2_raise_copy(st);
        json_free(st);
        return out;
    }
    long long size = (long long)cr2_nval(json_obj_get(st, "size"));
    json_free(st);
    char *sbytes = cr2_xsf("%lld bytes", size);
    /* try: with offline_file_access(...): ... except (LiveConnectionError, OSError) */
    json_t *c = json_object();
    cr2_set_str(c, "path", path);
    cr2_set_str(c, "what", "inspect folder entry");
    json_t *enter = cr2_call(sp, "cm_enter", c);
    if (cr2_is_raise(enter)) {
        bool fold = cr2_cls_is(cr2_raise_class(enter), "LiveConnectionError") ||
                    cr2_cls_is(cr2_raise_class(enter), "OSError");
        if (fold) { json_free(enter); json_t *r = json_string(sbytes); free(sbytes); return r; }
        json_t *out = cr2_raise_copy(enter);
        json_free(enter);
        free(sbytes);
        return out;
    }
    json_free(enter);
    /* if _is_binary_file(path) or size > 4MiB: return f"{size} bytes" */
    json_t *d = json_object();
    cr2_set_str(d, "path", path);
    json_t *bin = cr2_call(sp, "is_binary", d);
    if (cr2_is_raise(bin)) {
        bool fold = cr2_cls_is(cr2_raise_class(bin), "LiveConnectionError") ||
                    cr2_cls_is(cr2_raise_class(bin), "OSError");
        if (fold) { json_free(bin); json_t *r = json_string(sbytes); free(sbytes); return r; }
        json_t *out = cr2_raise_copy(bin);
        json_free(bin);
        free(sbytes);
        return out;
    }
    bool isbin = cr2_jtruth(bin);
    json_free(bin);
    if (isbin || size > 4LL * 1024 * 1024) {
        json_t *r = json_string(sbytes);
        free(sbytes);
        return r;
    }
    /* lines = sum(chunk.count(b"\n") for chunk in ...) */
    json_t *e = json_object();
    cr2_set_str(e, "path", path);
    json_t *chunks = cr2_call(sp, "read", e);
    if (cr2_is_raise(chunks)) {
        bool fold = cr2_cls_is(cr2_raise_class(chunks), "LiveConnectionError") ||
                    cr2_cls_is(cr2_raise_class(chunks), "OSError");
        if (fold) { json_free(chunks); json_t *r = json_string(sbytes); free(sbytes); return r; }
        json_t *out = cr2_raise_copy(chunks);
        json_free(chunks);
        free(sbytes);
        return out;
    }
    long long lines = 0;
    if (chunks) {
        if (chunks->type == JSON_STRING) {
            for (const char *p = chunks->str_val; p && *p; p++) if (*p == '\n') lines++;
        } else if (cr2_is_arr(chunks)) {
            for (size_t i = 0; i < chunks->c.count; i++) {
                const char *ch = cr2_sval(json_get(chunks, i));
                for (const char *p = ch; *p; p++) if (*p == '\n') lines++;
            }
        }
    }
    json_free(chunks);
    free(sbytes);
    char *lbuf = cr2_xsf("%lld lines", lines + 1);
    json_t *lr = json_string(lbuf);
    free(lbuf);
    return lr;
}
/* state: {"providers": {prefix: {"id":int,...}}, "owners": {prefix: owner|null}} */
static json_t *cr2_register_provider(json_t *state, const char *raw_prefix,
                                     bool is_provider, const json_t *owner,
                                     bool reclaim, bool do_publish,
                                     bool publish_changed) {
    json_t *providers = json_obj_get(state, "providers");
    json_t *owners = json_obj_get(state, "owners");
    if (!is_provider)
        return cr2_raise("TypeError", "provider must be a ContextReferenceProvider instance");
    char *prefix = cr2_lower_strip(raw_prefix);
    if (!prefix[0]) { free(prefix); return cr2_raise("ValueError", "prefix must be a non-empty string"); }
    static const char *BUILTIN[] = {"diff","staged","file","folder","git","url",NULL};
    for (int i = 0; BUILTIN[i]; i++) {
        if (!strcmp(prefix, BUILTIN[i])) {
            char *m = cr2_xsf("prefix '%s' is reserved for built-in references", prefix);
            free(prefix);
            json_t *r = cr2_raise("ValueError", m);
            free(m);
            return r;
        }
    }
    const json_t *held = json_obj_get(providers, prefix);
    bool held_present = held != NULL;
    const json_t *held_owner = json_obj_get(owners, prefix);
    if (!held_present) {
        json_set(providers, prefix, json_object());
        json_set(owners, prefix, owner ? json_copy(owner) : json_null());
        free(prefix);
        return NULL; /* publish() returned None -> no closure */
    }
    bool owner_same = false;
    const char *hov = cr2_sval(held_owner);
    const char *ov = cr2_sval(owner);
    owner_same = !strcmp(hov, ov);
    if (!reclaim || !owner || !cr2_jtruth(owner) || !owner_same) {
        char *m = cr2_xsf("prefix '%s' is already registered", prefix);
        free(prefix);
        json_t *r = cr2_raise("ValueError", m);
        free(m);
        return r;
    }
    /* publish closure */
    json_t *ret = NULL;
    if (do_publish) {
        bool ok = !publish_changed;
        if (ok) json_set(providers, prefix, json_object());
        ret = json_bool(ok);
    }
    (void)held;
    free(prefix);
    return ret;
}
