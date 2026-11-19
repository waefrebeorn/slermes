/*
 * pm_packages_flow.c -- FLOW cores + leaves of pm/packages.py (hermes-agent):
 * the package-definition decision chains (BinaryPackage verify/probe/binary/
 * stage/_rel/_probe_env, the per-class fetch_url/latest_versions resolvers,
 * Nodejs.repair_staged_verification, the bionic/Dmgbuild verify dispatches)
 * and the module-level env/cache helpers (npm_env, pnpm_env, uv_cache_path,
 * _github_release_digests).
 *
 * Decision sequences only: every live I/O / foreign call (subprocess.run,
 * native, current_target, flatten_single_dir, _binary_reason, _probe_reason,
 * the pm.update index seams, os.environ, env_for, get_default_hermes_root,
 * _get_json, try_install_libatomic, read_bytes, and the cross-class verify
 * dispatches super().verify / DebPackage.verify / BinaryPackage.verify /
 * self.verify / self.missing_reason / Package.fetch_url) enters through ONE
 * universal seam: pp_call(ctx, name, args). The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle
 * installs recording stubs emitting the identical event shape
 * {"name", "args", "ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and pp_-prefixed.
 *
 * Representation contract (shared with tests/oracle_pm_packages_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)} (OSError adds
 *   errno, message "[Errno N] msg"; KeyError message is repr(key)).
 * - Paths are POSIX strings; the oracle drives PurePosixPath doubles and
 *   projects with as_posix(), so str(Path) never flips slashes.
 * - read_bytes scripts are latin-1 byte tokens (strings): equality of the
 *   tokens == equality of the bytes.
 * - self is a JSON object of class attributes (the oracle materializes a
 *   types.SimpleNamespace); a missing attribute is the py3.14
 *   "'types.SimpleNamespace' object has no attribute 'x'" AttributeError.
 * - Cross-class method dispatch (super().verify, DebPackage.verify(self,..),
 *   BinaryPackage.verify(self,..), Nodejs self.verify, Ffmpeg
 *   self.missing_reason, WhisperCppCpu self.fetch_url=Package.fetch_url) is
 *   SEAMED; within-class self calls between ported methods compose REAL
 *   (binary->_rel, verify->binary/_probe_env, fetch_url->fetch_urls->
 *   _asset_names).
 * - Plain JSON numbers are ints (integral doubles format %.0f); bools are
 *   bools. int()-parse and type-name slots see the fixture corpus only:
 *   ASCII strings, ints, bools, null, lists, dicts.
 * - str.format domain: templates carry only {toolkit} holes (LlamaCppCuda).
 * - dict(os.environ) scripts: object or null (null -> the real TypeError).
 * - InstallError messages carry the live em dash as \xe2\x80\x94 (ASCII source).
 * - BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 *   CancelledError) pass through every except Exception / except OSError
 *   site; fail-closed: a missing/exhausted script is a recorded
 *   RuntimeError("no script for X" / "script exhausted for X") raise.
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

/* -- universal seam context -- */
typedef struct pp_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} pp_ctx_t;

/* -- basic helpers -- */
static bool pp_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *pp_raise_kind(const json_t *v) {
    return pp_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool pp_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static const char *pp_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool pp_str_eq(const json_t *v, const char *s) {
    const char *t = pp_str(v);
    return t && s && strcmp(t, s) == 0;
}

static bool pp_is_null_node(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static bool pp_is_base_exc(const char *t) {
    return t && (strcmp(t, "SystemExit") == 0 || strcmp(t, "KeyboardInterrupt") == 0 ||
                 strcmp(t, "GeneratorExit") == 0 || strcmp(t, "CancelledError") == 0);
}

static bool pp_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "IOError", "TimeoutError", "PermissionError",
        "FileNotFoundError", "FileExistsError", "BlockingIOError",
        "ConnectionError", "ConnectionResetError", "BrokenPipeError",
        "InterruptedError", "NotADirectoryError", "IsADirectoryError",
        "ChildProcessError", "ProcessLookupError",
    };
    if (!t) return false;
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

static json_t *pp_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static char *pp_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* printf-style malloc'd format (NULL on OOM). */
static char *pp_xasprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return NULL; }
    char *o = malloc((size_t)n + 1);
    if (o) vsnprintf(o, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return o;
}

/* Python str repr on the ASCII fixture corpus: quote selection + escapes. */
static char *pp_repr_str(const char *s) {
    if (!s) return pp_strdup("None");
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(s) * 4 + 3, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    o[len++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { o[len++] = '\\'; o[len++] = (char)c; }
        else if (c == '\n') { o[len++] = '\\'; o[len++] = 'n'; }
        else if (c == '\r') { o[len++] = '\\'; o[len++] = 'r'; }
        else if (c == '\t') { o[len++] = '\\'; o[len++] = 't'; }
        else if (c < 0x20 || c == 0x7f) { len += (size_t)sprintf(o + len, "\\x%02x", c); }
        else o[len++] = (char)c;
    }
    o[len++] = q;
    o[len] = '\0';
    return o;
}

/* Type name for error messages; integral JSON numbers read as int. */
static const char *pp_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static json_t *pp_attr_err(const json_t *v, const char *attr) {
    char *m = pp_xasprintf("'%s' object has no attribute '%s'", pp_type_name(v), attr);
    json_t *e = pp_err("AttributeError", m ? m : "");
    free(m);
    return e;
}

static json_t *pp_type_err(const char *msg) { return pp_err("TypeError", msg); }

static json_t *pp_not_iterable(const json_t *v) {
    char *m = pp_xasprintf("'%s' object is not iterable", pp_type_name(v));
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

static json_t *pp_key_error(const json_t *key) {
    char *r = pp_repr_str(pp_str(key));
    if (!key || key->type != JSON_STRING) {
        /* non-string keys: None -> None, numbers/bools via str projection */
        free(r);
        const char *s = pp_str(key);
        if (s) r = pp_repr_str(s);
        else if (!key || key->type == JSON_NULL) r = pp_strdup("None");
        else if (key->type == JSON_BOOL) r = pp_strdup(key->bool_val ? "True" : "False");
        else if (key->type == JSON_NUMBER) {
            char buf[64];
            snprintf(buf, sizeof buf, "%.0f", key->num_val);
            r = pp_strdup(buf);
        } else r = pp_strdup("None");
    }
    json_t *e = pp_err("KeyError", r ? r : "");
    free(r);
    return e;
}

/* str() projection for f-strings on the fixture corpus (str/int/bool/None). */
static char *pp_str_of(const json_t *v) {
    if (!v || v->type == JSON_NULL) return pp_strdup("None");
    if (v->type == JSON_STRING) return pp_strdup(v->str_val);
    if (v->type == JSON_BOOL) return pp_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return pp_strdup(buf);
    }
    return pp_strdup(""); /* containers excluded from str()-slot fixtures */
}

static json_t *pp_install_error(const char *package, const char *cause, const char *remedy) {
    const char *rem = (remedy && remedy[0]) ? remedy : "retry, or run `hermes pm doctor`";
    char *m = pp_xasprintf("%s: %s \xe2\x80\x94 %s", package ? package : "", cause ? cause : "", rem);
    json_t *e = pp_err("InstallError", m ? m : "");
    free(m);
    return e;
}

#include <stdarg.h>

/* -- string helpers (Python str semantics on the ASCII fixture corpus) -- */
static bool pp_startswith(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}

static bool pp_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t n = strlen(s), m = strlen(suf);
    return m <= n && strcmp(s + n - m, suf) == 0;
}

