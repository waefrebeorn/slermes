/* kanban_tools_flow.c — FLOW port of tools/kanban_tools.py (tool surface).
 *
 * Ports the tool-call decision sequences: delegation-context gating, the
 * visibility check, the validation leaves (_check / _require_text /
 * _coerce_str_list / _require_dict_metadata / _merge_artifacts / _opt_int),
 * record projection (_fields), redaction wrappers, task-id resolution
 * (_own_task_env / _reject_inherited_task_mutation / _require_task_id),
 * the worker mutation preamble (_worker_guard), the _kanban_handler
 * wrapper (unknown-arg chain + structured error rendering) and the
 * _ok_landed success formatter. All external I/O is INJECTED through the
 * ktf_io_t seam dispatcher — no registry, board, config or redaction code
 * is linked.
 *
 * Seam model (mirrors port_kanban_db_flow.c): every external call goes
 * through ktf_seam(), which appends {"call": name, ...args} to io->trace
 * (the ordered decision sequence) and returns the scripted result. A
 * raised exception is modeled as {"__raise__": "<Type>", "message": "..."}
 * where message == str(exc) (KeyError specs carry repr(key)). Scripted
 * results: {"ret": value, "raise": {"type","message"}}; a list entry is a
 * per-call queue, a single object is reused. NULL result == None.
 *
 * Seams (name -> live call):
 *   delegation_ctx            -> getattr(agent.delegation_context, pred)()
 *                                (import failure: state.delegation_import_fail)
 *   profile_has_kanban_toolset-> _profile_has_kanban_toolset()
 *   tool_error                -> tools.registry.tool_error(message)
 *   redact_sensitive_text     -> agent.redact.redact_sensitive_text(text, force)
 *   owned_kanban_task         -> agent.delegation_context.owned_kanban_task()
 *   registry.get_schema       -> tools.registry.registry.get_schema(tool_name)
 *   inner_fn                  -> the wrapped handler fn(args)
 *   logger.exception          -> logger.exception(message)
 *   kb.latest_run / kb.get_task -> hermes_cli.kanban_db record reads
 *
 * os.environ is modeled by io->state["env"] (object of string values; an
 * absent key is an unset variable). Already-ported siblings
 * (_is_delegated_child_context, _default_task_id,
 * _reject_delegated_child_mutation, _enforce_worker_task_ownership,
 * _worker_run_id, _ok) stay REAL here as unclaimed internal helpers (their
 * PoP claims live in src/tools/kanban_tools.c) and in the oracle
 * (AST-extracted), so the nested resolution chains are exercised.
 *
 * Deterministic subset (documented): JSON numbers lose int/float identity
 * after parsing — integral doubles render as ints (str/dumps/type-name);
 * float repr covers the shortest round-trip decimal forms (no NaN/Inf,
 * which libjson cannot parse); str()/repr() of containers cover ASCII plus
 * printable non-ASCII (non-printable Unicode escapes out of fixture scope);
 * int() string parsing covers ASCII digits, signs, whitespace and
 * underscores (Unicode decimal digits and >2^53 magnitudes out of scope);
 * record doubles are non-empty objects or None (an empty {} record would be
 * truthy live as a dataclass but falsy as JSON — out of fixture scope);
 * tuples are modeled as lists; _merge_artifacts mutates a private copy of
 * the input dict (the live in-place mutation is unobservable through ret).
 * The _kanban_handler decorator closure is projected to one wrapper
 * execution: the wrapped fn is the inner_fn seam, kw is empty.
 *
 * Ownership: seam results and function returns are caller-owned (json_free).
 * io->trace / io->state belong to the harness. Args passed to ktf_seam are
 * consumed (copied into the trace event, then freed). Borrowed inputs are
 * never freed. A NULL json_t* input models Python None.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* Live module constants (tools/kanban_tools.py). */
#define KTF_LIST_DEFAULT_LIMIT 50
#define KTF_LIST_MAX_LIMIT 200

typedef struct ktf_io_t {
    json_t *(*call)(struct ktf_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of event objects: ordered decision sequence */
    json_t *state;   /* JSON object: env mirror + per-case flags */
    void   *ctx;     /* harness backend (scripted results) */
} ktf_io_t;

static bool ktf_raised(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *ktf_raise_kind(const json_t *v) {
    return json_get_str(v, "__raise__", "");
}

static const char *ktf_raise_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}

static json_t *ktf_raise(const char *type, const char *message) {
    json_t *e = json_object();
    json_set(e, "__raise__", json_string(type));
    json_set(e, "message", json_string(message ? message : ""));
    return e;
}

/* BaseException-only kinds pass through every `except Exception` site. */
static bool ktf_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 ||
           strcmp(kind, "KeyboardInterrupt") == 0 ||
           strcmp(kind, "GeneratorExit") == 0;
}

/* Record {"call": name, **args} and dispatch; consumes args. */
static json_t *ktf_seam(ktf_io_t *io, const char *name, json_t *args) {
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
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    if (args) json_free(args);
    return r ? r : json_null();
}

/* ── Python-faithful micro helpers ───────────────────────────────────────── */
static bool ktf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static bool ktf_truthy(const json_t *v) {
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

static char *ktf_xstrdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *out = malloc(n + 1);
    if (out) memcpy(out, s ? s : "", n + 1);
    return out;
}

/* UTF-8 decode one code point at *p; advances *p. */
static unsigned long ktf_next_cp(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned long cp = s[0];
    int extra = 0;
    if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    for (int i = 1; i <= extra && s[i]; i++)
        cp = (cp << 6) | (s[i] & 0x3F);
    *p = s + 1 + extra;
    return cp;
}