static char *pp_lower(const char *s) {
    char *o = pp_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

static char *pp_upper(const char *s) {
    char *o = pp_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}

static char *pp_replace_all(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0) return pp_strdup(s); /* corpus never replaces "" */
    size_t cap = strlen(s) + 1, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = s;
    while (*p) {
        if (strncmp(p, from, fl) == 0) {
            while (len + tl + 1 > cap) { cap *= 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            memcpy(o + len, to, tl);
            len += tl;
            p += fl;
        } else {
            if (len + 2 > cap) { cap *= 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            o[len++] = *p++;
        }
    }
    o[len] = '\0';
    return o;
}

static char *pp_removesuffix(const char *s, const char *suf) {
    if (!s) s = "";
    size_t n = strlen(s), m = strlen(suf);
    if (m <= n && strcmp(s + n - m, suf) == 0) {
        char *o = malloc(n - m + 1);
        if (o) { memcpy(o, s, n - m); o[n - m] = '\0'; }
        return o;
    }
    return pp_strdup(s);
}

/* str.partition(sep) -> owned 3-string array [before, sep, after]. */
static json_t *pp_partition(const char *s, const char *sep) {
    json_t *a = json_array();
    const char *hit = s ? strstr(s, sep) : NULL;
    if (!s) { json_append(a, json_string("")); json_append(a, json_string("")); json_append(a, json_string("")); return a; }
    if (!hit) {
        json_append(a, json_string(s));
        json_append(a, json_string(""));
        json_append(a, json_string(""));
    } else {
        size_t n = (size_t)(hit - s);
        char *before = malloc(n + 1);
        if (before) { memcpy(before, s, n); before[n] = '\0'; }
        json_append(a, json_string(before ? before : ""));
        free(before);
        json_append(a, json_string(sep));
        json_append(a, json_string(hit + strlen(sep)));
    }
    return a;
}

/* str.split(sep) with an explicit separator (no whitespace folding). */
static json_t *pp_split(const char *s, const char *sep) {
    json_t *a = json_array();
    if (!s) return a;
    size_t sl = strlen(sep);
    const char *p = s;
    while (1) {
        const char *hit = strstr(p, sep);
        if (!hit) { json_append(a, json_string(p)); break; }
        size_t n = (size_t)(hit - p);
        char *frag = malloc(n + 1);
        if (frag) { memcpy(frag, p, n); frag[n] = '\0'; }
        json_append(a, json_string(frag ? frag : ""));
        free(frag);
        p = hit + sl;
    }
    return a;
}

/* s.rsplit(sep, 1)[-1]: text after the LAST separator (whole s if absent). */
static char *pp_rsplit_last(const char *s, const char *sep) {
    if (!s) return pp_strdup("");
    const char *hit = NULL;
    for (const char *p = s; (p = strstr(p, sep)) != NULL; p += strlen(sep)) hit = p;
    return pp_strdup(hit ? hit + strlen(sep) : s);
}

/* PurePosixPath join on the fixture corpus (no '.'/'..' components). */
static char *pp_join(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    if (b[0] == '/') return pp_strdup(b);
    size_t n = strlen(a);
    while (n > 0 && a[n - 1] == '/') n--;
    bool root = (n == 0 && a[0] == '/');
    bool dotlike = (n == 0 && (a[0] == '\0' || (a[0] == '.' && a[1] == '\0')));
    if (b[0] == '\0') {
        if (root) return pp_strdup("/");
        if (dotlike) return pp_strdup(a);
        char *o = malloc(n + 1);
        if (o) { memcpy(o, a, n); o[n] = '\0'; }
        return o;
    }
    if (dotlike) return pp_strdup(b);
    if (root) return pp_xasprintf("/%s", b);
    char *o = malloc(n + 1 + strlen(b) + 1);
    if (o) { memcpy(o, a, n); o[n] = '/'; strcpy(o + n + 1, b); }
    return o;
}

/* PurePosixPath.parent on the fixture corpus. */
static char *pp_path_parent(const char *s) {
    if (!s || !s[0]) return pp_strdup(".");
    size_t n = strlen(s);
    while (n > 1 && s[n - 1] == '/') n--;
    if (n == 1 && s[0] == '/') return pp_strdup("/");
    const char *slash = NULL;
    for (size_t i = 0; i < n; i++) if (s[i] == '/') slash = s + i;
    if (!slash) return pp_strdup(".");
    if (slash == s) return pp_strdup("/");
    size_t k = (size_t)(slash - s);
    char *o = malloc(k + 1);
    if (o) { memcpy(o, s, k); o[k] = '\0'; }
    return o;
}

/* lhs / rhs where lhs is a seam result: str joins, anything else raises the
 * real operand TypeError (None / "cache" etc). Returns a string node. */
static json_t *pp_path_join_node(const json_t *lhs, const char *rhs) {
    const char *s = pp_str(lhs);
    if (!s) {
        char *m = pp_xasprintf("unsupported operand type(s) for /: '%s' and 'str'", pp_type_name(lhs));
        json_t *e = pp_type_err(m ? m : "");
        free(m);
        return e;
    }
    char *j = pp_join(s, rhs);
    json_t *o = json_string(j ? j : "");
    free(j);
    return o;
}

/* "x" in container: str substring / list membership / dict keys; else the
 * py3.14 TypeError. */
static json_t *pp_contains(const json_t *hay, const char *needle, bool *out) {
    if (hay && hay->type == JSON_STRING) { *out = strstr(hay->str_val, needle) != NULL; return NULL; }
    if (hay && hay->type == JSON_ARRAY) {
        *out = false;
        for (size_t i = 0; i < hay->c.count; i++)
            if (pp_str_eq(hay->c.items[i], needle)) { *out = true; break; }
        return NULL;
    }
    if (hay && hay->type == JSON_OBJECT) { *out = json_obj_get(hay, needle) != NULL; return NULL; }
    char *m = pp_xasprintf("argument of type '%s' is not a container or iterable", pp_type_name(hay));
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

/* dict.get on a seam result: non-dict raises AttributeError 'get'. Absent
 * key returns NULL (caller substitutes the default). */
static json_t *pp_dict_get(const json_t *d, const char *key, json_t **err) {
    *err = NULL;
    if (!d || d->type != JSON_OBJECT) { *err = pp_attr_err(d, "get"); return NULL; }
    return json_obj_get(d, key);
}

/* self.<attr> on the SimpleNamespace double. */
static json_t *pp_self_attr(const json_t *self, const char *name, json_t **err) {
    *err = NULL;
    json_t *v = (self && self->type == JSON_OBJECT) ? json_obj_get(self, name) : NULL;
    if (!v) {
        char *m = pp_xasprintf("'types.SimpleNamespace' object has no attribute '%s'", name);
        *err = pp_err("AttributeError", m ? m : "");
        free(m);
    }
    return v;
}

/* for x in v / [*v]: list items, str chars, dict keys; else TypeError.
 * Returns an OWNED array of borrowed-value copies, or a raise spec. */
static json_t *pp_iter_items(const json_t *v) {
    if (v && v->type == JSON_ARRAY) return json_copy(v);
    if (v && v->type == JSON_STRING) {
        json_t *a = json_array();
        for (const char *p = v->str_val; *p; p++) {
            char c[2] = { *p, '\0' };
            json_append(a, json_string(c));
        }
        return a;
    }
    if (v && v->type == JSON_OBJECT) {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, json_string(v->c.keys[i]));
        return a;
    }
    return pp_not_iterable(v);
}

/* *v argument splat: same iteration, but the non-iterable message is the
 * "argument after * must be an iterable, not X" form. */
static json_t *pp_star_items(const json_t *v) {
    if (v && (v->type == JSON_ARRAY || v->type == JSON_STRING || v->type == JSON_OBJECT))
        return pp_iter_items(v);
    char *m = pp_xasprintf("argument after * must be an iterable, not %s", pp_type_name(v));
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

/* [x, *v] list-display splat: the py3.14 "Value after * must be an iterable,
 * not X" form (no function-name prefix, unlike the call splat). */
static json_t *pp_spread_items(const json_t *v) {
    if (v && (v->type == JSON_ARRAY || v->type == JSON_STRING || v->type == JSON_OBJECT))
        return pp_iter_items(v);
    char *m = pp_xasprintf("Value after * must be an iterable, not %s", pp_type_name(v));
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

/* a, b = v (2-target unpack). Returns an OWNED 2-array or a raise spec with
 * the py3.14 messages (string overflow omits "got N"). */
static json_t *pp_unpack2(const json_t *v) {
    const char *tn = pp_type_name(v);
    if (v && (v->type == JSON_ARRAY || v->type == JSON_OBJECT || v->type == JSON_STRING)) {
        json_t *items = pp_iter_items(v);
        size_t n = items->c.count;
        if (n == 2) return items;
        json_free(items);
        char *m;
        if (n > 2) {
            if (v->type == JSON_STRING) m = pp_strdup("too many values to unpack (expected 2)");
            else m = pp_xasprintf("too many values to unpack (expected 2, got %zu)", n);
        } else {
            m = pp_xasprintf("not enough values to unpack (expected 2, got %zu)", n);
        }
        json_t *e = pp_err("ValueError", m ? m : "");
        free(m);
        return e;
    }
    char *m = pp_xasprintf("cannot unpack non-iterable %s object", tn);
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

/* int(str) on the fixture corpus: ASCII ws strip, sign, underscores between
 * digits. Returns 0 on success / -1 on ValueError (message via *err). */
static int pp_int_parse(const char *s, long long *out, json_t **err) {
    *err = NULL;
    const char *orig = s ? s : "";
    const char *p = orig;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *end = p + strlen(p);
    while (end > p && isspace((unsigned char)end[-1])) end--;
    bool neg = false;
    if (p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
    const char *dstart = p;
    long long v = 0;
    int digits = 0;
    bool prev_digit = false;
    for (; p < end; p++) {
        char c = *p;
        if (c == '_') {
            if (!prev_digit || p + 1 >= end || !isdigit((unsigned char)p[1])) goto bad;
            prev_digit = false;
            continue;
        }
        if (!isdigit((unsigned char)c)) goto bad;
        v = v * 10 + (c - '0');
        digits++;
        prev_digit = true;
    }
    if (digits == 0) goto bad;
    (void)dstart;
    *out = neg ? -v : v;
    return 0;
bad:;
    char *r = pp_repr_str(orig);
    char *m = pp_xasprintf("invalid literal for int() with base 10: %s", r ? r : "''");
    *err = pp_err("ValueError", m ? m : "");
    free(r);
    free(m);
    return -1;
}

/* str.format(toolkit=...) on the {toolkit}-hole corpus. */
static char *pp_format_toolkit(const char *tpl, const char *toolkit, json_t **err) {
    *err = NULL;
    size_t cap = strlen(tpl) + strlen(toolkit) + 16, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    const char *p = tpl;
    while (*p) {
        if (*p == '{') {
            if (p[1] == '{') { o[len++] = '{'; p += 2; continue; }
            const char *close = strchr(p, '}');
            if (!close) { *err = pp_err("ValueError", "Single '{' encountered in format string"); free(o); return NULL; }
            size_t n = (size_t)(close - p - 1);
            char *name = malloc(n + 1);
            if (name) { memcpy(name, p + 1, n); name[n] = '\0'; }
            if (name && strcmp(name, "toolkit") == 0) {
                size_t tl = strlen(toolkit);
                while (len + tl + 1 > cap) { cap *= 2; char *nb = realloc(o, cap); if (!nb) { free(o); free(name); return NULL; } o = nb; }
                memcpy(o + len, toolkit, tl);
                len += tl;
            } else {
                char *r = pp_repr_str(name ? name : "");
                char *m = pp_xasprintf("%s", r ? r : "''");
                *err = pp_err("KeyError", m ? m : "");
                free(r); free(m); free(o); free(name);
                return NULL;
            }
            free(name);
            p = close + 1;
        } else if (*p == '}') {
            if (p[1] == '}') { o[len++] = '}'; p += 2; continue; }
            *err = pp_err("ValueError", "Single '}' encountered in format string");
            free(o);
            return NULL;
        } else {
            if (len + 2 > cap) { cap *= 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
            o[len++] = *p++;
        }
    }
    o[len] = '\0';
    return o;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *pp_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    if (strcmp(type, "TimeoutExpired") == 0) {
        /* str(TimeoutExpired(cmd, timeout)) == "Command '<repr cmd>' timed
         * out after <timeout> seconds" (probed on CPython 3.14.9). */
        const json_t *cmd = json_obj_get(spec, "cmd");
        char *crepr = NULL;
        if (cmd && cmd->type == JSON_ARRAY) {
            size_t cap = 64, len = 0;
            crepr = malloc(cap);
            if (crepr) {
                crepr[len++] = '[';
                for (size_t i = 0; i < cmd->c.count; i++) {
                    char *r = pp_repr_str(pp_str(cmd->c.items[i]));
                    size_t rl = r ? strlen(r) : 0;
                    while (len + rl + 4 > cap) { cap *= 2; char *nb = realloc(crepr, cap); if (!nb) { free(crepr); crepr = NULL; break; } crepr = nb; }
                    if (!crepr) { free(r); break; }
                    if (i) { crepr[len++] = ','; crepr[len++] = ' '; }
                    if (r) { memcpy(crepr + len, r, rl); len += rl; }
                    free(r);
                }
                if (crepr) { crepr[len++] = ']'; crepr[len] = '\0'; }
            }
        } else {
            crepr = pp_repr_str(pp_str(cmd));
        }
        double t = json_get_num(spec, "timeout", 60);
        char tbuf[64];
        if (t == (double)(long long)t) snprintf(tbuf, sizeof tbuf, "%.0f", t);
        else snprintf(tbuf, sizeof tbuf, "%.17g", t);
        char *m = pp_xasprintf("Command '%s' timed out after %s seconds", crepr ? crepr : "None", tbuf);
        free(crepr);
        json_t *e = pp_err("TimeoutExpired", m ? m : "");
        free(m);
        return e;
    }
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && pp_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = pp_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return pp_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). */
static json_t *pp_call(pp_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = pp_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = pp_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = pp_make_exc(raise);
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

/* Seam args builder helpers: drop None-valued keys on both sides. */
static void pp_arg_str(json_t *args, const char *key, const json_t *v) {
    const char *s = pp_str(v);
    if (s) json_set_hdr(args, key, s);
}

static json_t *pp_ke(const char *key) {
    char *r = pp_repr_str(key);
    json_t *e = pp_err("KeyError", r ? r : "");
    free(r);
    return e;
}

static json_t *pp_subscript_err(const json_t *v) {
    char *m = pp_xasprintf("'%s' object is not subscriptable", pp_type_name(v));
    json_t *e = pp_type_err(m ? m : "");
    free(m);
    return e;
}

/* null-aware string equality (None == None is True in Python). */
static bool pp_str_eq_null(const json_t *a, const json_t *b) {
    if (pp_is_null_node(a) && pp_is_null_node(b)) return true;
    return pp_str_eq(a, pp_str(b));
}

/* -- module constants (live pm/packages.py + pm/store.py) -- */
static const char *pp_rust_triple(const char *target) {
    static const struct { const char *t, *triple; } M[] = {
        { "win32-x64", "x86_64-pc-windows-msvc" },
        { "win32-arm64", "aarch64-pc-windows-msvc" },
        { "linux-x64", "x86_64-unknown-linux-gnu" },
        { "linux-arm64", "aarch64-unknown-linux-gnu" },
        { "linux-x64-musl", "x86_64-unknown-linux-musl" },
        { "linux-arm64-musl", "aarch64-unknown-linux-musl" },
        { "darwin-x64", "x86_64-apple-darwin" },
        { "darwin-arm64", "aarch64-apple-darwin" },
    };
    if (!target) return NULL;
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++)
        if (strcmp(M[i].t, target) == 0) return M[i].triple;
    return NULL;
}

static const char *pp_node_plat(const char *target) {
    static const struct { const char *t, *p; } M[] = {
        { "win32-x64", "win-x64" },
        { "win32-arm64", "win-arm64" },
        { "linux-x64", "linux-x64" },
        { "linux-arm64", "linux-arm64" },
        { "linux-x64-musl", "linux-x64-musl" },
        { "linux-arm64-musl", "linux-arm64-musl" },
        { "darwin-x64", "darwin-x64" },
        { "darwin-arm64", "darwin-arm64" },
    };
    if (!target) return NULL;
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++)
        if (strcmp(M[i].t, target) == 0) return M[i].p;
    return NULL;
}

static bool pp_is_musl(const char *target) {
    return target && (strcmp(target, "linux-x64-musl") == 0 || strcmp(target, "linux-arm64-musl") == 0);
}

static bool pp_is_bionic(const json_t *target) {
    return pp_str_eq(target, "linux-arm64-bionic");
}

static const char *pp_vc_runtime(const char *target) {
    if (!target) return NULL;
    if (strcmp(target, "win32-x64") == 0)
        return "https://download.visualstudio.microsoft.com/download/pr/3984a6ef-2ffc-4c07-962e-bf9e9b862819/"
               "24a1c8dba0376d9b008ac2ba430d13c06664fc6f163511fd7866485030e289af/"
               "Microsoft.VC.14.51.CRT.Redist.X64.base.vsix";
    if (strcmp(target, "win32-arm64") == 0)
        return "https://download.visualstudio.microsoft.com/download/pr/3984a6ef-2ffc-4c07-962e-bf9e9b862819/"
               "4090d939c83e50ba36a3023bd46c7ea622082fadd88a42bd0a9bba445ff710c8/"
               "Microsoft.VC.14.51.CRT.Redist.ARM64.base.vsix";
    return NULL;
}

#define PP_WINDOWS_PYTHON3 "python3.exe"

/* -- module-level functions -- */

/* PoP: pp_npm_env @ pm/packages.py:npm_env */
static json_t *pp_npm_env(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *base_env = json_obj_get(args, "base_env");
    json_t *env = NULL;
    if (pp_is_null_node(base_env)) {
        json_t *r = pp_call(ctx, "os_environ", json_object());
        if (pp_is_raise(r)) return r;
        if (!r || r->type != JSON_OBJECT) { json_t *e = pp_attr_err(r, "items"); json_free(r); return e; }
        env = r;
    } else {
        if (base_env->type != JSON_OBJECT) return pp_attr_err(base_env, "items");
        env = json_copy(base_env);
    }
    json_t *out = json_object();
    for (size_t i = 0; i < env->c.count; i++) {
        const char *key = env->c.keys[i];
        char *lk = pp_lower(key);
        bool npm_cfg = pp_startswith(lk, "npm_config_");
        free(lk);
        if (npm_cfg) continue;
        char *uk = pp_upper(key);
        bool node_opt = strcmp(uk, "NODE_OPTIONS") == 0 || strcmp(uk, "NODE_PATH") == 0 ||
                        strcmp(uk, "NODE_ENV") == 0;
        free(uk);
        if (node_opt) continue;
        json_set(out, key, json_copy(env->c.items[i]));
    }
    json_free(env);
    char *cd = pp_str_of(json_obj_get(args, "cache_dir"));
    json_set(out, "npm_config_cache", json_string(cd ? cd : "None"));
    free(cd);
    return out;
}

/* PoP: pp_pnpm_env @ pm/packages.py:pnpm_env */
static json_t *pp_pnpm_env(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *base_env = json_obj_get(args, "base_env");
    json_t *env = NULL;
    if (pp_is_null_node(base_env)) {
        json_t *r = pp_call(ctx, "os_environ", json_object());
        if (pp_is_raise(r)) return r;
        if (!r || r->type != JSON_OBJECT) { json_free(r); return pp_not_iterable(NULL); }
        env = r;
    } else {
        if (base_env->type != JSON_OBJECT) return pp_not_iterable(base_env);
        env = json_copy(base_env);
    }
    json_t *root = pp_call(ctx, "get_default_hermes_root", json_object());
    if (pp_is_raise(root)) { json_free(env); return root; }
    json_t *j1 = pp_path_join_node(root, "cache");
    json_free(root);
    if (pp_is_raise(j1)) { json_free(env); return j1; }
    json_t *j2 = pp_path_join_node(j1, "pnpm");
    json_free(j1);
    if (pp_is_raise(j2)) { json_free(env); return j2; }
    const char *root_s = pp_str(j2);
    static const char *names[] = { "store", "cache", "state" };
    for (size_t n = 0; n < 3; n++) {
        char *key = pp_xasprintf("pnpm_config_%s_dir", names[n]);
        char *ukey = pp_upper(key);
        /* collect twins first (the live list comprehension), then delete */
        json_t *twins = json_array();
        for (size_t i = 0; i < env->c.count; i++) {
            char *uk = pp_upper(env->c.keys[i]);
            if (strcmp(uk, ukey) == 0 && strcmp(env->c.keys[i], key) != 0)
                json_append(twins, json_string(env->c.keys[i]));
            free(uk);
        }
        for (size_t i = 0; i < twins->c.count; i++) json_obj_del(env, twins->c.items[i]->str_val);
        json_free(twins);
        char *val = pp_join(root_s, names[n]);
        json_set(env, key, json_string(val ? val : ""));
        free(val);
        free(key);
        free(ukey);
    }
    json_free(j2);
    return env;
}

/* PoP: pp_uv_cache_path @ pm/packages.py:uv_cache_path */
static json_t *pp_uv_cache_path(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *root = pp_call(ctx, "get_default_hermes_root", json_object());
    if (pp_is_raise(root)) return root;
    json_t *j1 = pp_path_join_node(root, "cache");
    json_free(root);
    if (pp_is_raise(j1)) return j1;
    json_t *j2 = pp_path_join_node(j1, "uv");
    json_free(j1);
    return j2;
}

/* PoP: pp_github_release_digests @ pm/packages.py:_github_release_digests */
static json_t *pp_github_release_digests(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    char *repo = pp_str_of(json_obj_get(args, "repo"));
    char *tag = pp_str_of(json_obj_get(args, "tag"));
    json_t *cache = json_obj_get(ctx->state, "release_digest_cache");
    if (!cache || cache->type != JSON_OBJECT) {
        cache = json_object();
        json_set(ctx->state, "release_digest_cache", cache);
    }
    json_t *sub = json_obj_get(cache, repo);
    if (sub && sub->type == JSON_OBJECT) {
        json_t *hit = json_obj_get(sub, tag);
        if (hit) { free(repo); free(tag); return json_copy(hit); }
    }
    char *url = pp_xasprintf("https://api.github.com/repos/%s/releases/tags/%s", repo, tag);
    json_t *ga = json_object();
    json_set_hdr(ga, "url", url ? url : "");
    json_t *release = pp_call(ctx, "_get_json", ga);
    free(url);
    if (pp_is_raise(release)) {
        if (pp_is_base_exc(pp_raise_kind(release))) { free(repo); free(tag); return release; }
        json_free(release);
        free(repo); free(tag);
        return json_object(); /* except Exception: return {} */
    }
    if (!release || release->type != JSON_OBJECT) {
        json_t *e = pp_attr_err(release, "get");
        json_free(release); free(repo); free(tag);
        return e;
    }
    const json_t *assets = json_obj_get(release, "assets");
    json_t *items = assets ? pp_iter_items(assets) : json_array();
    if (pp_is_raise(items)) { json_free(release); free(repo); free(tag); return items; }
    json_t *digests = json_object();
    json_t *err = NULL;
    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *asset = items->c.items[i];
        if (!asset || asset->type != JSON_OBJECT) { err = pp_attr_err(asset, "get"); break; }
        const json_t *d = json_obj_get(asset, "digest");
        char *digest = NULL;
        if (pp_truthy(d)) {
            const char *ds = pp_str(d);
            if (!ds) { err = pp_attr_err(d, "partition"); break; }
            const char *hit = strstr(ds, "sha256:");
            digest = pp_strdup(hit ? hit + 7 : "");
        } else {
            digest = pp_strdup("");
        }
        if (digest[0]) {
            const json_t *name = json_obj_get(asset, "name");
            if (!name) { free(digest); err = pp_ke("name"); break; }
            const char *ns = pp_str(name);
            if (!ns) { free(digest); err = pp_type_err("unhashable type"); break; }
            json_set(digests, ns, json_string(digest));
        }
        free(digest);
    }
    json_free(items);
    json_free(release);
    if (err) { json_free(digests); free(repo); free(tag); return err; }
    sub = json_obj_get(cache, repo);
    if (!sub || sub->type != JSON_OBJECT) {
        sub = json_object();
        json_set(cache, repo, sub);
    }
    json_set(sub, tag, json_copy(digests));
    free(repo);
    free(tag);
    return digests;
}

/* -- shared small helpers for the method flows -- */

/* ' '.join(items) on str items (the probe_args corpus). */
static char *pp_join_strs(const json_t *items, const char *sep) {
    size_t cap = 64, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    o[0] = '\0';
    size_t sl = strlen(sep);
    for (size_t i = 0; i < items->c.count; i++) {
        const char *s = pp_str(items->c.items[i]);
        if (!s) { free(o); return NULL; }
        size_t need = len + sl + strlen(s) + 1;
        while (need > cap) { cap *= 2; char *nb = realloc(o, cap); if (!nb) { free(o); return NULL; } o = nb; }
        if (i) { memcpy(o + len, sep, sl); len += sl; }
        size_t l = strlen(s);
        memcpy(o + len, s, l);
        len += l;
        o[len] = '\0';
    }
    return o;
}

/* -- BinaryPackage -- */

/* PoP: pp_bp_rel @ pm/packages.py:_rel */
static json_t *pp_bp_rel(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    if (!ts) return pp_attr_err(target, "startswith");
    json_t *err = NULL;
    json_t *br = pp_self_attr(self, "binary_rel", &err);
    if (err) return err;
    json_t *v = pp_dict_get(br, ts, &err);
    if (err) return err;
    if (pp_truthy(v)) return json_copy(v);
    json_t *v2 = pp_dict_get(br, pp_startswith(ts, "win32") ? "win32" : "posix", &err);
    if (err) return err;
    return v2 ? json_copy(v2) : json_null();
}

/* PoP: pp_bp_stage @ pm/packages.py:stage */
static json_t *pp_bp_stage(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    json_t *err = NULL;
    json_t *flatten = pp_self_attr(self, "flatten", &err);
    if (err) return err;
    if (pp_truthy(flatten)) {
        json_t *sa = json_object();
        pp_arg_str(sa, "staged", json_obj_get(args, "staged"));
        json_t *r = pp_call(ctx, "flatten_single_dir", sa);
        if (pp_is_raise(r)) return r;
        json_free(r); /* live ignores the return; stage() returns None */
    }
    return json_null();
}

/* PoP: pp_bp_binary @ pm/packages.py:binary */
static json_t *pp_bp_binary(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    json_t *rel = pp_bp_rel(ctx, self, args);
    if (pp_is_raise(rel)) return rel;
    if (pp_is_null_node(rel)) { json_free(rel); return json_null(); }
    const json_t *entry = json_obj_get(args, "entry");
    const char *es = pp_str(entry);
    const char *rs = pp_str(rel);
    if (!es || !rs) {
        char *m = pp_xasprintf("unsupported operand type(s) for /: '%s' and '%s'",
                               pp_type_name(entry), pp_type_name(rel));
        json_t *e = pp_type_err(m ? m : "");
        free(m);
        json_free(rel);
        return e;
    }
    char *j = pp_join(es, rs);
    json_t *o = json_string(j ? j : "");
    free(j);
    json_free(rel);
    return o;
}

/* PoP: pp_bp_probe_env @ pm/packages.py:_probe_env */
static json_t *pp_bp_probe_env(pp_ctx_t *ctx, const json_t *self) {
    json_t *err = NULL;
    json_t *deps = pp_self_attr(self, "deps", &err);
    if (err) return err;
    if (!pp_truthy(deps)) {
        json_t *r = pp_call(ctx, "os_environ", json_object());
        if (pp_is_raise(r)) return r;
        if (!r || r->type != JSON_OBJECT) { json_free(r); return pp_not_iterable(NULL); }
        return r; /* dict(os.environ) copy */
    }
    json_t *items = pp_star_items(deps);
    if (pp_is_raise(items)) return items;
    json_t *da = json_object();
    json_set(da, "deps", items);
    return pp_call(ctx, "env_for", da);
}

/* Classify a raise from the subprocess.run try region:
 * 0 = not a raise, 1 = OSError handler, 2 = TimeoutExpired handler,
 * 3 = propagate (BaseException or any other kind). */
static int pp_probe_raise_class(const json_t *r) {
    if (!pp_is_raise(r)) return 0;
    const char *k = pp_raise_kind(r);
    if (pp_is_base_exc(k)) return 3;
    if (strcmp(k, "TimeoutExpired") == 0) return 2;
    if (pp_is_oserror_type(k)) return 1;
    return 3;
}

/* PoP: pp_bp_verify @ pm/packages.py:verify */
static json_t *pp_bp_verify(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    json_t *ret = NULL, *err = NULL;
    json_t *binary = pp_bp_binary(ctx, self, args);
    if (pp_is_raise(binary)) return binary;
    if (pp_is_null_node(binary)) { json_free(binary); return json_string("no binary_rel for this target"); }
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *target = json_obj_get(args, "target");
    json_t *ba = json_object();
    pp_arg_str(ba, "binary", binary);
    pp_arg_str(ba, "entry", entry);
    pp_arg_str(ba, "target", target);
    json_t *reason = pp_call(ctx, "_binary_reason", ba);
    if (pp_is_raise(reason)) { json_free(binary); return reason; }
    if (pp_truthy(reason)) { json_free(binary); return reason; }
    json_free(reason);
    json_t *pv = pp_self_attr(self, "probe_version", &err);
    if (err) { json_free(binary); return err; }
    if (!pp_truthy(pv)) { json_free(binary); return json_string(""); }
    json_t *ct = pp_call(ctx, "current_target", json_object());
    if (pp_is_raise(ct)) { json_free(binary); return ct; }
    bool same = pp_str_eq_null(ct, target);
    json_free(ct);
    if (!same) { json_free(binary); return json_string(""); }

    json_t *pa = pp_self_attr(self, "probe_args", &err);
    if (err) { json_free(binary); return err; }
    json_t *nb = NULL, *cwd = NULL, *env = NULL, *proc = NULL, *argv = NULL, *raised = NULL;
    int cls = 0;
    json_t *na = json_object();
    pp_arg_str(na, "path", binary);
    nb = pp_call(ctx, "native", na);
    cls = pp_probe_raise_class(nb);
    if (cls == 3) { ret = nb; nb = NULL; goto done; }
    if (cls) { raised = nb; nb = NULL; goto handle; }
    json_t *stars = pp_spread_items(pa);
    if (pp_is_raise(stars)) { json_free(nb); ret = stars; goto done; }
    argv = json_array();
    json_append(argv, nb);
    nb = NULL;
    for (size_t i = 0; i < stars->c.count; i++) json_append(argv, json_copy(stars->c.items[i]));
    json_free(stars);
    json_t *pc = pp_self_attr(self, "probe_cwd", &err);
    if (err) { ret = err; goto done; }
    if (pp_truthy(pc)) {
        char *parent = pp_path_parent(pp_str(binary));
        json_t *na2 = json_object();
        json_set_hdr(na2, "path", parent ? parent : "");
        free(parent);
        json_t *nb2 = pp_call(ctx, "native", na2);
        cls = pp_probe_raise_class(nb2);
        if (cls == 3) { ret = nb2; goto done; }
        if (cls) { raised = nb2; goto handle; }
        cwd = nb2;
    }
    env = pp_bp_probe_env(ctx, self);
    cls = pp_probe_raise_class(env);
    if (cls == 3) { ret = env; env = NULL; goto done; }
    if (cls) { raised = env; env = NULL; goto handle; }
    json_t *ra = json_object();
    json_set(ra, "argv", argv);
    argv = NULL;
    json_set(ra, "capture_output", json_bool(true));
    json_set(ra, "timeout", json_number(60));
    if (!pp_is_null_node(cwd)) { json_set(ra, "cwd", cwd); cwd = NULL; }
    json_set(ra, "env", env);
    env = NULL;
    json_set(ra, "check", json_bool(false));
    proc = pp_call(ctx, "subprocess_run", ra);
    cls = pp_probe_raise_class(proc);
    if (cls == 3) { ret = proc; proc = NULL; goto done; }
    if (cls) { raised = proc; proc = NULL; goto handle; }
    goto after;

handle:
    {
        json_t *na3 = json_object();
        pp_arg_str(na3, "path", binary);
        json_t *nb3 = pp_call(ctx, "native", na3);
        if (pp_is_raise(nb3)) { json_free(raised); ret = nb3; goto done; }
        char *nbs = pp_str_of(nb3);
        json_free(nb3);
        json_t *stars2 = pp_spread_items(pa);
        if (pp_is_raise(stars2)) { free(nbs); json_free(raised); ret = stars2; goto done; }
        char *joined = pp_join_strs(stars2, " ");
        json_free(stars2);
        if (!joined) { free(nbs); json_free(raised); ret = pp_type_err("sequence item: expected str instance"); goto done; }
        char *msg;
        if (cls == 1)
            msg = pp_xasprintf("could not exec %s %s: %s", nbs, joined, json_get_str(raised, "message", ""));
        else
            msg = pp_xasprintf("%s %s timed out after 60s", nbs, joined);
        ret = json_string(msg ? msg : "");
        free(msg);
        free(nbs);
        free(joined);
        json_free(raised);
        goto done;
    }

after:
    if (!proc || proc->type != JSON_OBJECT) { ret = pp_attr_err(proc, "returncode"); goto done; }
    json_t *rc = json_obj_get(proc, "returncode");
    if (!rc) {
        /* the oracle materializes the scripted proc as a SimpleNamespace */
        ret = pp_err("AttributeError", "'types.SimpleNamespace' object has no attribute 'returncode'");
        goto done;
    }
    bool zero = (rc->type == JSON_NUMBER && rc->num_val == 0.0) ||
                (rc->type == JSON_BOOL && !rc->bool_val);
    if (!zero) {
        json_t *pra = json_object();
        pp_arg_str(pra, "binary", binary);
        json_set(pra, "proc", json_copy(proc));
        ret = pp_call(ctx, "_probe_reason", pra);
        goto done;
    }
    ret = json_string("");

done:
    json_free(binary);
    json_free(nb);
    json_free(cwd);
    json_free(env);
    json_free(argv);
    json_free(proc);
    return ret;
}

/* -- Dmgbuild -- */

/* PoP: pp_dmgbuild_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_dmgbuild_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *vs = pp_str(version);
    if (!vs) return pp_attr_err(version, "partition");
    json_t *part = pp_partition(vs, "+");
    const char *release = part->c.items[0]->str_val;
    const char *revision = part->c.items[2]->str_val;
    const char *ts = pp_str(target);
    const char *arch = NULL;
    if (ts && strcmp(ts, "darwin-arm64") == 0) arch = "arm64";
    else if (ts && strcmp(ts, "darwin-x64") == 0) arch = "x86_64";
    if (!arch) { json_free(part); return pp_key_error(target); }
    char *url = pp_xasprintf(
        "https://github.com/electron-userland/electron-builder-binaries/releases/download/"
        "dmg-builder@%s/dmgbuild-bundle-%s-%s.tar.gz", release, arch, revision);
    json_t *o = json_string(url ? url : "");
    free(url);
    json_free(part);
    return o;
}

/* PoP: pp_dmgbuild_verify @ pm/packages.py:verify */
static json_t *pp_dmgbuild_verify(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *target = json_obj_get(args, "target");
    json_t *sa = json_object();
    pp_arg_str(sa, "entry", entry);
    pp_arg_str(sa, "target", target);
    json_t *r = pp_call(ctx, "super_verify", sa);
    if (pp_is_raise(r)) return r;
    if (pp_truthy(r)) return r;
    json_free(r);
    json_t *bin = pp_path_join_node(entry, "python/bin/python3");
    if (pp_is_raise(bin)) return bin;
    json_t *ba = json_object();
    pp_arg_str(ba, "binary", bin);
    pp_arg_str(ba, "entry", entry);
    pp_arg_str(ba, "target", target);
    json_free(bin);
    return pp_call(ctx, "_binary_reason", ba);
}

/* -- _BionicDebArm -- */

/* PoP: pp_bionic_verify @ pm/packages.py:verify */
static json_t *pp_bionic_verify(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    json_t *sa = json_object();
    pp_arg_str(sa, "entry", json_obj_get(args, "entry"));
    pp_arg_str(sa, "target", json_obj_get(args, "target"));
    return pp_call(ctx, pp_is_bionic(json_obj_get(args, "target")) ? "deb_verify" : "bp_verify", sa);
}

/* -- Uv -- */

/* PoP: pp_uv_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_uv_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    char *vs = pp_str_of(version);
    char *url = NULL;
    if (pp_is_bionic(target)) {
        url = pp_xasprintf("https://packages.termux.dev/apt/termux-main/pool/main/u/uv/uv_%s_aarch64.deb", vs);
    } else {
        const char *triple = pp_rust_triple(pp_str(target));
        if (!triple) { free(vs); return pp_key_error(target); }
        const char *ext = pp_startswith(pp_str(target), "win32") ? "zip" : "tar.gz";
        url = pp_xasprintf("https://github.com/astral-sh/uv/releases/download/%s/uv-%s.%s", vs, triple, ext);
    }
    json_t *o = json_string(url ? url : "");
    free(url);
    free(vs);
    return o;
}

/* PoP: pp_uv_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_uv_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "astral-sh/uv");
    return pp_call(ctx, "github_release_tags", ga);
}

/* -- Python -- */

/* PoP: pp_python_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_python_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    if (pp_is_bionic(target)) {
        json_t *err = NULL;
        json_t *u = pp_self_attr(self, "_BIONIC_URL", &err);
        if (err) return err;
        return json_copy(u);
    }
    const char *vs = pp_str(version);
    if (!vs) return pp_attr_err(version, "partition");
    json_t *part = pp_partition(vs, "+");
    const char *pyver = part->c.items[0]->str_val;
    const char *tag = part->c.items[2]->str_val;
    if (!tag[0]) {
        json_t *err = NULL;
        json_t *name = pp_self_attr(self, "name", &err);
        if (err) { json_free(part); return err; }
        char *ns = pp_str_of(name);
        char *r = pp_repr_str(vs);
        char *cause = pp_xasprintf("version %s needs the +<release> tag", r ? r : "''");
        json_t *e = pp_install_error(ns, cause, "");
        free(ns); free(r); free(cause);
        json_free(part);
        return e;
    }
    const char *triple = pp_rust_triple(pp_str(target));
    if (!triple) { json_free(part); return pp_key_error(target); }
    char *url = pp_xasprintf(
        "https://github.com/astral-sh/python-build-standalone/releases/download/"
        "%s/cpython-%s+%s-%s-install_only.tar.gz", tag, pyver, tag, triple);
    json_t *o = json_string(url ? url : "");
    free(url);
    json_free(part);
    return o;
}

/* PoP: pp_python_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_python_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *target = json_obj_get(args, "target");
    const json_t *locked = json_obj_get(args, "locked");
    if (pp_is_bionic(target) || !pp_truthy(locked)) return json_array();
    bool has = false;
    json_t *ce = pp_contains(locked, "+", &has);
    if (ce) return ce;
    if (!has) return json_array();
    const char *ls = pp_str(locked);
    if (!ls) return pp_attr_err(locked, "partition");
    json_t *part = pp_partition(ls, "+");
    json_t *sp = pp_split(part->c.items[0]->str_val, ".");
    size_t n = sp->c.count < 2 ? sp->c.count : 2;
    size_t cap = 64, len = 0;
    char *minor = malloc(cap);
    if (minor) {
        minor[0] = '\0';
        for (size_t i = 0; i < n; i++) {
            const char *s = sp->c.items[i]->str_val;
            size_t need = len + strlen(s) + 2;
            while (need > cap) { cap *= 2; char *nb = realloc(minor, cap); if (!nb) { free(minor); minor = NULL; break; } minor = nb; }
            if (!minor) break;
            if (i) minor[len++] = '.';
            size_t l = strlen(s);
            memcpy(minor + len, s, l);
            len += l;
            minor[len] = '\0';
        }
    }
    json_free(sp);
    json_free(part);
    const char *triple = pp_rust_triple(pp_str(target));
    if (!triple) { free(minor); return pp_key_error(target); }
    json_t *pa = json_object();
    json_set_hdr(pa, "minor", minor ? minor : "");
    json_set_hdr(pa, "triple", triple);
    free(minor);
    return pp_call(ctx, "pbs_versions", pa);
}

/* PoP: pp_python_verify @ pm/packages.py:verify */
static json_t *pp_python_verify(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *target = json_obj_get(args, "target");
    json_t *sa = json_object();
    pp_arg_str(sa, "entry", entry);
    pp_arg_str(sa, "target", target);
    json_t *reason = pp_call(ctx, "super_verify", sa);
    if (pp_is_raise(reason)) return reason;
    if (pp_truthy(reason)) return reason;
    const char *ts = pp_str(target);
    if (!ts) { json_free(reason); return pp_attr_err(target, "startswith"); }
    if (!pp_startswith(ts, "win32")) return reason;
    json_free(reason);
    json_t *alias = pp_path_join_node(entry, PP_WINDOWS_PYTHON3);
    if (pp_is_raise(alias)) return alias;
    json_t *b1 = NULL, *b2 = NULL, *p2 = NULL;
    json_t *ra1 = json_object();
    pp_arg_str(ra1, "path", alias);
    b1 = pp_call(ctx, "read_bytes", ra1);
    if (pp_is_raise(b1)) {
        const char *k = pp_raise_kind(b1);
        if (!pp_is_base_exc(k) && pp_is_oserror_type(k)) {
            json_free(b1); json_free(alias);
            return json_string(PP_WINDOWS_PYTHON3 " is missing beside python.exe");
        }
        json_free(alias);
        return b1;
    }
    p2 = pp_path_join_node(entry, "python.exe");
    if (pp_is_raise(p2)) { json_free(b1); json_free(alias); return p2; }
    json_t *ra2 = json_object();
    pp_arg_str(ra2, "path", p2);
    json_free(p2);
    b2 = pp_call(ctx, "read_bytes", ra2);
    if (pp_is_raise(b2)) {
        const char *k = pp_raise_kind(b2);
        if (!pp_is_base_exc(k) && pp_is_oserror_type(k)) {
            json_free(b1); json_free(b2); json_free(alias);
            return json_string(PP_WINDOWS_PYTHON3 " is missing beside python.exe");
        }
        json_free(b1); json_free(alias);
        return b2;
    }
    bool equal = (pp_is_null_node(b1) && pp_is_null_node(b2)) ||
                 (pp_str(b1) && pp_str(b2) && strcmp(pp_str(b1), pp_str(b2)) == 0);
    json_free(b1);
    json_free(b2);
    json_free(alias);
    return json_string(equal ? "" : PP_WINDOWS_PYTHON3 " differs from python.exe");
}

/* -- Nodejs -- */

/* PoP: pp_nodejs_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_nodejs_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    char *vs = pp_str_of(version);
    char *url = NULL;
    if (pp_is_bionic(target)) {
        url = pp_xasprintf("https://packages.termux.dev/apt/termux-main/pool/main/n/nodejs/nodejs_%s-1_aarch64.deb", vs);
    } else {
        const char *plat = pp_node_plat(pp_str(target));
        if (!plat) { free(vs); return pp_key_error(target); }
        const char *ts = pp_str(target);
        const char *ext = pp_startswith(ts, "win32") ? "zip" : "tar.xz";
        const char *base = pp_is_musl(ts)
            ? "https://unofficial-builds.nodejs.org/download/release"
            : "https://nodejs.org/dist";
        url = pp_xasprintf("%s/v%s/node-v%s-%s.%s", base, vs, vs, plat, ext);
    }
    json_t *o = json_string(url ? url : "");
    free(url);
    free(vs);
    return o;
}

/* PoP: pp_nodejs_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_nodejs_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    return pp_call(ctx, "node_latest_versions", json_object());
}

/* PoP: pp_nodejs_repair @ pm/packages.py:repair_staged_verification */
static json_t *pp_nodejs_repair(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *entry = json_obj_get(args, "entry");
    const json_t *target = json_obj_get(args, "target");
    const json_t *reason = json_obj_get(args, "reason");
    json_t *ct = pp_call(ctx, "current_target", json_object());
    if (pp_is_raise(ct)) return ct;
    bool same = pp_str_eq_null(ct, target);
    json_free(ct);
    const char *ts = pp_str(target);
    bool skip = !same;
    if (!skip) {
        if (!ts) return pp_attr_err(target, "startswith");
        if (!pp_startswith(ts, "linux")) {
            skip = true;
        } else {
            bool has = false;
            json_t *ce = pp_contains(reason, "libatomic.so.1", &has);
            if (ce) return ce;
            skip = !has;
        }
    }
    if (skip) {
        json_t *o = json_array();
        json_append(o, reason ? json_copy(reason) : json_null());
        json_append(o, json_string(""));
        return o;
    }
    json_t *r = pp_call(ctx, "try_install_libatomic", json_object());
    if (pp_is_raise(r)) return r;
    json_t *pair = pp_unpack2(r);
    json_free(r);
    if (pp_is_raise(pair)) return pair;
    json_t *installed = pair->c.items[0];
    json_t *remedy = pair->c.items[1];
    if (!pp_truthy(installed)) {
        json_t *o = json_array();
        json_append(o, reason ? json_copy(reason) : json_null());
        json_append(o, json_copy(remedy));
        json_free(pair);
        return o;
    }
    json_free(pair);
    json_t *va = json_object();
    pp_arg_str(va, "entry", entry);
    pp_arg_str(va, "target", target);
    json_t *retried = pp_call(ctx, "self_verify", va);
    if (pp_is_raise(retried)) return retried;
    bool still = false;
    json_t *ce = pp_contains(retried, "libatomic.so.1", &still);
    if (ce) { json_free(retried); return ce; }
    json_t *o = json_array();
    json_append(o, retried);
    json_append(o, json_string(still
        ? "the libatomic package installed, but Node still cannot load libatomic.so.1; check the loader path"
        : ""));
    return o;
}

/* -- TermuxDocker -- */

/* PoP: pp_termuxdocker_missing_reason @ pm/packages.py:missing_reason */
static json_t *pp_termuxdocker_missing_reason(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    if (pp_is_bionic(json_obj_get(args, "target"))) return json_null();
    return json_string("docker image target is linux-arm64-bionic");
}

/* PoP: pp_termuxdocker_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_termuxdocker_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    char *vs = pp_str_of(json_obj_get(args, "version"));
    char *url = pp_xasprintf("docker://termux/termux-docker@%s", vs);
    json_t *o = json_string(url ? url : "");
    free(url);
    free(vs);
    return o;
}

/* -- Npm / Pnpm latest_versions share the dist-tags shape -- */
static json_t *pp_dist_tags_latest(pp_ctx_t *ctx, const char *pkg) {
    json_t *da = json_object();
    json_set_hdr(da, "name", pkg);
    json_t *r = pp_call(ctx, "npm_dist_tags", da);
    if (pp_is_raise(r)) return r;
    json_t *err = NULL;
    json_t *latest = pp_dict_get(r, "latest", &err);
    if (err) { json_free(r); return err; }
    json_t *o = json_array();
    if (pp_truthy(latest)) json_append(o, json_copy(latest));
    json_free(r);
    return o;
}

/* PoP: pp_npm_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_npm_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    return pp_dist_tags_latest(ctx, "npm");
}

/* PoP: pp_pnpm_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_pnpm_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    return pp_dist_tags_latest(ctx, "pnpm");
}

/* PoP: pp_pnpm_fetch_urls @ pm/packages.py:fetch_urls */
static json_t *pp_pnpm_fetch_urls(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *target = json_obj_get(args, "target");
    const json_t *version = json_obj_get(args, "version");
    json_t *err = NULL;
    json_t *ep = pp_self_attr(self, "exe_platforms", &err);
    if (err) return err;
    if (!ep || ep->type != JSON_OBJECT) return pp_attr_err(ep, "get");
    const char *tk = pp_str(target);
    json_t *v = tk ? json_obj_get(ep, tk) : NULL; /* .get(None, target) -> absent */
    char *exe = v ? pp_str_of(v) : pp_str_of(target);
    char *vs = pp_str_of(version);
    char *u1 = pp_xasprintf("https://registry.npmjs.org/@pnpm/exe.%s/-/exe.%s-%s.tgz", exe, exe, vs);
    char *u2 = pp_xasprintf("https://registry.npmjs.org/pnpm/-/pnpm-%s.tgz", vs);
    json_t *o = json_array();
    json_append(o, json_string(u1 ? u1 : ""));
    json_append(o, json_string(u2 ? u2 : ""));
    free(u1); free(u2); free(exe); free(vs);
    return o;
}

/* -- Git -- */

/* PoP: pp_git_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_git_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *vs = pp_str(version);
    if (!vs) return pp_attr_err(version, "split");
    json_t *sp = pp_split(vs, "+");
    json_t *pair = pp_unpack2(sp);
    json_free(sp);
    if (pp_is_raise(pair)) return pair;
    const char *tag = pair->c.items[0]->str_val;
    const char *build = pair->c.items[1]->str_val;
    const char *ts = pp_str(target);
    if (!ts) { json_free(pair); return pp_attr_err(target, "endswith"); }
    const char *arch = pp_endswith(ts, "arm64") ? "arm64" : "64-bit";
    /* file_rev: "" when build == "1" else ".<build>" */
    char *rev = strcmp(build, "1") == 0 ? pp_strdup("") : pp_xasprintf(".%s", build);
    char *url = pp_xasprintf(
        "https://github.com/git-for-windows/git/releases/download/"
        "v%s.windows.%s/PortableGit-%s%s-%s.7z.exe",
        tag, build, tag, rev ? rev : "", arch);
    free(rev);
    json_t *o = json_string(url ? url : "");
    free(url);
    json_free(pair);
    return o;
}

/* PoP: pp_git_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_git_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "git-for-windows/git");
    json_set_hdr(ga, "strip_prefix", "v");
    json_t *r = pp_call(ctx, "github_release_tags", ga);
    if (pp_is_raise(r)) return r;
    json_t *items = pp_iter_items(r);
    json_free(r);
    if (pp_is_raise(items)) return items;
    json_t *out = json_array();
    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *tag = items->c.items[i];
        const char *ts = pp_str(tag);
        if (!ts) {
            bool dummy = false;
            json_t *ce = pp_contains(tag, ".windows.", &dummy);
            if (ce) { json_free(items); json_free(out); return ce; }
            continue;
        }
        if (!strstr(ts, ".windows.")) continue;
        json_t *part = pp_partition(ts, ".windows.");
        char *v = pp_xasprintf("%s+%s", part->c.items[0]->str_val, part->c.items[2]->str_val);
        json_append(out, json_string(v ? v : ""));
        free(v);
        json_free(part);
    }
    json_free(items);
    return out;
}

/* PoP: pp_git_env @ pm/packages.py:env */
static json_t *pp_git_env(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *entry = json_obj_get(args, "entry");
    const char *es = pp_str(entry);
    if (!es) {
        char *m = pp_xasprintf("unsupported operand type(s) for /: '%s' and 'str'", pp_type_name(entry));
        json_t *e = pp_type_err(m ? m : "");
        free(m);
        return e;
    }
    char *p1 = pp_join(es, "cmd");
    char *p2a = pp_join(es, "usr");
    char *p2 = pp_join(p2a ? p2a : "", "bin");
    free(p2a);
    json_t *path = json_array();
    json_append(path, json_string(p1 ? p1 : ""));
    json_append(path, json_string(p2 ? p2 : ""));
    free(p1); free(p2);
    json_t *o = json_object();
    json_set(o, "PATH", path);
    return o;
}

/* -- Gh -- */

/* PoP: pp_gh_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_gh_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *target = json_obj_get(args, "target");
    const json_t *version = json_obj_get(args, "version");
    const char *ts = pp_str(target);
    char *lookup = NULL;
    if (pp_is_musl(ts)) lookup = pp_removesuffix(ts, "-musl");
    else lookup = pp_strdup(ts ? ts : "");
    if (!ts) {
        /* None in MUSL_TARGETS is False; lookup stays None -> .split raises */
        free(lookup);
        return pp_attr_err(target, "split");
    }
    json_t *sp = pp_split(lookup, "-");
    free(lookup);
    json_t *pair = pp_unpack2(sp);
    json_free(sp);
    if (pp_is_raise(pair)) return pair;
    const char *osname = pair->c.items[0]->str_val;
    const char *arch = pair->c.items[1]->str_val;
    const char *plat = NULL;
    if (strcmp(osname, "win32") == 0) plat = "windows";
    else if (strcmp(osname, "linux") == 0) plat = "linux";
    else if (strcmp(osname, "darwin") == 0) plat = "macOS";
    if (!plat) { json_free(pair); return pp_ke(osname); }
    const char *arch2 = NULL;
    if (strcmp(arch, "x64") == 0) arch2 = "amd64";
    else if (strcmp(arch, "arm64") == 0) arch2 = "arm64";
    if (!arch2) { json_free(pair); return pp_ke(arch); }
    const char *ext = (strcmp(osname, "win32") == 0 || strcmp(osname, "darwin") == 0) ? "zip" : "tar.gz";
    char *vs = pp_str_of(version);
    char *url = pp_xasprintf("https://github.com/cli/cli/releases/download/v%s/gh_%s_%s_%s.%s",
                             vs, vs, plat, arch2, ext);
    json_t *o = json_string(url ? url : "");
    free(url); free(vs);
    json_free(pair);
    return o;
}

/* PoP: pp_gh_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_gh_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "cli/cli");
    json_set_hdr(ga, "strip_prefix", "v");
    return pp_call(ctx, "github_release_tags", ga);
}

/* -- Ffmpeg -- */

/* PoP: pp_ffmpeg_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_ffmpeg_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    char *vs = pp_str_of(version);
    if (pp_is_bionic(target)) {
        char *url = pp_xasprintf("https://packages.termux.dev/apt/termux-main/pool/main/f/ffmpeg/ffmpeg_%s_aarch64.deb", vs);
        json_t *o = json_string(url ? url : "");
        free(url); free(vs);
        return o;
    }
    if (pp_is_musl(ts)) {
        /* InstallError(self.name, f"unavailable on {target}: {self.missing_reason(target)}")
         * -- self.name evaluates BEFORE the f-string's missing_reason seam. */
        json_t *err = NULL;
        json_t *name = pp_self_attr(self, "name", &err);
        if (err) { free(vs); return err; }
        char *ns = pp_str_of(name);
        json_t *ma = json_object();
        pp_arg_str(ma, "target", target);
        json_t *mr = pp_call(ctx, "missing_reason", ma);
        if (pp_is_raise(mr)) { free(ns); free(vs); return mr; }
        char *mrs = pp_str_of(mr);
        json_free(mr);
        char *tss = pp_str_of(target);
        char *cause = pp_xasprintf("unavailable on %s: %s", tss, mrs);
        json_t *e = pp_install_error(ns, cause, "");
        free(ns); free(mrs); free(cause); free(vs); free(tss);
        return e;
    }
    if (!ts) { free(vs); return pp_attr_err(target, "split"); }
    json_t *sp = pp_split(ts, "-");
    json_t *pair = pp_unpack2(sp);
    json_free(sp);
    if (pp_is_raise(pair)) { free(vs); return pair; }
    const char *osname = pair->c.items[0]->str_val;
    const char *arch = pair->c.items[1]->str_val;
    json_t *o = NULL;
    if (strcmp(osname, "win32") == 0 || strcmp(osname, "linux") == 0) {
        json_t *idx = pp_call(ctx, "btbn_index", json_object());
        if (pp_is_raise(idx)) { json_free(pair); free(vs); return idx; }
        json_t *err = NULL;
        json_t *sub = pp_dict_get(idx, ts, &err);
        if (err) { json_free(idx); json_free(pair); free(vs); return err; }
        json_t *art = NULL;
        if (sub) {
            art = pp_dict_get(sub, vs, &err);
            if (err) { json_free(idx); json_free(pair); free(vs); return err; }
        }
        if (art && art->type != JSON_NULL) {
            json_t *ap = pp_unpack2(art);
            if (pp_is_raise(ap)) { json_free(idx); json_free(pair); free(vs); return ap; }
            char *url = pp_xasprintf("https://github.com/BtbN/FFmpeg-Builds/releases/download/%s/%s",
                                     ap->c.items[0]->str_val, ap->c.items[1]->str_val);
            o = json_string(url ? url : "");
            free(url);
            json_free(ap);
        }
        json_free(idx);
    } else {
        json_t *idx = pp_call(ctx, "martin_riedl_index", json_object());
        if (pp_is_raise(idx)) { json_free(pair); free(vs); return idx; }
        json_t *err = NULL;
        json_t *sub = pp_dict_get(idx, ts, &err);
        if (err) { json_free(idx); json_free(pair); free(vs); return err; }
        json_t *epoch = NULL;
        if (sub) {
            epoch = pp_dict_get(sub, vs, &err);
            if (err) { json_free(idx); json_free(pair); free(vs); return err; }
        }
        if (epoch && epoch->type != JSON_NULL) {
            const char *source_arch = strcmp(arch, "x64") == 0 ? "amd64" : arch;
            char *es = pp_str_of(epoch);
            char *url = pp_xasprintf("https://ffmpeg.martin-riedl.de/download/macos/%s/%s_%s/ffmpeg.zip",
                                     source_arch, es, vs);
            o = json_string(url ? url : "");
            free(url); free(es);
        }
        json_free(idx);
    }
    json_free(pair);
    if (o) { free(vs); return o; }
    json_t *err = NULL;
    json_t *name = pp_self_attr(self, "name", &err);
    if (err) { free(vs); return err; }
    char *ns = pp_str_of(name);
    char *cause = pp_xasprintf("no advertised %s artifact for %s", vs, ts);
    json_t *e = pp_install_error(ns, cause, "retry when the upstream index is available, or keep the existing pin");
    free(ns); free(cause); free(vs);
    return e;
}

/* PoP: pp_ffmpeg_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_ffmpeg_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    if (pp_is_musl(ts)) return json_array();
    json_t *ta = json_object();
    pp_arg_str(ta, "target", target);
    if (ts && (strcmp(ts, "win32-x64") == 0 || strcmp(ts, "win32-arm64") == 0 ||
               strcmp(ts, "linux-x64") == 0 || strcmp(ts, "linux-arm64") == 0))
        return pp_call(ctx, "btbn_versions", ta);
    return pp_call(ctx, "martin_riedl_versions", ta);
}

/* -- Ripgrep / Just share the musl-static remap shape -- */
static json_t *pp_musl_static_url(pp_ctx_t *ctx, const json_t *args, const char *repo_path,
                                  const char *fname_prefix) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    char *talloc = NULL;
    if (pp_is_bionic(target)) { ts = "linux-arm64"; }
    const char *triple0 = pp_rust_triple(ts);
    if (!triple0) { free(talloc); return pp_key_error(target); }
    char *triple = pp_replace_all(triple0, "-unknown-linux-gnu", "-unknown-linux-musl");
    const char *ext = pp_startswith(ts, "win32") ? "zip" : "tar.gz";
    char *vs = pp_str_of(version);
    char *url = pp_xasprintf("https://github.com/%s/releases/download/%s/%s-%s-%s.%s",
                             repo_path, vs, fname_prefix, vs, triple ? triple : "", ext);
    json_t *o = json_string(url ? url : "");
    free(url); free(vs); free(triple); free(talloc);
    return o;
}