/* Python str.isspace() set (ASCII + Unicode White_Space). */
static bool ktf_is_space_cp(unsigned long cp) {
    if (cp <= 0x7F)
        return (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F) || cp == 0x20;
    return cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* Python str.strip() (both ends, Unicode White_Space). Caller frees. */
static char *ktf_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *a = p, *b = p + strlen((const char *)p);
    while (a < b) {
        const unsigned char *q = a;
        unsigned long cp = ktf_next_cp(&q);
        if (!ktf_is_space_cp(cp)) break;
        a = q;
    }
    while (b > a) {
        const unsigned char *q = b - 1;
        while (q > a && (*q & 0xC0) == 0x80) q--;
        const unsigned char *r = q;
        unsigned long cp = ktf_next_cp(&r);
        if (!ktf_is_space_cp(cp) || r != b) break;
        b = q;
    }
    size_t n = (size_t)(b - a);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* type(value).__name__ (integral doubles read as int — identity lost). */
static const char *ktf_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == floor(v->num_val)) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python float repr: shortest round-trip digits, positional for decimal
 * exponent in [-4, 16), scientific otherwise (1e+16 / 1e-05 shapes). */
static char *ktf_float_repr(double v) {
    if (v == 0.0) return ktf_xstrdup(signbit(v) ? "-0.0" : "0.0");
    char buf[64];
    int prec;
    for (prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*e", prec - 1, v);
        if (strtod(buf, NULL) == v) break;
    }
    /* buf = [-]d[.ddd]e[+-]XX */
    const char *p = buf;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    char digits[32];
    size_t nd = 0;
    while (*p && *p != 'e' && *p != 'E') {
        if (*p >= '0' && *p <= '9') digits[nd++] = *p;
        p++;
    }
    digits[nd] = '\0';
    int e = (*p == 'e' || *p == 'E') ? atoi(p + 1) : 0;
    while (nd > 1 && digits[nd - 1] == '0') digits[--nd] = '\0';
    char out[512];
    size_t o = 0;
    if (neg) out[o++] = '-';
    int n = (int)nd;
    if (e >= -4 && e < 16) {
        if (e >= n - 1) {
            memcpy(out + o, digits, nd); o += nd;
            for (int i = 0; i < e - n + 1; i++) out[o++] = '0';
            out[o++] = '.'; out[o++] = '0';
        } else if (e >= 0) {
            memcpy(out + o, digits, (size_t)(e + 1)); o += (size_t)(e + 1);
            out[o++] = '.';
            memcpy(out + o, digits + e + 1, (size_t)(n - e - 1)); o += (size_t)(n - e - 1);
        } else {
            out[o++] = '0'; out[o++] = '.';
            for (int i = 0; i < -e - 1; i++) out[o++] = '0';
            memcpy(out + o, digits, nd); o += nd;
        }
    } else {
        out[o++] = digits[0];
        if (n > 1) {
            out[o++] = '.';
            memcpy(out + o, digits + 1, (size_t)(n - 1)); o += (size_t)(n - 1);
        }
        out[o++] = 'e';
        out[o++] = (e < 0) ? '-' : '+';
        int ae = (e < 0) ? -e : e;
        char eb[8];
        snprintf(eb, sizeof eb, "%02d", ae);
        size_t el = strlen(eb);
        memcpy(out + o, eb, el); o += el;
    }
    out[o] = '\0';
    return ktf_xstrdup(out);
}

/* Python str() of a JSON value (integral doubles render as ints). */
static char *ktf_py_str(const json_t *v);

/* Python repr() of a string: quote choice + escapes. Caller frees. */
static char *ktf_repr_str(const char *s) {
    const char *str = s ? s : "";
    bool has_sq = strchr(str, '\'') != NULL;
    bool has_dq = strchr(str, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    size_t cap = strlen(str) * 4 + 3;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = q;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20 || c == 0x7F) {
            o += (size_t)snprintf(out + o, cap - o, "\\x%02x", c);
        } else out[o++] = (char)c;  /* printable UTF-8 passes through */
    }
    out[o++] = q;
    out[o] = '\0';
    return out;
}

/* Python repr() of a JSON value (containers use repr of elements). */
static char *ktf_py_repr(const json_t *v) {
    if (v && v->type == JSON_STRING) return ktf_repr_str(v->str_val);
    return ktf_py_str(v);
}

static char *ktf_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ktf_xstrdup("None");
    switch (v->type) {
        case JSON_BOOL: return ktf_xstrdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            if (v->num_val == floor(v->num_val)) {
                char buf[64];
                snprintf(buf, sizeof buf, "%.0f", v->num_val);
                return ktf_xstrdup(buf);
            }
            return ktf_float_repr(v->num_val);
        }
        case JSON_STRING: return ktf_xstrdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            size_t cap = 3;
            char **parts = calloc(v->c.count ? v->c.count : 1, sizeof(char *));
            for (size_t i = 0; i < v->c.count; i++) {
                parts[i] = ktf_py_repr(v->c.items[i]);
                cap += strlen(parts[i]) + 2;
            }
            char *out = malloc(cap);
            size_t o = 0;
            out[o++] = '[';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) { out[o++] = ','; out[o++] = ' '; }
                size_t l = strlen(parts[i]);
                memcpy(out + o, parts[i], l); o += l;
                free(parts[i]);
            }
            out[o++] = ']'; out[o] = '\0';
            free(parts);
            return out;
        }
        case JSON_OBJECT: {
            size_t cap = 3;
            char **kp = calloc(v->c.count ? v->c.count : 1, sizeof(char *));
            char **vp = calloc(v->c.count ? v->c.count : 1, sizeof(char *));
            for (size_t i = 0; i < v->c.count; i++) {
                kp[i] = ktf_repr_str(v->c.keys[i]);
                vp[i] = ktf_py_repr(v->c.items[i]);
                cap += strlen(kp[i]) + strlen(vp[i]) + 4;
            }
            char *out = malloc(cap);
            size_t o = 0;
            out[o++] = '{';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) { out[o++] = ','; out[o++] = ' '; }
                size_t l = strlen(kp[i]);
                memcpy(out + o, kp[i], l); o += l;
                out[o++] = ':'; out[o++] = ' ';
                l = strlen(vp[i]);
                memcpy(out + o, vp[i], l); o += l;
                free(kp[i]); free(vp[i]);
            }
            out[o++] = '}'; out[o] = '\0';
            free(kp); free(vp);
            return out;
        }
        default: return ktf_xstrdup("");
    }
}