/* PoP: pp_ripgrep_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_ripgrep_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    return pp_musl_static_url(ctx, args, "BurntSushi/ripgrep", "ripgrep");
}

/* PoP: pp_ripgrep_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_ripgrep_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "BurntSushi/ripgrep");
    return pp_call(ctx, "github_release_tags", ga);
}

/* PoP: pp_just_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_just_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    return pp_musl_static_url(ctx, args, "casey/just", "just");
}

/* PoP: pp_just_known_sha256 @ pm/packages.py:known_sha256 */
static json_t *pp_just_known_sha256(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *url = json_obj_get(args, "url");
    char *vs = pp_str_of(version);
    json_t *da = json_object();
    json_set_hdr(da, "repo", "casey/just");
    json_set_hdr(da, "tag", vs);
    free(vs);
    json_t *r = pp_call(ctx, "github_release_digests", da);
    if (pp_is_raise(r)) return r;
    const char *us = pp_str(url);
    if (!us) { json_free(r); return pp_attr_err(url, "rsplit"); }
    char *last = pp_rsplit_last(us, "/");
    json_t *err = NULL;
    json_t *d = pp_dict_get(r, last, &err);
    free(last);
    if (err) { json_free(r); return err; }
    json_t *o = d ? json_copy(d) : json_null();
    json_free(r);
    return o;
}