/* json.dumps() defaults: ", " / ": " separators, ensure_ascii=True. */
static void ktf_dumps_str(const char *s, char **buf, size_t *o, size_t *cap) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    /* worst case: every byte -> \uXXXX (6) */
    size_t need = *o + strlen((const char *)p) * 6 + 3;
    if (need > *cap) {
        *cap = need * 2;
        *buf = realloc(*buf, *cap);
    }
    char *out = *buf;
    size_t k = *o;
    out[k++] = '"';
    while (*p) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { out[k++] = '\\'; out[k++] = (char)c; p++; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; p++; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; p++; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; p++; }
        else if (c == '\b') { out[k++] = '\\'; out[k++] = 'b'; p++; }
        else if (c == '\f') { out[k++] = '\\'; out[k++] = 'f'; p++; }
        else if (c < 0x20) {
            k += (size_t)snprintf(out + k, *cap - k, "\\u%04x", c);
            p++;
        } else if (c < 0x80) { out[k++] = (char)c; p++; }
        else {
            const unsigned char *q = p;
            unsigned long cp = ktf_next_cp(&q);
            if (cp > 0xFFFF) {
                unsigned long v = cp - 0x10000;
                k += (size_t)snprintf(out + k, *cap - k, "\\u%04x\\u%04x",
                    (unsigned)(0xD800 + (v >> 10)), (unsigned)(0xDC00 + (v & 0x3FF)));
            } else {
                k += (size_t)snprintf(out + k, *cap - k, "\\u%04x", (unsigned)cp);
            }
            p = q;
        }
    }
    out[k++] = '"';
    out[k] = '\0';
    *o = k;
}

static void ktf_dumps_rec(const json_t *v, char **buf, size_t *o, size_t *cap) {
    size_t need = *o + 64;
    if (need > *cap) { *cap = need * 2; *buf = realloc(*buf, *cap); }
    char *out = *buf;
    if (!v || v->type == JSON_NULL) { memcpy(out + *o, "null", 5); *o += 4; out[*o] = '\0'; return; }
    switch (v->type) {
        case JSON_BOOL: {
            const char *s = v->bool_val ? "true" : "false";
            size_t l = strlen(s);
            memcpy(out + *o, s, l + 1); *o += l;
            return;
        }
        case JSON_NUMBER: {
            char tmp[64];
            if (v->num_val == floor(v->num_val)) snprintf(tmp, sizeof tmp, "%.0f", v->num_val);
            else {
                char *fr = ktf_float_repr(v->num_val);
                snprintf(tmp, sizeof tmp, "%s", fr);
                free(fr);
            }
            size_t l = strlen(tmp);
            if (*o + l + 1 > *cap) { *cap = (*o + l + 1) * 2; *buf = realloc(*buf, *cap); out = *buf; }
            memcpy(out + *o, tmp, l + 1); *o += l;
            return;
        }
        case JSON_STRING: ktf_dumps_str(v->str_val, buf, o, cap); return;
        case JSON_ARRAY: {
            if (*o + 2 > *cap) { *cap = (*o + 2) * 2; *buf = realloc(*buf, *cap); out = *buf; }
            out[(*o)++] = '[';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) {
                    if (*o + 3 > *cap) { *cap = (*o + 3) * 2; *buf = realloc(*buf, *cap); out = *buf; }
                    out[(*o)++] = ','; out[(*o)++] = ' ';
                }
                ktf_dumps_rec(v->c.items[i], buf, o, cap);
                out = *buf;
            }
            if (*o + 2 > *cap) { *cap = (*o + 2) * 2; *buf = realloc(*buf, *cap); out = *buf; }
            out[(*o)++] = ']'; out[*o] = '\0';
            return;
        }
        case JSON_OBJECT: {
            if (*o + 2 > *cap) { *cap = (*o + 2) * 2; *buf = realloc(*buf, *cap); out = *buf; }
            out[(*o)++] = '{';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) {
                    if (*o + 3 > *cap) { *cap = (*o + 3) * 2; *buf = realloc(*buf, *cap); out = *buf; }
                    out[(*o)++] = ','; out[(*o)++] = ' ';
                }
                ktf_dumps_str(v->c.keys[i], buf, o, cap);
                out = *buf;
                if (*o + 3 > *cap) { *cap = (*o + 3) * 2; *buf = realloc(*buf, *cap); out = *buf; }
                out[(*o)++] = ':'; out[(*o)++] = ' ';
                ktf_dumps_rec(v->c.items[i], buf, o, cap);
                out = *buf;
            }
            if (*o + 2 > *cap) { *cap = (*o + 2) * 2; *buf = realloc(*buf, *cap); out = *buf; }
            out[(*o)++] = '}'; out[*o] = '\0';
            return;
        }
        default: return;
    }
}

/* json.dumps(v) with Python defaults. Caller frees. */
static char *ktf_json_dumps_py(const json_t *v) {
    size_t cap = 256, o = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    ktf_dumps_rec(v, &buf, &o, &cap);
    return buf;
}

/* int(str): strip, optional sign, ASCII digits with underscores BETWEEN
 * digits. Returns a number node or a ValueError raise spec. */
static json_t *ktf_int_parse(const char *raw) {
    char *s = ktf_strip(raw ? raw : "");
    const char *p = s;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    long long acc = 0;
    bool any = false, prev_digit = false, ok = true;
    for (; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            any = true; prev_digit = true;
        } else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { ok = false; break; }
            prev_digit = false;
        } else { ok = false; break; }
    }
    if (!ok || !any || !prev_digit) {
        char *r = ktf_repr_str(raw ? raw : "");
        size_t n = strlen(r) + 64;
        char *msg = malloc(n);
        snprintf(msg, n, "invalid literal for int() with base 10: %s", r);
        json_t *e = ktf_raise("ValueError", msg);
        free(msg); free(r); free(s);
        return e;
    }
    free(s);
    return json_number(neg ? (double)-acc : (double)acc);
}

/* sorted() of an object's keys (UTF-8 byte order == code point order).
 * Caller frees the array and each string. */
static char **ktf_sorted_keys(const json_t *obj, size_t *count) {
    size_t n = (obj && obj->type == JSON_OBJECT) ? obj->c.count : 0;
    char **keys = calloc(n ? n : 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) keys[i] = ktf_xstrdup(obj->c.keys[i]);
    for (size_t i = 1; i < n; i++) {
        char *k = keys[i];
        size_t j = i;
        while (j > 0 && strcmp(keys[j - 1], k) > 0) { keys[j] = keys[j - 1]; j--; }
        keys[j] = k;
    }
    *count = n;
    return keys;
}

static bool ktf_str_in(const char *s, char *const *arr, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(arr[i], s) == 0) return true;
    return false;
}

/* os.environ modeled by io->state["env"]. Borrowed result (NULL == unset). */
static json_t *ktf_env_get(ktf_io_t *io, const char *key) {
    json_t *env = io ? json_obj_get(io->state, "env") : NULL;
    if (!env || env->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(env, key);
    return (v && v->type == JSON_STRING) ? v : NULL;
}

static const char *ktf_env_str(ktf_io_t *io, const char *key) {
    json_t *v = ktf_env_get(io, key);
    return v ? v->str_val : NULL;
}

/* ── gating ──────────────────────────────────────────────────────────────── */

/* PoP: ktf_delegation_ctx @ tools/kanban_tools.py:_delegation_ctx */
json_t *ktf_delegation_ctx(ktf_io_t *io, const char *predicate, bool default_val) {
    /* Import failure is modeled by a state flag: live `from agent import
     * delegation_context` raises before any call (no trace event). */
    if (json_get_bool(io ? io->state : NULL, "delegation_import_fail", false))
        return json_bool(default_val);
    json_t *args = json_object();
    json_set(args, "predicate", json_string(predicate));
    json_t *r = ktf_seam(io, "delegation_ctx", args);
    if (ktf_raised(r)) {
        if (ktf_is_base_exc(ktf_raise_kind(r))) return r;
        json_free(r);
        return json_bool(default_val);
    }
    return r;
}

/* PoP: ktf_is_dispatcher_owned_worker @ tools/kanban_tools.py:_is_dispatcher_owned_worker */
json_t *ktf_is_dispatcher_owned_worker(ktf_io_t *io) {
    return ktf_delegation_ctx(io, "is_dispatcher_owned_worker_context", true);
}

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c). */
static json_t *ktf_is_delegated_child_context(ktf_io_t *io) {
    return ktf_delegation_ctx(io, "is_delegated_child_context", false);
}

/* PoP: ktf_visible @ tools/kanban_tools.py:_visible */
json_t *ktf_visible(ktf_io_t *io, bool to_env_worker) {
    json_t *dc = ktf_is_delegated_child_context(io);
    if (ktf_raised(dc)) return dc;
    bool delegated = ktf_truthy(dc);
    json_free(dc);
    if (delegated) return json_bool(false);
    const char *env_tid = ktf_env_str(io, "HERMES_KANBAN_TASK");
    if (env_tid && env_tid[0]) {
        json_t *ow = ktf_is_dispatcher_owned_worker(io);
        if (ktf_raised(ow)) return ow;
        bool owned = ktf_truthy(ow);
        json_free(ow);
        if (owned) return json_bool(to_env_worker);
    }
    return ktf_seam(io, "profile_has_kanban_toolset", json_object());
}

/* ── validation leaves ───────────────────────────────────────────────────── */

/* _Reject(message): args[0] = tool_error(message); str(e) == that payload. */
static json_t *ktf_reject(ktf_io_t *io, const char *message) {
    json_t *args = json_object();
    json_set(args, "message", json_string(message));
    json_t *payload = ktf_seam(io, "tool_error", args);
    if (ktf_raised(payload)) return payload;
    char *s = ktf_py_str(payload);
    json_t *e = ktf_raise("_Reject", s);
    free(s);
    json_free(payload);
    return e;
}

/* PoP: ktf_check @ tools/kanban_tools.py:_check */
json_t *ktf_check(ktf_io_t *io, const json_t *cond, const char *message) {
    if (ktf_truthy(cond)) return json_null();
    return ktf_reject(io, message);
}

/* PoP: ktf_require_text @ tools/kanban_tools.py:_require_text */
json_t *ktf_require_text(ktf_io_t *io, const json_t *args, const char *name,
                         const char *message) {
    json_t *value = json_obj_get(args, name);
    bool ok = false;
    if (ktf_truthy(value)) {
        char *s = ktf_py_str(value);
        char *st = ktf_strip(s);
        ok = st && st[0] != '\0';
        free(s);
        free(st);
    }
    if (!ok) {
        /* message or f"{name} is required" — an empty message falls back. */
        char *def = NULL;
        const char *m = message;
        if (!m || !m[0]) {
            def = malloc(strlen(name) + 16);
            sprintf(def, "%s is required", name);
            m = def;
        }
        json_t *e = ktf_reject(io, m);
        free(def);
        return e;
    }
    return value ? json_copy(value) : json_null();
}

/* PoP: ktf_coerce_str_list @ tools/kanban_tools.py:_coerce_str_list */
json_t *ktf_coerce_str_list(ktf_io_t *io, const json_t *value, const char *name,
                            const char *what, bool strip) {
    if (ktf_is_none(value)) return json_null();
    json_t *v = (json_t *)value;
    bool wrapped = false;
    if (v->type == JSON_STRING) {
        v = json_array();
        json_append(v, json_copy(value));
        wrapped = true;
    }
    if (v->type != JSON_ARRAY) {  /* tuples are modeled as lists */
        char *msg = malloc(strlen(name) + strlen(what) + 64);
        sprintf(msg, "%s must be a list of %s, got %s", name, what,
                ktf_type_name(value));
        json_t *e = ktf_reject(io, msg);
        free(msg);
        if (wrapped) json_free(v);
        return e;
    }
    if (!strip) {
        json_t *out = json_copy(v);
        if (wrapped) json_free(v);
        return out;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < v->c.count; i++) {
        char *s = ktf_py_str(v->c.items[i]);
        char *st = ktf_strip(s);
        if (st && st[0]) json_append(out, json_string(st));
        free(s);
        free(st);
    }
    if (wrapped) json_free(v);
    return out;
}