/* PoP: pp_just_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_just_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "casey/just");
    return pp_call(ctx, "github_release_tags", ga);
}

/* -- CuaDriver -- */

/* PoP: pp_cuadriver_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_cuadriver_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx; (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    const char *arch = NULL;
    if (ts) {
        if (strcmp(ts, "darwin-x64") == 0 || strcmp(ts, "darwin-arm64") == 0) arch = "darwin-universal";
        else if (strcmp(ts, "linux-x64") == 0) arch = "linux-x86_64";
        else if (strcmp(ts, "linux-arm64") == 0) arch = "linux-arm64";
        else if (strcmp(ts, "win32-x64") == 0) arch = "windows-x86_64";
        else if (strcmp(ts, "win32-arm64") == 0) arch = "windows-arm64";
    }
    if (!arch) return pp_key_error(target);
    const char *ext = pp_startswith(ts, "win32") ? "zip" : "tar.gz";
    const char *variant = pp_startswith(ts, "darwin") ? "" : "-binary";
    char *vs = pp_str_of(version);
    char *url = pp_xasprintf(
        "https://github.com/trycua/cua/releases/download/cua-driver-rs-v%s/"
        "cua-driver-rs-%s-%s%s.%s", vs, vs, arch, variant, ext);
    json_t *o = json_string(url ? url : "");
    free(url); free(vs);
    return o;
}

/* PoP: pp_cuadriver_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_cuadriver_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "trycua/cua");
    json_set_hdr(ga, "strip_prefix", "cua-driver-rs-v");
    json_set(ga, "include_prereleases", json_bool(true));
    return pp_call(ctx, "github_release_tags", ga);
}

/* -- Chromium -- */