/* PoP: ktf_require_dict_metadata @ tools/kanban_tools.py:_require_dict_metadata */
json_t *ktf_require_dict_metadata(ktf_io_t *io, const json_t *metadata) {
    bool ok = ktf_is_none(metadata) || metadata->type == JSON_OBJECT;
    if (ok) return json_null();
    char *msg = malloc(strlen(ktf_type_name(metadata)) + 64);
    sprintf(msg, "metadata must be an object/dict, got %s", ktf_type_name(metadata));
    json_t *e = ktf_reject(io, msg);
    free(msg);
    return e;
}

/* PoP: ktf_merge_artifacts @ tools/kanban_tools.py:_merge_artifacts */
json_t *ktf_merge_artifacts(ktf_io_t *io, const json_t *metadata,
                            const json_t *artifacts) {
    json_t *chk = ktf_require_dict_metadata(io, metadata);
    if (ktf_raised(chk)) return chk;
    json_free(chk);
    /* Live mutates the caller's dict in place; the mutation is observable
     * only through the returned dict, so a private copy is equivalent. */
    json_t *m = ktf_is_none(metadata) ? json_object() : json_copy(metadata);
    json_t *existing = json_obj_get(m, "artifacts");
    if (existing && existing->type == JSON_ARRAY) {
        json_t *merged = json_array();
        const json_t *srcs[2] = { existing, artifacts };
        for (int s = 0; s < 2; s++) {
            const json_t *src = srcs[s];
            if (!src || src->type != JSON_ARRAY) continue;
            for (size_t i = 0; i < src->c.count; i++) {
                char *str = ktf_py_str(src->c.items[i]);
                char *st = ktf_strip(str);
                if (st && st[0]) {
                    bool seen = false;
                    for (size_t j = 0; j < merged->c.count; j++) {
                        json_t *prev = merged->c.items[j];
                        if (prev->type == JSON_STRING &&
                            strcmp(prev->str_val, st) == 0) { seen = true; break; }
                    }
                    if (!seen) json_append(merged, json_string(st));
                }
                free(str);
                free(st);
            }
        }
        json_set(m, "artifacts", merged);
    } else {
        json_set(m, "artifacts",
                 (artifacts && artifacts->type == JSON_ARRAY)
                     ? json_copy(artifacts) : json_array());
    }
    return m;
}

/* PoP: ktf_opt_int @ tools/kanban_tools.py:_opt_int */
json_t *ktf_opt_int(const json_t *value, const json_t *default_val) {
    if (ktf_is_none(value))
        return default_val ? json_copy(default_val) : json_null();
    switch (value->type) {
        case JSON_BOOL: return json_number(value->bool_val ? 1.0 : 0.0);
        case JSON_NUMBER: return json_number(trunc(value->num_val));
        case JSON_STRING: return ktf_int_parse(value->str_val);
        default: {
            char *msg = malloc(strlen(ktf_type_name(value)) + 96);
            sprintf(msg, "int() argument must be a string, a bytes-like object "
                         "or a real number, not '%s'", ktf_type_name(value));
            json_t *e = ktf_raise("TypeError", msg);
            free(msg);
            return e;
        }
    }
}

/* ── record projection + redaction ───────────────────────────────────────── */

/* Attribute access on a record double: dicts become types.SimpleNamespace
 * in the oracle; a missing attribute raises AttributeError. */
static json_t *ktf_getattr(const json_t *obj, const char *name) {
    json_t *v = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, name) : NULL;
    if (v) return json_copy(v);
    const char *tn = (obj && obj->type == JSON_OBJECT)
                         ? "types.SimpleNamespace" : ktf_type_name(obj);
    char *msg = malloc(strlen(tn) + strlen(name) + 64);
    sprintf(msg, "'%s' object has no attribute '%s'", tn, name);
    json_t *e = ktf_raise("AttributeError", msg);
    free(msg);
    return e;
}

/* PoP: ktf_fields @ tools/kanban_tools.py:_fields */
json_t *ktf_fields(const json_t *obj, const json_t *names) {
    json_t *out = json_object();
    size_t n = (names && names->type == JSON_ARRAY) ? names->c.count : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *nm = names->c.items[i];
        const char *name = (nm && nm->type == JSON_STRING) ? nm->str_val : "";
        if (ktf_is_none(obj)) {
            json_set(out, name, json_null());
            continue;
        }
        json_t *v = ktf_getattr(obj, name);
        if (ktf_raised(v)) {
            json_free(out);
            return v;
        }
        json_set(out, name, v);
    }
    return out;
}

/* PoP: ktf_redact @ tools/kanban_tools.py:_redact */
json_t *ktf_redact(ktf_io_t *io, const json_t *value) {
    char *s = ktf_py_str(value);
    json_t *args = json_object();
    json_set(args, "text", json_string(s));
    json_set(args, "force", json_bool(true));
    free(s);
    return ktf_seam(io, "redact_sensitive_text", args);
}

/* PoP: ktf_redact_opt @ tools/kanban_tools.py:_redact_opt */
json_t *ktf_redact_opt(ktf_io_t *io, const json_t *value) {
    if (!ktf_truthy(value))
        return value ? json_copy(value) : json_null();
    return ktf_redact(io, value);
}

/* ── task-id resolution + ownership gates ────────────────────────────────── */