/* PoP: pp_chromium_store_entry @ pm/packages.py:store_entry */
static json_t *pp_chromium_store_entry(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const char *vs = pp_str(version);
    if (!vs) return pp_attr_err(version, "partition");
    json_t *part = pp_partition(vs, "+");
    json_t *err = NULL;
    json_t *name = pp_self_attr(self, "name", &err);
    if (err) { json_free(part); return err; }
    const char *ns = pp_str(name);
    if (!ns) { json_free(part); return pp_attr_err(name, "replace"); }
    char *repl = pp_replace_all(ns, "-", "_");
    char *o = pp_xasprintf("%s-%s", repl ? repl : "", part->c.items[0]->str_val);
    json_t *r = json_string(o ? o : "");
    free(o); free(repl);
    json_free(part);
    return r;
}

/* PoP: pp_chromium_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_chromium_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *vs = pp_str(version);
    if (!vs) return pp_attr_err(version, "partition");
    json_t *part = pp_partition(vs, "+");
    const char *chrome = part->c.items[2]->str_val;
    json_t *err = NULL;
    json_t *cft = pp_self_attr(self, "_CFT", &err);
    if (err) { json_free(part); return err; }
    if (!cft || cft->type != JSON_OBJECT) { json_free(part); return pp_subscript_err(cft); }
    const char *ts = pp_str(target);
    json_t *plat = ts ? json_obj_get(cft, ts) : NULL;
    if (!plat) { json_free(part); return pp_key_error(target); }
    char *plats = pp_str_of(plat);
    json_t *cdn = pp_self_attr(self, "_CDN", &err);
    if (err) { json_free(part); free(plats); return err; }
    char *cdns = pp_str_of(cdn);
    char *url = pp_xasprintf("%s/builds/cft/%s/%s/chrome-%s.zip", cdns, chrome, plats, plats);
    json_t *o = json_string(url ? url : "");
    free(url); free(cdns); free(plats);
    json_free(part);
    return o;
}

/* -- LlamaCpp -- */