/* PoP: ktf_own_task_env @ tools/kanban_tools.py:_own_task_env */
json_t *ktf_own_task_env(ktf_io_t *io, const char *task_id, const char *var) {
    if (!task_id || !task_id[0]) return json_null();
    json_t *owned = ktf_seam(io, "owned_kanban_task", json_object());
    if (ktf_raised(owned)) return owned;
    bool eq = owned->type == JSON_STRING && owned->str_val &&
              strcmp(owned->str_val, task_id) == 0;
    json_free(owned);
    if (!eq) return json_null();
    json_t *v = ktf_env_get(io, var);
    return v ? json_copy(v) : json_null();
}

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c). */
static json_t *ktf_worker_run_id(ktf_io_t *io, const char *tid) {
    json_t *raw = ktf_own_task_env(io, tid, "HERMES_KANBAN_RUN_ID");
    if (ktf_raised(raw)) return raw;
    if (!ktf_truthy(raw)) {
        json_free(raw);
        return json_null();
    }
    json_t *n = ktf_int_parse(raw->type == JSON_STRING ? raw->str_val : "");
    json_free(raw);
    if (ktf_raised(n)) {
        if (strcmp(ktf_raise_kind(n), "ValueError") == 0) {
            json_free(n);
            return json_null();
        }
        return n;
    }
    return n;
}

/* PoP: ktf_reject_inherited_task_mutation @ tools/kanban_tools.py:_reject_inherited_task_mutation */
json_t *ktf_reject_inherited_task_mutation(ktf_io_t *io, const char *tid,
                                           const json_t *env_tid) {
    bool env_truthy, eq;
    char *stripped = NULL;
    if (env_tid && env_tid->type != JSON_NULL) {
        /* Explicit env_tid (even "") is used as-is: no env read, no strip. */
        env_truthy = ktf_truthy(env_tid);
        eq = env_tid->type == JSON_STRING && env_tid->str_val &&
             strcmp(tid, env_tid->str_val) == 0;
    } else {
        const char *raw = ktf_env_str(io, "HERMES_KANBAN_TASK");
        stripped = ktf_strip(raw ? raw : "");
        env_truthy = stripped && stripped[0] != '\0';
        eq = env_truthy && strcmp(tid, stripped) == 0;
    }
    json_t *out = json_null();
    if (env_truthy && eq) {
        json_t *owned = ktf_seam(io, "owned_kanban_task", json_object());
        if (ktf_raised(owned)) { free(stripped); return owned; }
        bool owned_truthy = ktf_truthy(owned);
        json_free(owned);
        if (!owned_truthy) {
            const char *tail =
                ": this run inherited it from the Kanban worker that fired it "
                "and does not own it. Report findings in this run's output or "
                "file a new card with kanban_create; only the card's own worker "
                "may change it.";
            char *msg = malloc(strlen(tid) + strlen(tail) + 32);
            sprintf(msg, "refusing to mutate task %s%s", tid, tail);
            out = ktf_reject(io, msg);
            free(msg);
        }
    }
    free(stripped);
    return out;
}

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c). */
static json_t *ktf_default_task_id(ktf_io_t *io, const json_t *args) {
    json_t *arg = json_obj_get(args, "task_id");
    if (!ktf_is_none(arg)) {
        char *s = ktf_py_str(arg);
        char *st = ktf_strip(s);
        if (st && st[0]) {
            json_t *r = json_string(st);
            free(s); free(st);
            return r;
        }
        free(s); free(st);
    }
    json_t *dc = ktf_is_delegated_child_context(io);
    if (ktf_raised(dc)) return dc;
    bool delegated = ktf_truthy(dc);
    json_free(dc);
    if (delegated) return json_null();
    json_t *ow = ktf_is_dispatcher_owned_worker(io);
    if (ktf_raised(ow)) return ow;
    bool owned = ktf_truthy(ow);
    json_free(ow);
    if (!owned) return json_null();
    const char *env_tid = ktf_env_str(io, "HERMES_KANBAN_TASK");
    if (env_tid && env_tid[0]) {
        char *st = ktf_strip(env_tid);
        if (st && st[0]) {
            json_t *r = json_string(st);
            free(st);
            return r;
        }
        free(st);
    }
    return json_null();
}

/* PoP: ktf_require_task_id @ tools/kanban_tools.py:_require_task_id */
json_t *ktf_require_task_id(ktf_io_t *io, const json_t *args) {
    json_t *tid = ktf_default_task_id(io, args);
    if (ktf_raised(tid)) return tid;
    if (ktf_truthy(tid)) return tid;
    json_free(tid);
    const char *env_tid = ktf_env_str(io, "HERMES_KANBAN_TASK");
    if (env_tid && env_tid[0])
        return ktf_reject(io,
            "task_id is required: this session does not own the inherited "
            "HERMES_KANBAN_TASK, so it is not a valid default. Pass an explicit "
            "task_id.");
    return ktf_reject(io,
        "task_id is required: this session has no dispatcher-assigned task to "
        "default to. Pass an explicit task_id; discover task ids with "
        "kanban_list.");
}

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c). */
static json_t *ktf_reject_delegated_child_mutation(ktf_io_t *io,
                                                   const char *tool_name) {
    json_t *dc = ktf_delegation_ctx(io, "is_delegated_child_process_context", false);
    if (ktf_raised(dc)) return dc;
    bool delegated = ktf_truthy(dc);
    json_free(dc);
    if (!delegated) return json_null();
    const char *tail =
        " refused: delegate_task child agents are not Kanban run owners. "
        "Return findings to the parent agent; the dispatcher worker or an "
        "explicitly configured Kanban orchestrator must perform board "
        "mutations.";
    char *msg = malloc(strlen(tool_name) + strlen(tail) + 2);
    sprintf(msg, "%s%s", tool_name, tail);
    json_t *e = ktf_reject(io, msg);
    free(msg);
    return e;
}

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c). */
static json_t *ktf_enforce_worker_task_ownership(ktf_io_t *io, const char *tid) {
    const char *raw = ktf_env_str(io, "HERMES_KANBAN_TASK");
    char *env_tid = ktf_strip(raw ? raw : "");
    json_t *out = json_null();
    if (env_tid && env_tid[0]) {
        json_t *owned = ktf_seam(io, "owned_kanban_task", json_object());
        if (ktf_raised(owned)) { free(env_tid); return owned; }
        bool owned_truthy = ktf_truthy(owned);
        json_free(owned);
        if (!owned_truthy) {
            /* Inherited, not owned: the worker's card is read-only here. */
            json_t *explicit = json_string(env_tid);
            out = ktf_reject_inherited_task_mutation(io, tid, explicit);
            json_free(explicit);
            free(env_tid);
            return out;
        }
        if (strcmp(tid, env_tid) != 0) {
            char *msg = malloc(strlen(tid) + strlen(env_tid) + 256);
            sprintf(msg,
                "worker is scoped to task %s; refusing to mutate %s. Use "
                "kanban_comment to hand off information to other tasks, or "
                "kanban_create to spawn follow-up work.", env_tid, tid);
            out = ktf_reject(io, msg);
            free(msg);
        }
    }
    free(env_tid);
    return out;
}

/* _RUN_LIFECYCLE_TOOLS (membership only; order irrelevant). */
static bool ktf_run_lifecycle_tool(const char *tool_name) {
    static const char *const tools[] = {
        "kanban_complete", "kanban_block", "kanban_schedule",
        "kanban_request_review", "kanban_request_changes",
    };
    for (size_t i = 0; i < sizeof tools / sizeof tools[0]; i++)
        if (strcmp(tool_name, tools[i]) == 0) return true;
    return false;
}

/* PoP: ktf_worker_guard @ tools/kanban_tools.py:_worker_guard */
json_t *ktf_worker_guard(ktf_io_t *io, const char *tool_name, const json_t *args) {
    json_t *r = ktf_reject_delegated_child_mutation(io, tool_name);
    if (ktf_raised(r)) return r;
    json_free(r);
    json_t *tid = ktf_require_task_id(io, args);
    if (ktf_raised(tid)) return tid;
    r = ktf_enforce_worker_task_ownership(io, tid->str_val ? tid->str_val : "");
    if (ktf_raised(r)) { json_free(tid); return r; }
    json_free(r);
    if (ktf_run_lifecycle_tool(tool_name)) {
        json_t *owned = ktf_seam(io, "owned_kanban_task", json_object());
        if (ktf_raised(owned)) { json_free(tid); return owned; }
        bool owned_truthy = ktf_truthy(owned);
        json_free(owned);
        if (owned_truthy) {
            json_t *rid = ktf_worker_run_id(io, tid->str_val ? tid->str_val : "");
            if (ktf_raised(rid)) { json_free(tid); return rid; }
            bool unbound = ktf_is_none(rid);
            json_free(rid);
            if (unbound) {
                json_free(tid);
                const char *tail =
                    " refused: this worker cannot resolve its "
                    "HERMES_KANBAN_RUN_ID, so it cannot prove ownership of the "
                    "card's current run. A stale or unbound worker must not "
                    "terminate a run a live successor owns. Re-run through the "
                    "dispatcher so the run id is pinned, or use an "
                    "orchestrator/CLI path that passes an explicit "
                    "expected_run_id.";
                char *msg = malloc(strlen(tool_name) + strlen(tail) + 2);
                sprintf(msg, "%s%s", tool_name, tail);
                json_t *e = ktf_reject(io, msg);
                free(msg);
                return e;
            }
        }
    }
    return tid;
}

/* ── the _kanban_handler wrapper flow ────────────────────────────────────── */

/* _UNDECLARED_ARGS: keys a handler reads that its schema does not declare. */
static bool ktf_undeclared_arg(const char *tool_name, const char *key) {
    if (strcmp(tool_name, "kanban_create") == 0)
        return strcmp(key, "session_id") == 0 || strcmp(key, "project_id") == 0;
    if (strcmp(tool_name, "kanban_attach_url") == 0)
        return strcmp(key, "title") == 0;
    return false;
}

/* obj[key] with the live subscript error surface. Borrowed result; on
 * failure *exc carries the raise spec (KeyError message == repr(key)). */
static json_t *ktf_subscript(json_t *obj, const char *key, json_t **exc) {
    *exc = NULL;
    if (!obj || obj->type == JSON_NULL) {
        *exc = ktf_raise("TypeError", "'NoneType' object is not subscriptable");
        return NULL;
    }
    if (obj->type == JSON_OBJECT) {
        json_t *v = json_obj_get(obj, key);
        if (!v) {
            char *r = ktf_repr_str(key);
            *exc = ktf_raise("KeyError", r);
            free(r);
        }
        return v;
    }
    if (obj->type == JSON_STRING) {
        *exc = ktf_raise("TypeError", "string indices must be integers, not 'str'");
        return NULL;
    }
    if (obj->type == JSON_ARRAY) {
        *exc = ktf_raise("TypeError",
                         "list indices must be integers or slices, not str");
        return NULL;
    }
    char *msg = malloc(strlen(ktf_type_name(obj)) + 64);
    sprintf(msg, "'%s' object is not subscriptable", ktf_type_name(obj));
    *exc = ktf_raise("TypeError", msg);
    free(msg);
    return NULL;
}

/* PoP: ktf_kanban_handler @ tools/kanban_tools.py:_kanban_handler
 * Projection: one execution of the decorator's wrapper(args); the wrapped
 * handler is the inner_fn seam and **kw is empty. */