/* PoP: pp_llamacpp_asset_names @ pm/packages.py:_asset_names */
static json_t *pp_llamacpp_asset_names(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    const char *ext = pp_startswith(ts, "win32") ? "zip" : "tar.gz";
    json_t *err = NULL;
    json_t *assets = pp_self_attr(self, "assets", &err);
    if (err) return err;
    if (!assets || assets->type != JSON_OBJECT) return pp_subscript_err(assets);
    json_t *infix = ts ? json_obj_get(assets, ts) : NULL;
    if (!infix) return pp_key_error(target);
    char *infixs = pp_str_of(infix);
    char *vs = pp_str_of(version);
    char *name = pp_xasprintf("llama-b%s-bin-%s.%s", vs, infixs, ext);
    json_t *o = json_array();
    json_append(o, json_string(name ? name : ""));
    free(name); free(vs); free(infixs);
    return o;
}

/* PoP: pp_llamacpp_fetch_urls @ pm/packages.py:fetch_urls */
static json_t *pp_llamacpp_fetch_urls(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *ts = pp_str(target);
    json_t *err = NULL;
    json_t *libgomp = pp_self_attr(self, "_LIBGOMP", &err);
    if (err) return err;
    /* runtime = [libs[target] for libs in (self._LIBGOMP, _VC_RUNTIME) if target in libs] */
    json_t *runtime = json_array();
    bool in1 = false;
    json_t *ce = pp_contains(libgomp, ts ? ts : "", &in1);
    if (ce) { json_free(runtime); return ce; }
    if (in1 && ts) {
        json_t *u = json_obj_get(libgomp, ts);
        json_append(runtime, u ? json_copy(u) : json_null());
    }
    const char *vc = pp_vc_runtime(ts);
    if (vc) json_append(runtime, json_string(vc));
    json_t *names = pp_llamacpp_asset_names(ctx, self, args);
    if (pp_is_raise(names)) { json_free(runtime); return names; }
    char *vs = pp_str_of(version);
    json_t *o = json_array();
    for (size_t i = 0; i < names->c.count; i++) {
        char *url = pp_xasprintf("https://github.com/ggml-org/llama.cpp/releases/download/b%s/%s",
                                 vs, pp_str(names->c.items[i]) ? names->c.items[i]->str_val : "");
        json_append(o, json_string(url ? url : ""));
        free(url);
    }
    for (size_t i = 0; i < runtime->c.count; i++) json_append(o, json_copy(runtime->c.items[i]));
    json_free(names);
    json_free(runtime);
    free(vs);
    return o;
}