json_t *ktf_kanban_handler(ktf_io_t *io, const char *tool_name,
                           const json_t *args) {
    json_t *schema = NULL, *exc = NULL, *ret = NULL;
    json_t *gargs = json_object();
    json_set(gargs, "tool_name", json_string(tool_name));
    schema = ktf_seam(io, "registry.get_schema", gargs);
    if (ktf_raised(schema)) { exc = schema; schema = NULL; goto catch; }
    json_t *params = ktf_subscript(schema, "parameters", &exc);
    if (exc) goto catch;
    json_t *props = ktf_subscript(params, "properties", &exc);
    if (exc) goto catch;
    if (!props || props->type != JSON_OBJECT) {
        /* set(properties) needs an iterable; fixture scope: objects only. */
        char *msg = malloc(strlen(ktf_type_name(props)) + 64);
        sprintf(msg, "'%s' object is not iterable", ktf_type_name(props));
        exc = ktf_raise("TypeError", msg);
        free(msg);
        goto catch;
    }
    {
        size_t nprops = 0, nargs = 0;
        char **pkeys = ktf_sorted_keys(props, &nprops);
        char **akeys = ktf_sorted_keys(args, &nargs);
        /* unknown = sorted(set(args) - allowed), allowed = props | undeclared */
        char **unknown = calloc(nargs ? nargs : 1, sizeof(char *));
        size_t nunk = 0;
        for (size_t i = 0; i < nargs; i++) {
            if (!ktf_str_in(akeys[i], pkeys, nprops) &&
                !ktf_undeclared_arg(tool_name, akeys[i]))
                unknown[nunk++] = akeys[i];
        }
        if (nunk > 0) {
            size_t cap = strlen(tool_name) + 64;
            for (size_t i = 0; i < nunk; i++) cap += strlen(unknown[i]) + 2;
            for (size_t i = 0; i < nprops; i++) cap += strlen(pkeys[i]) + 2;
            char *msg = malloc(cap);
            size_t o = (size_t)sprintf(msg, "%s: unknown parameter(s): ", tool_name);
            for (size_t i = 0; i < nunk; i++) {
                if (i) { msg[o++] = ','; msg[o++] = ' '; }
                size_t l = strlen(unknown[i]);
                memcpy(msg + o, unknown[i], l); o += l;
            }
            o += (size_t)sprintf(msg + o, ". Valid parameters: ");
            for (size_t i = 0; i < nprops; i++) {
                if (i) { msg[o++] = ','; msg[o++] = ' '; }
                size_t l = strlen(pkeys[i]);
                memcpy(msg + o, pkeys[i], l); o += l;
            }
            strcpy(msg + o, ". Nothing changed.");
            json_t *cond = json_bool(false);  /* not unknown == False */
            json_t *chk = ktf_check(io, cond, msg);
            json_free(cond);
            free(msg);
            if (ktf_raised(chk)) { exc = chk; goto catch_free; }
            json_free(chk);
        }
    catch_free:
        for (size_t i = 0; i < nprops; i++) free(pkeys[i]);
        free(pkeys);
        for (size_t i = 0; i < nargs; i++) free(akeys[i]);
        free(akeys);
        free(unknown);
        if (exc) goto catch;
    }
    {
        json_t *fargs = json_object();
        json_set(fargs, "args", args ? json_copy(args) : json_object());
        ret = ktf_seam(io, "inner_fn", fargs);
        if (ktf_raised(ret)) { exc = ret; ret = NULL; goto catch; }
    }
    goto done;

catch:
    if (strcmp(ktf_raise_kind(exc), "_Reject") == 0) {
        /* except _Reject as e: return e.args[0] (the finished payload). */
        ret = json_string(ktf_raise_msg(exc));
        json_free(exc);
        goto done;
    }
    if (ktf_is_base_exc(ktf_raise_kind(exc))) {
        ret = exc;  /* BaseException propagates through except Exception */
        goto done;
    }
    if (strcmp(ktf_raise_kind(exc), "ValueError") != 0) {
        json_t *largs = json_object();
        char *lmsg = malloc(strlen(tool_name) + 16);
        sprintf(lmsg, "%s failed", tool_name);
        json_set(largs, "message", json_string(lmsg));
        free(lmsg);
        json_t *lr = ktf_seam(io, "logger.exception", largs);
        if (ktf_raised(lr)) {
            json_free(exc);
            ret = lr;
            goto done;
        }
        json_free(lr);
    }
    {
        const char *em = ktf_raise_msg(exc);
        char *msg = malloc(strlen(tool_name) + strlen(em) + 4);
        sprintf(msg, "%s: %s", tool_name, em);
        json_t *te_args = json_object();
        json_set(te_args, "message", json_string(msg));
        free(msg);
        json_free(exc);
        ret = ktf_seam(io, "tool_error", te_args);
    }
done:
    if (schema) json_free(schema);
    return ret ? ret : json_null();
}

/* ── success formatting ──────────────────────────────────────────────────── */

/* Unclaimed internal helper (PoP claim lives in src/tools/kanban_tools.c):
 * _ok(**fields) = json.dumps({"ok": True, **fields}). Consumes fields. */
static json_t *ktf_ok(json_t *fields) {
    json_t *obj = json_object();
    json_set(obj, "ok", json_bool(true));
    if (fields && fields->type == JSON_OBJECT) {
        for (size_t i = 0; i < fields->c.count; i++)
            json_set(obj, fields->c.keys[i], json_copy(fields->c.items[i]));
    }
    json_free(fields);
    char *s = ktf_json_dumps_py(obj);
    json_free(obj);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: ktf_ok_landed @ tools/kanban_tools.py:_ok_landed */
json_t *ktf_ok_landed(ktf_io_t *io, const char *tid, const char *default_status,
                      const json_t *extra) {
    json_t *rargs = json_object();
    json_set(rargs, "task_id", json_string(tid));
    json_t *run = ktf_seam(io, "kb.latest_run", rargs);
    if (ktf_raised(run)) return run;
    json_t *largs = json_object();
    json_set(largs, "task_id", json_string(tid));
    json_t *landed = ktf_seam(io, "kb.get_task", largs);
    if (ktf_raised(landed)) { json_free(run); return landed; }
    /* run.id if run else None — truthiness, not `is not None`. */
    json_t *run_id;
    if (ktf_truthy(run)) {
        run_id = ktf_getattr(run, "id");
        if (ktf_raised(run_id)) { json_free(run); json_free(landed); return run_id; }
    } else {
        run_id = json_null();
    }
    json_t *status;
    if (ktf_truthy(landed)) {
        status = ktf_getattr(landed, "status");
        if (ktf_raised(status)) {
            json_free(run); json_free(landed); json_free(run_id);
            return status;
        }
    } else {
        status = json_string(default_status ? default_status : "");
    }
    json_t *fields = json_object();
    json_set(fields, "task_id", json_string(tid));
    json_set(fields, "run_id", run_id);
    json_set(fields, "status", status);
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(fields, extra->c.keys[i], json_copy(extra->c.items[i]));
    }
    json_free(run);
    json_free(landed);
    return ktf_ok(fields);
}