/* PoP: pp_llamacpp_fetch_url @ pm/packages.py:fetch_url */
static json_t *pp_llamacpp_fetch_url(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    json_t *urls = pp_llamacpp_fetch_urls(ctx, self, args);
    if (pp_is_raise(urls)) return urls;
    if (!urls || urls->type != JSON_ARRAY || urls->c.count == 0) {
        json_free(urls);
        return pp_err("IndexError", "list index out of range");
    }
    json_t *o = json_copy(urls->c.items[0]);
    json_free(urls);
    return o;
}

/* PoP: pp_llamacpp_latest_versions @ pm/packages.py:latest_versions */
static json_t *pp_llamacpp_latest_versions(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self; (void)args;
    json_t *latest = pp_call(ctx, "llama_app_latest", json_object());
    if (pp_is_raise(latest)) return latest;
    if (!pp_is_null_node(latest)) {
        json_t *bucket = pp_call(ctx, "llama_app_bucket_versions", json_object());
        if (pp_is_raise(bucket)) { json_free(latest); return bucket; }
        json_t *items = pp_spread_items(bucket);
        json_free(bucket);
        if (pp_is_raise(items)) { json_free(latest); return items; }
        json_t *o = json_array();
        json_append(o, latest);
        for (size_t i = 0; i < items->c.count; i++) json_append(o, json_copy(items->c.items[i]));
        json_free(items);
        return o;
    }
    json_free(latest);
    json_t *bucket = pp_call(ctx, "llama_app_bucket_versions", json_object());
    if (pp_is_raise(bucket)) return bucket;
    if (pp_truthy(bucket)) return bucket;
    json_free(bucket);
    json_t *ga = json_object();
    json_set_hdr(ga, "repo", "ggml-org/llama.cpp");
    json_set_hdr(ga, "strip_prefix", "b");
    return pp_call(ctx, "github_release_tags", ga);
}

/* PoP: pp_llamacpp_known_sha256 @ pm/packages.py:known_sha256 */
static json_t *pp_llamacpp_known_sha256(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *url = json_obj_get(args, "url");
    char *vs = pp_str_of(version);
    char *tag = pp_xasprintf("b%s", vs);
    free(vs);
    json_t *da = json_object();
    json_set_hdr(da, "repo", "ggml-org/llama.cpp");
    json_set_hdr(da, "tag", tag ? tag : "b");
    free(tag);
    json_t *r = pp_call(ctx, "github_release_digests", da);
    if (pp_is_raise(r)) return r;
    const char *us = pp_str(url);
    if (!us) { json_free(r); return pp_attr_err(url, "rsplit"); }
    char *last = pp_rsplit_last(us, "/");
    json_t *err = NULL;
    json_t *d = pp_dict_get(r, last, &err);
    free(last);
    if (err) { json_free(r); return err; }
    json_t *o = d ? json_copy(d) : json_null();
    json_free(r);
    return o;
}

/* -- LlamaCppCuda -- */

/* PoP: pp_llamacppcuda_asset_names @ pm/packages.py:_asset_names */
static json_t *pp_llamacppcuda_asset_names(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)ctx;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    const char *vs = pp_str(version);
    if (!vs) {
        char *m = pp_xasprintf("int() argument must be a string, a bytes-like object or a real number, not '%s'",
                               pp_type_name(version));
        json_t *e = pp_type_err(m ? m : "");
        free(m);
        return e;
    }
    long long iv = 0;
    json_t *ierr = NULL;
    if (pp_int_parse(vs, &iv, &ierr) != 0) return ierr;
    json_t *err = NULL;
    json_t *first = pp_self_attr(self, "_FIRST_CUDA_13_5", &err);
    if (err) return err;
    if (!first || first->type != JSON_NUMBER) {
        char *m = pp_xasprintf("'>=' not supported between instances of 'int' and '%s'", pp_type_name(first));
        json_t *e = pp_type_err(m ? m : "");
        free(m);
        return e;
    }
    double fv = first->num_val;
    const char *toolkit = ((double)iv >= fv) ? "13.5" : "13.4";
    json_t *assets = pp_self_attr(self, "assets", &err);
    if (err) return err;
    if (!assets || assets->type != JSON_OBJECT) return pp_subscript_err(assets);
    const char *ts = pp_str(target);
    json_t *tpl = ts ? json_obj_get(assets, ts) : NULL;
    if (!tpl) return pp_key_error(target);
    if (tpl->type != JSON_STRING) return pp_attr_err(tpl, "format");
    json_t *ferr = NULL;
    char *infix = pp_format_toolkit(tpl->str_val, toolkit, &ferr);
    if (ferr) return ferr;
    const char *ext = pp_startswith(ts, "win32") ? "zip" : "tar.gz";
    char *engine = pp_xasprintf("llama-b%s-bin-%s.%s", vs, infix ? infix : "", ext);
    char *cudart;
    if (pp_startswith(ts, "win32"))
        cudart = pp_xasprintf("cudart-llama-bin-%s.zip", infix ? infix : "");
    else
        cudart = pp_xasprintf("cudart-llama-b%s-bin-%s.tar.gz", vs, infix ? infix : "");
    json_t *o = json_array();
    json_append(o, json_string(engine ? engine : ""));
    json_append(o, json_string(cudart ? cudart : ""));
    free(engine); free(cudart); free(infix);
    return o;
}

/* -- WhisperCppCpu -- */

/* PoP: pp_whispercpp_fetch_urls @ pm/packages.py:fetch_urls */
static json_t *pp_whispercpp_fetch_urls(pp_ctx_t *ctx, const json_t *self, const json_t *args) {
    (void)self;
    const json_t *version = json_obj_get(args, "version");
    const json_t *target = json_obj_get(args, "target");
    json_t *fa = json_object();
    pp_arg_str(fa, "version", version);
    pp_arg_str(fa, "target", target);
    json_t *r = pp_call(ctx, "base_fetch_url", fa);
    if (pp_is_raise(r)) return r;
    const char *vc = pp_vc_runtime(pp_str(target));
    if (!vc) { json_free(r); return pp_key_error(target); }
    json_t *o = json_array();
    json_append(o, r);
    json_append(o, json_string(vc));
    return o;
}
