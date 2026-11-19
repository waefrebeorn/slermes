/*
 * ws_revive1.c — deep-stub revival wave 1 (web_server_wrappers group 1).
 *
 * Faithful C ports of 17 hermes_cli web-server helpers, extracted from the
 * EMPTY-ALLOC stubs in web_server_wrappers.c.  Every external dependency
 * (config reads, HTTP, persistence, registries, clocks, logging) crosses the
 * injected universal seam:
 *
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 *
 * NULL return == Python None; {"__raise__": cls, "msg": m} == a raised
 * exception.  The seam OWNS args — callers must not free them.  BaseException
 * kinds (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) are
 * never swallowed by `except Exception` ports.
 *
 * libjson-only: no filesystem, no regex, no HTTP.  Path objects are modeled
 * as POSIX strings (the oracle injects an identical PathDouble); attribute
 * objects (pydantic bodies, provider plugins, catalog cards, process handles)
 * are modeled as JSON objects with the documented marker conventions below.
 *
 * Marker conventions (shared with tests/oracle_ws_revive1.py):
 *   - {"get_config_schema": "fn"}  -> provider double whose method calls the
 *     "provider_get_config_schema" seam; any other dict is a plain dict.
 *   - {"__callable__": true}       -> a callable value (never invoked here).
 *   - proc handle                  -> {"pid": int?, "poll": null|int};
 *     poll == null models a live child (Popen.poll() is None).
 *   - status_fn input "fn"         -> the zero-arg "status_fn" seam.
 *
 * Pinned constants are cross-checked fail-closed by the oracle against the
 * live quarry modules (drift => FATAL, never a silent mismatch).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"

/* strndup is not portable across MinGW configurations; local copy. */
static char *ws1_strndup(const char *s, size_t n)
{
    char *r = malloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

/* ================================================================
 * Seam plumbing
 * ================================================================ */

#ifndef WS1_SEAM_FN_DEFINED
#define WS1_SEAM_FN_DEFINED
typedef json_t *(*ws1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

static ws1_seam_fn ws1_seam_fn_p = NULL;
static void *ws1_seam_ctx_p = NULL;

void ws1_seams_install(ws1_seam_fn fn, void *ctx)
{
    ws1_seam_fn_p = fn;
    ws1_seam_ctx_p = ctx;
}

/* Per-case mutable state (borrowed; the harness owns it).  Keys:
 *   last_auto_archive_check  — _last_auto_archive_check dict
 *   last_gateway_restart     — _LAST_GATEWAY_RESTART tuple|null
 *   action_procs             — _gateway_mod._ACTION_PROCS
 *   action_commands          — _gateway_mod._ACTION_COMMANDS
 */
static json_t *ws1_state = NULL;

void ws1_state_install(json_t *st) { ws1_state = st; }

static json_t *ws1_raise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static int ws1_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static int ws1_is_base_exc(const json_t *v)
{
    if (!ws1_is_raise(v)) return 0;
    const char *k = json_obj_get(v, "__raise__")->str_val;
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

/* args is ADOPTED by the seam (caller must not free it). */
static json_t *ws1_call(const char *name, json_t *args)
{
    if (!ws1_seam_fn_p) {
        json_free(args);
        return ws1_raise("RuntimeError", "ws1 seam not installed");
    }
    return ws1_seam_fn_p(ws1_seam_ctx_p, name, args);
}

static json_t *ws1_args0(void) { return json_array(); }

static json_t *ws1_args1(json_t *a)
{
    json_t *args = json_array();
    json_append(args, a);
    return args;
}

static json_t *ws1_args2(json_t *a, json_t *b)
{
    json_t *args = json_array();
    json_append(args, a);
    json_append(args, b);
    return args;
}

static json_t *ws1_args3(json_t *a, json_t *b, json_t *c)
{
    json_t *args = json_array();
    json_append(args, a);
    json_append(args, b);
    json_append(args, c);
    return args;
}

static json_t *ws1_args4(const char *a, const char *b, json_t *c, json_t *d)
{
    json_t *args = json_array();
    json_append(args, json_string(a));
    json_append(args, json_string(b));
    json_append(args, c);
    json_append(args, d);
    return args;
}

/* ================================================================
 * Value helpers (Python semantics on JSON values)
 * ================================================================ */

static int ws1_jtruth(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int ws1_is_dict(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int ws1_is_list(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int ws1_is_str(const json_t *v)  { return v && v->type == JSON_STRING; }

static int ws1_num_is_int(const json_t *v)
{
    if (!v || v->type != JSON_NUMBER) return 0;
    double d = v->num_val;
    return isfinite(d) && d == floor(d) && fabs(d) < 9.2233720368547758e18;
}

/* Python type(value).__name__ for JSON-modelled values. */
static const char *ws1_type_name(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return ws1_num_is_int(v) ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* Python str()/repr() of a JSON number (shortest round-trip for floats). */
static char *ws1_num_str(double d)
{
    char buf[64];
    if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) {
        snprintf(buf, sizeof buf, "%lld", (long long)d);
        return strdup(buf);
    }
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* Python str() of a scalar JSON value.  Containers return NULL (the ported
 * surface never str()s a container; fixtures stay within that domain). */
static char *ws1_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER: return ws1_num_str(v->num_val);
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    default:          return NULL;
    }
}

/* str(v or "") — the ubiquitous guard idiom. */
static char *ws1_str_or_empty(const json_t *v)
{
    if (!ws1_jtruth(v)) return strdup("");
    char *s = ws1_pystr(v);
    return s ? s : strdup("");
}

/* ---- dynamic string buffer ---- */
typedef struct { char *p; size_t len, cap; } ws1_sb;

static void ws1_sb_init(ws1_sb *b) { b->cap = 64; b->len = 0; b->p = malloc(b->cap); b->p[0] = 0; }

static void ws1_sb_puts(ws1_sb *b, const char *s)
{
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = realloc(b->p, b->cap);
    }
    memcpy(b->p + b->len, s, n + 1);
    b->len += n;
}

static void ws1_sb_putc(ws1_sb *b, char c)
{
    if (b->len + 2 > b->cap) { b->cap *= 2; b->p = realloc(b->p, b->cap); }
    b->p[b->len++] = c;
    b->p[b->len] = 0;
}

static void ws1_sb_printf(ws1_sb *b, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    ws1_sb_puts(b, tmp);
}

static char *ws1_sb_take(ws1_sb *b) { char *p = b->p; b->p = NULL; b->len = b->cap = 0; return p; }

/* ---- ASCII string ops (fixtures stay ASCII; Python semantics) ---- */

static int ws1_py_isspace(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *ws1_strip(const char *s)
{
    if (!s) return strdup("");
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ws1_py_isspace((unsigned char)s[a])) a++;
    while (b > a && ws1_py_isspace((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    memcpy(r, s + a, b - a);
    r[b - a] = 0;
    return r;
}

static char *ws1_lower(const char *s)
{
    char *r = strdup(s ? s : "");
    for (char *p = r; *p; p++) *p = (char)tolower((unsigned char)*p);
    return r;
}

/* Python str.title() for ASCII: a cased letter after a non-letter starts a
 * word (upper), every other letter is lower.  "api_key" -> "Api Key". */
static char *ws1_title(const char *s)
{
    char *r = strdup(s ? s : "");
    int prev_alpha = 0;
    for (char *p = r; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalpha(c)) {
            *p = (char)(prev_alpha ? tolower(c) : toupper(c));
            prev_alpha = 1;
        } else {
            prev_alpha = 0;
        }
    }
    return r;
}

/* ---- UTF-8 aware length / tail (for _truncate_token) ---- */
static size_t ws1_utf8_len(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* last `count` characters, malloc'd */
static char *ws1_utf8_tail(const char *s, size_t count)
{
    size_t total = ws1_utf8_len(s);
    if (count >= total) return strdup(s);
    size_t skip = total - count;
    const char *p = s;
    while (skip > 0) {
        p++;
        while (((unsigned char)*p & 0xC0) == 0x80) p++;
        skip--;
    }
    return strdup(p);
}

/* ---- deep equality (Python == on JSON-modelled values) ---- */
static int ws1_eq(const json_t *a, const json_t *b)
{
    if (!a || !b) return a == b;
    if (a->type != b->type) {
        /* int/float cross-compare: 1 == 1.0 */
        if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
            return a->num_val == b->num_val;
        return 0;
    }
    switch (a->type) {
    case JSON_NULL:   return 1;
    case JSON_BOOL:   return a->bool_val == b->bool_val;
    case JSON_NUMBER: return a->num_val == b->num_val;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                    b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!ws1_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !ws1_eq(a->c.items[i], bv)) return 0;
        }
        return 1;
    }
    return 0;
}

/* ---- raise builders for Python builtin errors ---- */
static json_t *ws1_key_error(const char *key)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_putc(&b, '\'');
    ws1_sb_puts(&b, key);
    ws1_sb_putc(&b, '\'');
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("KeyError", m);
    free(m);
    return r;
}

static json_t *ws1_attr_error(const json_t *v, const char *attr)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "'%s' object has no attribute '%s'", ws1_type_name(v), attr);
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("AttributeError", m);
    free(m);
    return r;
}

static json_t *ws1_not_callable(const json_t *v)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "'%s' object is not callable", ws1_type_name(v));
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

static json_t *ws1_not_iterable(const json_t *v)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "'%s' object is not iterable", ws1_type_name(v));
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

static json_t *ws1_unhashable(const json_t *v)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "unhashable type: '%s'", ws1_type_name(v));
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

/* dict[k] with KeyError on miss (borrowed result). */
static json_t *ws1_item(json_t *obj, const char *key, json_t **err)
{
    json_t *v = ws1_is_dict(obj) ? json_obj_get(obj, key) : NULL;
    if (!v) *err = ws1_key_error(key);
    return v;
}

/* obj.get(key, default) — borrowed; default may be NULL (None). */
static json_t *ws1_get(const json_t *obj, const char *key, json_t *dflt)
{
    json_t *v = ws1_is_dict(obj) ? json_obj_get(obj, key) : NULL;
    return v ? v : dflt;
}

/* obj.get(key) -> NULL when absent (Python None). */
static json_t *ws1_get0(const json_t *obj, const char *key)
{
    return ws1_is_dict(obj) ? json_obj_get(obj, key) : NULL;
}

/* dict.update(src) in place; src must be a dict (fixture domain). */
static json_t *ws1_update(json_t *dst, const json_t *src)
{
    if (!ws1_is_dict(src)) return ws1_not_iterable(src);
    for (size_t i = 0; i < src->c.count; i++)
        json_set(dst, src->c.keys[i], json_copy(src->c.items[i]));
    return NULL;
}

/* ---- Python int() / float() on JSON values ----
 * Return NULL on success (value written to *out), else a raise spec.
 * bool is an int subclass: int(True) == 1, isinstance(True, int) is True.
 * Underscores between digits are accepted, whitespace is stripped. */

static int ws1_digits_underscore_ok(const char *s)
{
    /* s: optional sign already consumed; must be digits with single
     * underscores BETWEEN digits (no leading/trailing/double). */
    size_t n = strlen(s);
    if (n == 0) return 0;
    if (s[0] == '_' || s[n - 1] == '_') return 0;
    int prev_digit = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (!prev_digit) return 0;
            prev_digit = 0;
        } else if (isdigit((unsigned char)s[i])) {
            prev_digit = 1;
        } else {
            return 0;
        }
    }
    return prev_digit;
}

static json_t *ws1_pyint(const json_t *v, long long *out)
{
    if (!v || v->type == JSON_NULL)
        return ws1_raise("TypeError",
            "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val))
            return ws1_raise("ValueError", "cannot convert float infinity to integer");
        *out = (long long)trunc(v->num_val);
        return NULL;
    }
    if (v->type == JSON_STRING) {
        char *s = ws1_strip(v->str_val);
        const char *p = s;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        char *clean = malloc(strlen(p) + 1);
        size_t j = 0;
        for (const char *q = p; *q; q++) if (*q != '_') clean[j++] = *q;
        clean[j] = 0;
        int ok = ws1_digits_underscore_ok(p) && j > 0;
        long long val = 0;
        if (ok) {
            errno = 0;
            val = strtoll(clean, NULL, 10);
            if (errno == ERANGE) ok = 0;
        }
        free(clean);
        if (!ok) {
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b, "invalid literal for int() with base 10: '%s'", v->str_val);
            char *m = ws1_sb_take(&b);
            json_t *r = ws1_raise("ValueError", m);
            free(m); free(s);
            return r;
        }
        free(s);
        *out = neg ? -val : val;
        return NULL;
    }
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b,
        "int() argument must be a string, a bytes-like object or a real number, not '%s'",
        ws1_type_name(v));
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

static json_t *ws1_pyfloat(const json_t *v, double *out)
{
    if (!v || v->type == JSON_NULL)
        return ws1_raise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v->type == JSON_STRING) {
        char *s = ws1_strip(v->str_val);
        /* strip underscores between digits (Python float() accepts them) */
        char *clean = malloc(strlen(s) + 1);
        size_t j = 0;
        for (const char *q = s; *q; q++) if (*q != '_') clean[j++] = *q;
        clean[j] = 0;
        char *end = NULL;
        double d = strtod(clean, &end);
        int ok = (end != clean && *end == 0 && j > 0);
        /* reject inf/nan spellings: JSON cannot carry them (fixture domain) */
        if (ok && !isfinite(d)) ok = 0;
        free(clean);
        if (!ok) {
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b, "could not convert string to float: '%s'", v->str_val);
            char *m = ws1_sb_take(&b);
            json_t *r = ws1_raise("ValueError", m);
            free(m); free(s);
            return r;
        }
        free(s);
        *out = d;
        return NULL;
    }
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "float() argument must be a string or a real number, not '%s'",
                  ws1_type_name(v));
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

/* ---- POSIX path string helpers (PathDouble model) ---- */
static char *ws1_path_join(const char *a, const char *b)
{
    if (b[0] == '/') return strdup(b);
    size_t na = strlen(a);
    while (na > 1 && a[na - 1] == '/') na--;
    ws1_sb s; ws1_sb_init(&s);
    char *tmp = ws1_strndup(a, na);
    ws1_sb_puts(&s, tmp);
    free(tmp);
    if (na > 0 && a[na - 1] != '/') ws1_sb_putc(&s, '/');
    ws1_sb_puts(&s, b);
    return ws1_sb_take(&s);
}

static char *ws1_path_name(const char *p)
{
    const char *slash = strrchr(p, '/');
    return strdup(slash ? slash + 1 : p);
}

static char *ws1_path_parent(const char *p)
{
    const char *slash = strrchr(p, '/');
    if (!slash) return strdup(".");
    if (slash == p) return strdup("/");
    return ws1_strndup(p, (size_t)(slash - p));
}

/* str.rstrip("/") */
static char *ws1_rstrip_slash(const char *s)
{
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    return ws1_strndup(s, n);
}

/* Python str.removeprefix("custom:") */
static char *ws1_removeprefix_custom(const char *s)
{
    if (strncmp(s, "custom:", 7) == 0) return strdup(s + 7);
    return strdup(s);
}

/* ================================================================
 * Pinned constants (oracle cross-checks each against the live module)
 * ================================================================ */

/* tools.tts_command_provider.BUILTIN_TTS_PROVIDERS */
static const char *WS1_TTS_BUILTINS[] = {
    "edge", "elevenlabs", "openai", "minimax", "xai", "mistral", "gemini",
    "neutts", "kittentts", "piper", "deepinfra"
};
/* tools.transcription_common.BUILTIN_STT_PROVIDERS */
static const char *WS1_STT_BUILTINS[] = {
    "local", "local_command", "groq", "openai", "mistral", "xai", "elevenlabs", "deepinfra"
};

static int ws1_in_strset(const char *s, const char *const *set, size_t n)
{
    for (size_t i = 0; i < n; i++) if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

/* hermes_cli/web_server_gateway.py:_PORT_BINDING_PLATFORM_PORTS */
typedef struct { const char *name, *port_key; int default_port; } ws1_port_binding;
static const ws1_port_binding WS1_PORT_BINDINGS[] = {
    {"webhook", "port", 8644}, {"api_server", "port", 8642},
    {"msgraph_webhook", "port", 8646}, {"feishu", "webhook_port", 8765},
    {"wecom_callback", "port", 8645}, {"bluebubbles", "webhook_port", 8645},
    {"sms", "webhook_port", 8080}, {"whatsapp_cloud", "webhook_port", 8090},
    {"line", "port", 8646}, {"teams", "port", 3978}
};
static const char *WS1_DEAD_STATES[] = {"fatal", "disconnected", "stopped"};

/* hermes_cli/config_providers.py:_API_MODE_ALIASES */
typedef struct { const char *alias, *canonical; } ws1_mode_alias;
static const ws1_mode_alias WS1_API_MODE_ALIASES[] = {
    {"openai", "chat_completions"}, {"openai_chat", "chat_completions"},
    {"openai-chat", "chat_completions"}, {"chat-completions", "chat_completions"},
    {"chatcompletions", "chat_completions"}, {"responses", "codex_responses"},
    {"openai_responses", "codex_responses"}, {"openai-responses", "codex_responses"},
    {"anthropic", "anthropic_messages"}, {"anthropic-messages", "anthropic_messages"},
    {"messages", "anthropic_messages"}, {"bedrock", "bedrock_converse"},
    {"bedrock-converse", "bedrock_converse"}
};
static const char *WS1_DESKTOP_API_MODES[] = {
    "chat_completions", "codex_responses", "anthropic_messages"
};

/* hermes_cli/web_routers/analytics.py key tuples */
static const char *WS1_MODEL_CARD_KEYS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
    "estimated_cost", "actual_cost", "sessions", "api_calls", "tool_calls",
    "last_used_at", "avg_tokens_per_session"
};
static const char *WS1_AUX_SUMMED_KEYS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
    "estimated_cost", "api_calls"
};
static const char *WS1_USAGE_KEYS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "reasoning_tokens",
    "estimated_cost", "actual_cost", "api_calls", "tool_calls"
};

#define WS1_AUTO_ARCHIVE_CHECK_INTERVAL_S 300.0
#define WS1_GATEWAY_RESTART_COOLDOWN_SECONDS 10.0

#define WS1_TELEGRAM_UNAVAILABLE "Telegram setup service is unavailable. Try again shortly."
#define WS1_TELEGRAM_INVALID "Telegram setup service returned an invalid response."

/* ================================================================
 * 1. _normalize_memory_provider_schema (+ _schema_field_kind)
 * ================================================================ */

/* port of hermes_cli/web_server_memory.py:_schema_field_kind (helper) */
static char *ws1_schema_field_kind(const json_t *raw, const json_t *choices)
{
    json_t *k = ws1_get0(raw, "kind");
    json_t *t = ws1_get0(raw, "type");
    json_t *hint = ws1_jtruth(k) ? k : t;
    char *h = ws1_str_or_empty(hint);
    char *hs = ws1_strip(h); free(h);
    char *explicit_kind = ws1_lower(hs); free(hs);
    json_t *dflt = ws1_get0(raw, "default");
    const char *kind = "text";
    if (ws1_jtruth(ws1_get0(raw, "secret"))) kind = "secret";
    else if (ws1_jtruth(choices)) kind = "select";
    else if (strcmp(explicit_kind, "bool") == 0 || strcmp(explicit_kind, "boolean") == 0
             || (dflt && dflt->type == JSON_BOOL)) kind = "boolean";
    else if (strcmp(explicit_kind, "int") == 0 || strcmp(explicit_kind, "integer") == 0
             || (dflt && dflt->type == JSON_NUMBER && ws1_num_is_int(dflt))) kind = "integer";
    else if (strcmp(explicit_kind, "float") == 0 || strcmp(explicit_kind, "number") == 0
             || (dflt && dflt->type == JSON_NUMBER && !ws1_num_is_int(dflt))) kind = "number";
    char *r = strdup(kind);
    free(explicit_kind);
    return r;
}

/* provider double convention: dict with get_config_schema=="fn" carries the
 * method; the seam supplies its return (or raise). */
static int ws1_provider_has_schema_method(const json_t *provider)
{
    if (!ws1_is_dict(provider)) return 0;
    json_t *m = json_obj_get(provider, "get_config_schema");
    return m && m->type == JSON_STRING && strcmp(m->str_val, "fn") == 0;
}

static json_t *ws1_normalize_memory_provider_schema_impl(const char *name, json_t *provider)
{
    json_t *raw_schema = json_array();
    if (provider && ws1_provider_has_schema_method(provider)) {
        json_t *raw = ws1_call("provider_get_config_schema", ws1_args0());
        if (ws1_is_raise(raw)) {
            if (ws1_is_base_exc(raw)) { json_free(raw_schema); return raw; }
            json_free(raw);
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b, "Failed to read memory provider schema for %s", name);
            char *m = ws1_sb_take(&b);
            json_t *w = ws1_call("log_warning", ws1_args1(json_string(m)));
            free(m);
            json_free(w);
        } else if (ws1_is_list(raw)) {
            for (size_t i = 0; i < raw->c.count; i++)
                if (ws1_is_dict(raw->c.items[i]))
                    json_append(raw_schema, json_copy(raw->c.items[i]));
            json_free(raw);
        } else {
            json_free(raw);
        }
    }

    json_t *fields = json_array();
    for (size_t i = 0; i < raw_schema->c.count; i++) {
        json_t *raw = raw_schema->c.items[i];
        char *ks = ws1_str_or_empty(ws1_get0(raw, "key"));
        char *key = ws1_strip(ks); free(ks);
        if (!key[0]) { free(key); continue; }
        json_t *choices = ws1_get0(raw, "choices");
        if (!ws1_jtruth(choices)) choices = ws1_get0(raw, "options");
        if (!ws1_jtruth(choices)) choices = NULL;
        if (choices && !ws1_is_list(choices)) choices = NULL;

        json_t *field = json_object();
        json_set(field, "key", json_string(key));
        json_t *label = ws1_get0(raw, "label");
        if (ws1_jtruth(label)) {
            char *ls = ws1_pystr(label);
            json_set(field, "label", json_string(ls ? ls : ""));
            free(ls);
        } else {
            char *rep = malloc(strlen(key) + 1);
            for (size_t j = 0; key[j]; j++) rep[j] = key[j] == '_' ? ' ' : key[j];
            rep[strlen(key)] = 0;
            char *t = ws1_title(rep); free(rep);
            json_set(field, "label", json_string(t)); free(t);
        }
        char *kind = ws1_schema_field_kind(raw, choices);
        json_set(field, "kind", json_string(kind)); free(kind);
        char *ds = ws1_str_or_empty(ws1_get0(raw, "description"));
        json_set(field, "description", json_string(ds)); free(ds);
        char *ps = ws1_str_or_empty(ws1_get0(raw, "placeholder"));
        json_set(field, "placeholder", json_string(ps)); free(ps);
        json_t *req = ws1_get0(raw, "required");
        json_set(field, "required", json_bool(req ? ws1_jtruth(req) : false));
        json_t *dflt = ws1_get0(raw, "default");
        json_set(field, "default", dflt ? json_copy(dflt) : json_string(""));
        json_t *options = json_array();
        if (choices) {
            for (size_t j = 0; j < choices->c.count; j++) {
                char *cs = ws1_pystr(choices->c.items[j]);
                json_t *opt = json_object();
                json_set(opt, "value", json_string(cs ? cs : ""));
                json_set(opt, "label", json_string(cs ? cs : ""));
                json_set(opt, "description", json_string(""));
                free(cs);
                json_append(options, opt);
            }
        }
        json_set(field, "options", options);
        char *us = ws1_str_or_empty(ws1_get0(raw, "url"));
        json_set(field, "url", json_string(us)); free(us);
        json_t *when = ws1_get0(raw, "when");
        json_set(field, "when", ws1_is_dict(when) ? json_copy(when) : json_null());
        json_t *mn = ws1_get0(raw, "minimum");
        json_set(field, "minimum", mn ? json_copy(mn) : json_null());
        json_t *mx = ws1_get0(raw, "maximum");
        json_set(field, "maximum", mx ? json_copy(mx) : json_null());
        json_t *st = ws1_get0(raw, "step");
        json_set(field, "step", st ? json_copy(st) : json_null());
        char *es = ws1_str_or_empty(ws1_get0(raw, "env_var"));
        json_set(field, "_env_key", es[0] ? json_string(es) : json_null());
        free(es);
        free(key);
        json_append(fields, field);
    }
    json_free(raw_schema);
    return fields;
}

/* PoP: ws_u_normalize_memory_provider_schema @ hermes_cli/web_server_memory.py:_normalize_memory_provider_schema */
json_t *ws_u_normalize_memory_provider_schema(json_t *req)
{
    const char *name = json_get_str(req, "name", "");
    return ws1_normalize_memory_provider_schema_impl(name, ws1_get0(req, "provider"));
}

/* ================================================================
 * 2. _read_memory_provider_existing_values
 * ================================================================ */

/* PoP: ws_u_read_memory_provider_existing_values @ hermes_cli/web_server_memory.py:_read_memory_provider_existing_values */
json_t *ws_u_read_memory_provider_existing_values(json_t *req)
{
    const char *name = json_get_str(req, "name", "");
    json_t *values = json_object();
    json_t *out = NULL;

    json_t *home = ws1_call("get_hermes_home", ws1_args0());
    if (ws1_is_raise(home)) { out = home; goto done; }
    if (!home || home->type == JSON_NULL) {
        json_free(home);
        out = ws1_raise("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
        goto done;
    }
    char *home_s = ws1_pystr(home);
    json_free(home);
    if (!home_s) home_s = strdup("");

    char *fname1 = malloc(strlen(name) + 8);
    sprintf(fname1, "%s.json", name);
    char *fname2 = malloc(strlen(name) + 16);
    sprintf(fname2, "%s/config.json", name);
    char *p1 = ws1_path_join(home_s, fname1);
    char *p2 = ws1_path_join(home_s, fname2);
    free(fname1); free(fname2);
    const char *paths[2] = {p1, p2};
    for (int i = 0; i < 2; i++) {
        json_t *data = ws1_call("read_json_file", ws1_args1(json_string(paths[i])));
        if (ws1_is_raise(data)) { out = data; goto done_paths; }
        json_t *err = ws1_update(values, data);
        json_free(data);
        if (err) { out = err; goto done_paths; }
    }
done_paths:
    free(p1); free(p2); free(home_s);
    if (out) goto done;

    json_t *cfg = ws1_call("load_config", ws1_args0());
    if (ws1_is_raise(cfg)) {
        if (ws1_is_base_exc(cfg)) { out = cfg; goto done; }
        json_free(cfg);
        cfg = json_object();
    }
    if (!ws1_is_dict(cfg)) { json_free(cfg); cfg = json_object(); }

    json_t *memory_cfg = ws1_get0(cfg, "memory");
    if (ws1_is_dict(memory_cfg)) {
        json_t *provider_cfg = ws1_get0(memory_cfg, name);
        if (ws1_is_dict(provider_cfg)) {
            json_t *err = ws1_update(values, provider_cfg);
            if (err) { out = err; json_free(cfg); goto done; }
        }
        json_t *legacy_cfg = ws1_get0(memory_cfg, "provider_config");
        if (ws1_is_dict(legacy_cfg)) {
            json_t *merged = json_copy(legacy_cfg);
            json_t *err = ws1_update(merged, values);
            if (err) { out = err; json_free(merged); json_free(cfg); goto done; }
            json_free(values);
            values = merged;
        }
    }
    json_t *plugins_cfg = ws1_get0(cfg, "plugins");
    if (strcmp(name, "holographic") == 0 && ws1_is_dict(plugins_cfg)) {
        json_t *holo = ws1_get0(plugins_cfg, "hermes-memory-store");
        if (ws1_is_dict(holo)) {
            json_t *err = ws1_update(values, holo);
            if (err) { out = err; json_free(cfg); goto done; }
        }
    }
    json_free(cfg);
    out = values;
    values = NULL;
done:
    json_free(values);
    return out;
}

/* ================================================================
 * 3. _discover_memory_provider_statuses
 *    (+ _normalize_memory_provider_name, _memory_provider_status)
 * ================================================================ */

static json_t *ws1_normalize_memory_provider_name(json_t *name_val)
{
    char *s0 = ws1_str_or_empty(name_val);
    char *provider = ws1_strip(s0); free(s0);
    json_t *core = ws1_call("is_core_memory_provider", ws1_args1(json_string(provider)));
    json_t *result;
    if (ws1_is_raise(core)) {
        result = core;
    } else {
        result = json_string(ws1_jtruth(core) ? "" : provider);
        json_free(core);
    }
    free(provider);
    return result;
}

static json_t *ws1_memory_provider_status(const json_t *row, const json_t *setup,
                                          int configured, const json_t *schema_fields)
{
    if (ws1_jtruth(ws1_get0(row, "missing"))) return json_string("missing");
    json_t *name = ws1_get0(row, "name");
    json_t *parked = ws1_call("parked_provider_notice",
                              ws1_args1(name ? json_copy(name) : json_null()));
    if (ws1_is_raise(parked)) return parked;
    int is_parked = ws1_jtruth(parked);
    json_free(parked);
    if (is_parked) return json_string("unavailable");
    json_t *deps = ws1_get0(setup, "dependencies_installed");
    int deps_ok = deps ? ws1_jtruth(deps) : 1;
    if (!deps_ok) return json_string("unavailable");
    int available = ws1_jtruth(ws1_get0(row, "available"));
    if (!configured || (!available && ws1_jtruth(schema_fields)))
        return json_string("needs_config");
    return json_string(available ? "ready" : "unavailable");
}

/* PoP: ws_u_discover_memory_provider_statuses @ hermes_cli/web_server_memory.py:_discover_memory_provider_statuses */
json_t *ws_u_discover_memory_provider_statuses(json_t *req)
{
    (void)req;
    json_t *discovered = json_object();
    json_t *out = NULL;

    json_t *rows = ws1_call("discover_memory_providers", ws1_args0());
    if (ws1_is_raise(rows)) {
        if (ws1_is_base_exc(rows)) { out = rows; goto done; }
        json_free(rows);
        json_t *w = ws1_call("log_exception",
                             ws1_args1(json_string("discover_memory_providers failed")));
        if (ws1_is_raise(w)) { out = w; goto done; }
        json_free(w);
    } else {
        /* for name, description, available in discover_memory_providers() */
        json_t *err = NULL;
        if (!ws1_is_list(rows)) {
            err = ws1_not_iterable(rows);
        } else {
            for (size_t i = 0; i < rows->c.count && !err; i++) {
                json_t *triple = rows->c.items[i];
                if (!ws1_is_list(triple)) {
                    ws1_sb b; ws1_sb_init(&b);
                    ws1_sb_printf(&b, "cannot unpack non-iterable %s object", ws1_type_name(triple));
                    char *m = ws1_sb_take(&b);
                    err = ws1_raise("TypeError", m);
                    free(m);
                    break;
                }
                if (triple->c.count != 3) {
                    ws1_sb b; ws1_sb_init(&b);
                    if (triple->c.count > 3)
                        ws1_sb_printf(&b, "too many values to unpack (expected 3)");
                    else
                        ws1_sb_printf(&b, "not enough values to unpack (expected 3, got %zu)",
                                      triple->c.count);
                    char *m = ws1_sb_take(&b);
                    err = ws1_raise("ValueError", m);
                    free(m);
                    break;
                }
                char *ns = ws1_pystr(triple->c.items[0]);
                json_t *row = json_object();
                json_set(row, "name", json_string(ns ? ns : ""));
                char *ds = ws1_str_or_empty(triple->c.items[1]);
                json_set(row, "description", json_string(ds)); free(ds);
                json_set(row, "available", json_bool(ws1_jtruth(triple->c.items[2])));
                json_set(row, "missing", json_bool(false));
                json_set(discovered, ns ? ns : "", row);
                free(ns);
            }
        }
        json_free(rows);
        if (err) {
            if (ws1_is_base_exc(err)) { out = err; goto done; }
            json_free(err);
            json_t *w = ws1_call("log_exception",
                                 ws1_args1(json_string("discover_memory_providers failed")));
            if (ws1_is_raise(w)) { out = w; goto done; }
            json_free(w);
        }
    }

    json_t *cfg = ws1_call("load_config", ws1_args0());
    if (ws1_is_raise(cfg)) { out = cfg; goto done; }
    json_t *mem = NULL;
    if (!ws1_is_dict(cfg)) {
        out = ws1_attr_error(cfg, "get");
        json_free(cfg);
        goto done;
    }
    mem = ws1_get0(cfg, "memory");
    char *active = strdup("");
    if (ws1_is_dict(mem)) {
        json_t *n = ws1_normalize_memory_provider_name(ws1_get0(mem, "provider"));
        if (ws1_is_raise(n)) { out = n; json_free(cfg); goto done; }
        free(active);
        active = strdup(n->str_val);
        json_free(n);
    }
    json_free(cfg);
    if (active[0] && !json_obj_get(discovered, active)) {
        json_t *row = json_object();
        json_set(row, "name", json_string(active));
        json_set(row, "description", json_string("Configured provider was not found."));
        json_set(row, "available", json_bool(false));
        json_set(row, "missing", json_bool(true));
        json_set(discovered, active, row);
    }
    free(active);

    /* sorted(discovered) — ASCII name order */
    size_t n = discovered->c.count;
    const char **names = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) names[i] = discovered->c.keys[i];
    for (size_t i = 1; i < n; i++) {
        const char *key = names[i];
        size_t j = i;
        while (j > 0 && strcmp(names[j - 1], key) > 0) { names[j] = names[j - 1]; j--; }
        names[j] = key;
    }

    json_t *providers = json_array();
    for (size_t i = 0; i < n && !out; i++) {
        json_t *row = json_obj_get(discovered, names[i]);
        int missing = ws1_jtruth(ws1_get0(row, "missing"));
        json_t *provider = NULL;
        if (!missing) {
            provider = ws1_call("load_memory_provider", ws1_args1(json_string(names[i])));
            if (ws1_is_raise(provider)) { out = provider; break; }
        }
        json_t *setup = ws1_call("memory_provider_setup_info", ws1_args1(json_string(names[i])));
        if (ws1_is_raise(setup)) { out = setup; json_free(provider); break; }
        int configured = 0;
        if (!missing) {
            json_t *c = ws1_call("memory_provider_is_configured",
                                 ws1_args2(json_string(names[i]),
                                           provider ? json_copy(provider) : json_null()));
            if (ws1_is_raise(c)) { out = c; json_free(provider); json_free(setup); break; }
            configured = ws1_jtruth(c);
            json_free(c);
        }
        json_t *schema_fields = json_array();
        if (!missing) {
            json_free(schema_fields);
            schema_fields = ws1_normalize_memory_provider_schema_impl(names[i], provider);
            if (ws1_is_raise(schema_fields)) {
                out = schema_fields; json_free(provider); json_free(setup); break;
            }
        }
        json_t *status = ws1_memory_provider_status(row, setup, configured, schema_fields);
        if (ws1_is_raise(status)) {
            out = status; json_free(provider); json_free(setup); json_free(schema_fields); break;
        }
        json_t *p = json_object();
        json_set(p, "name", json_string(names[i]));
        json_set(p, "description", json_copy(ws1_get0(row, "description")));
        json_set(p, "available", json_copy(ws1_get0(row, "available")));
        json_set(p, "configured", json_bool(configured));
        json_set(p, "status", status);
        json_set(p, "setup", setup);
        json_append(providers, p);
        json_free(provider);
        json_free(schema_fields);
    }
    free(names);
    if (out) { json_free(providers); goto done; }
    out = providers;
done:
    json_free(discovered);
    return out;
}

/* ================================================================
 * 4. _profile_platform_ports
 * ================================================================ */

/* PoP: ws_u_profile_platform_ports @ hermes_cli/web_server_gateway.py:_profile_platform_ports */
json_t *ws_u_profile_platform_ports(json_t *req)
{
    json_t *runtime = ws1_get0(req, "runtime");
    json_t *platforms_holder = ws1_jtruth(runtime) ? runtime : NULL;
    json_t *platforms = NULL;
    if (platforms_holder) {
        if (!ws1_is_dict(platforms_holder))
            return ws1_attr_error(platforms_holder, "get");
        platforms = ws1_get0(platforms_holder, "platforms");
        if (!ws1_jtruth(platforms)) platforms = NULL;
    }
    if (platforms && !ws1_is_dict(platforms))
        return ws1_attr_error(platforms, "items");

    /* active = [name for name, state in platforms.items() if ...] */
    json_t *active = json_array();
    if (platforms) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            const char *name = platforms->c.keys[i];
            json_t *state = platforms->c.items[i];
            const ws1_port_binding *b = NULL;
            for (size_t j = 0; j < sizeof WS1_PORT_BINDINGS / sizeof WS1_PORT_BINDINGS[0]; j++)
                if (strcmp(WS1_PORT_BINDINGS[j].name, name) == 0) { b = &WS1_PORT_BINDINGS[j]; break; }
            if (!b || !ws1_is_dict(state)) continue;
            json_t *st = ws1_get0(state, "state");
            if (st && (ws1_is_list(st) || ws1_is_dict(st))) {
                json_free(active);
                return ws1_unhashable(st);
            }
            int dead = 0;
            if (ws1_is_str(st))
                dead = ws1_in_strset(st->str_val, WS1_DEAD_STATES, 3);
            if (!dead) json_append(active, json_string(name));
        }
    }
    if (active->c.count == 0) { json_free(active); return json_object(); }

    /* blocks from the probed profile's raw config.yaml */
    json_t *blocks = json_object();
    const char *home = json_get_str(req, "profile_home", "");
    char *cfg_path = ws1_path_join(home, "config.yaml");
    json_t *cfg = ws1_call("read_user_config_raw", ws1_args1(json_string(cfg_path)));
    free(cfg_path);
    if (ws1_is_raise(cfg)) {
        if (ws1_is_base_exc(cfg)) { json_free(active); json_free(blocks); return cfg; }
        json_free(cfg);
        cfg = NULL;
    }
    if (cfg) {
        json_t *err = NULL;
        if (!ws1_is_dict(cfg)) {
            err = ws1_attr_error(cfg, "get");
        } else {
            json_t *gw = ws1_get0(cfg, "gateway");
            json_t *gateway_cfg = ws1_is_dict(gw) ? gw : NULL;
            json_t *srcs[2] = {
                gateway_cfg ? ws1_get0(gateway_cfg, "platforms") : NULL,
                ws1_get0(cfg, "platforms")
            };
            for (int s = 0; s < 2 && !err; s++) {
                json_t *src = srcs[s];
                if (!ws1_is_dict(src)) continue;
                for (size_t i = 0; i < src->c.count; i++) {
                    json_t *plat_block = src->c.items[i];
                    if (!ws1_is_dict(plat_block)) continue;
                    json_t *dst = json_obj_get(blocks, src->c.keys[i]);
                    if (!dst) {
                        dst = json_object();
                        json_set(blocks, src->c.keys[i], dst);
                    }
                    err = ws1_update(dst, plat_block);
                    if (err) break;
                }
            }
        }
        json_free(cfg);
        if (err) {
            if (ws1_is_base_exc(err)) { json_free(active); json_free(blocks); return err; }
            json_free(err);
            /* except Exception: blocks = {} */
            json_free(blocks);
            blocks = json_object();
        }
    }

    json_t *ports = json_object();
    for (size_t i = 0; i < active->c.count; i++) {
        const char *name = active->c.items[i]->str_val;
        const ws1_port_binding *b = NULL;
        for (size_t j = 0; j < sizeof WS1_PORT_BINDINGS / sizeof WS1_PORT_BINDINGS[0]; j++)
            if (strcmp(WS1_PORT_BINDINGS[j].name, name) == 0) { b = &WS1_PORT_BINDINGS[j]; break; }
        json_t *block = ws1_get0(blocks, name);
        if (!ws1_jtruth(block)) block = NULL;
        json_t *extra = block ? ws1_get0(block, "extra") : NULL;
        if (!ws1_is_dict(extra)) extra = NULL;
        /* block.get(port_key, extra.get(port_key, default_port)): a PRESENT
         * null wins over the fallback (dict.get semantics), then int(None)
         * fails back to the default. */
        json_t *raw = NULL;
        int present = 0;
        if (block && json_obj_get(block, b->port_key)) {
            raw = json_obj_get(block, b->port_key);
            present = 1;
        } else if (extra && json_obj_get(extra, b->port_key)) {
            raw = json_obj_get(extra, b->port_key);
            present = 1;
        }
        long long port = b->default_port;
        if (present) {
            long long v = 0;
            json_t *err = ws1_pyint(raw, &v);
            if (err) json_free(err);   /* except (TypeError, ValueError): default */
            else port = v;
        }
        json_set(ports, name, json_number((double)port));
    }
    json_free(active);
    json_free(blocks);
    return ports;
}

/* ================================================================
 * 5. _custom_provider_options (+ _is_command_provider_block, cfg_get)
 * ================================================================ */

static int ws1_is_command_provider_block(const json_t *value)
{
    if (!ws1_is_dict(value)) return 0;
    char *t0 = ws1_str_or_empty(ws1_get0(value, "type"));
    char *ts = ws1_strip(t0); free(t0);
    char *ptype = ws1_lower(ts); free(ts);
    int ok = (!ptype[0] || strcmp(ptype, "command") == 0);
    free(ptype);
    if (!ok) return 0;
    json_t *command = ws1_get0(value, "command");
    if (!ws1_is_str(command)) return 0;
    char *cs = ws1_strip(command->str_val);
    int nonempty = cs[0] != 0;
    free(cs);
    return nonempty;
}

/* hermes_cli/config.py:cfg_get — nested key traversal, dict.get semantics */
static json_t *ws1_cfg_get(const json_t *cfg, const char *k1, const char *k2)
{
    if (!ws1_is_dict(cfg)) return NULL;
    const json_t *node = cfg;
    const char *keys[2] = {k1, k2};
    for (int i = 0; i < 2; i++) {
        if (!ws1_is_dict(node)) return NULL;
        json_t *v = json_obj_get(node, keys[i]);
        if (!v) return NULL;
        node = v;
    }
    return (json_t *)node;
}

static json_t *ws1_custom_provider_options_impl(const char *kind,
                                                const json_t *builtin_names,
                                                const json_t *cfg)
{
    json_t *names = json_array();
    json_t *seen = json_array();   /* lowercased stripped names, set semantics */
    if (!ws1_is_list(builtin_names)) return ws1_not_iterable(builtin_names);
    for (size_t i = 0; i < builtin_names->c.count; i++) {
        char *s = ws1_pystr(builtin_names->c.items[i]);
        if (!s) s = strdup("");
        json_append(names, json_string(s));
        char *st = ws1_strip(s);
        char *lo = ws1_lower(st); free(st);
        json_append(seen, json_string(lo));
        free(lo); free(s);
    }

    const char *const *runtime_builtins =
        strcmp(kind, "tts") == 0 ? WS1_TTS_BUILTINS : WS1_STT_BUILTINS;
    size_t n_builtin = strcmp(kind, "tts") == 0
        ? sizeof WS1_TTS_BUILTINS / sizeof WS1_TTS_BUILTINS[0]
        : sizeof WS1_STT_BUILTINS / sizeof WS1_STT_BUILTINS[0];

    /* _add(name): strip (str only), dedupe case-insensitively */
    #define WS1_ADD_NAME(nval) do { \
        char *stripped = ws1_is_str(nval) ? ws1_strip((nval)->str_val) : strdup(""); \
        if (stripped[0]) { \
            char *lo = ws1_lower(stripped); \
            int found = 0; \
            for (size_t si = 0; si < seen->c.count; si++) \
                if (strcmp(seen->c.items[si]->str_val, lo) == 0) { found = 1; break; } \
            if (!found) { \
                json_append(names, json_string(stripped)); \
                json_append(seen, json_string(lo)); \
            } \
            free(lo); \
        } \
        free(stripped); \
    } while (0)

    if (!ws1_is_dict(cfg)) { json_free(names); json_free(seen); return ws1_attr_error(cfg, "get"); }
    json_t *section = ws1_get0(cfg, kind);
    if (!ws1_is_dict(section)) section = NULL;
    json_t *providers_map = section ? ws1_get0(section, "providers") : NULL;
    /* candidate_blocks: [providers_map if dict] + {k:v for section if k != "providers"} */
    json_t *legacy_block = json_object();
    if (section) {
        for (size_t i = 0; i < section->c.count; i++)
            if (strcmp(section->c.keys[i], "providers") != 0)
                json_set(legacy_block, section->c.keys[i], json_copy(section->c.items[i]));
    }
    json_t *blocks[2] = { ws1_is_dict(providers_map) ? providers_map : NULL, legacy_block };
    for (int bi = 0; bi < 2; bi++) {
        json_t *block = blocks[bi];
        if (!block) continue;
        for (size_t i = 0; i < block->c.count; i++) {
            const char *name = block->c.keys[i];
            char *st = ws1_strip(name);
            char *lo = ws1_lower(st); free(st);
            int excluded = ws1_in_strset(lo, runtime_builtins, n_builtin);
            free(lo);
            if (!excluded && ws1_is_command_provider_block(block->c.items[i])) {
                json_t *nv = json_string(name);
                WS1_ADD_NAME(nv);
                json_free(nv);
            }
        }
    }
    json_free(legacy_block);

    /* plugin registries — opportunistic; any exception is swallowed */
    json_t *plist = ws1_call("list_voice_providers", ws1_args0());
    if (ws1_is_raise(plist)) {
        if (ws1_is_base_exc(plist)) { json_free(names); json_free(seen); return plist; }
        json_free(plist);
    } else if (!ws1_is_list(plist)) {
        json_free(plist);   /* TypeError iterating -> except Exception: pass */
    } else {
        for (size_t i = 0; i < plist->c.count; i++) {
            json_t *p = plist->c.items[i];
            json_t *nm = ws1_is_dict(p) ? ws1_get0(p, "name") : NULL;
            WS1_ADD_NAME(nm);
        }
        json_free(plist);
    }

    json_t *active = ws1_cfg_get(cfg, kind, "provider");
    WS1_ADD_NAME(active);
    #undef WS1_ADD_NAME
    json_free(seen);
    return names;
}

/* PoP: ws_u_custom_provider_options @ hermes_cli/web_server_config.py:_custom_provider_options */
json_t *ws_u_custom_provider_options(json_t *req)
{
    const char *kind = json_get_str(req, "kind", "");
    return ws1_custom_provider_options_impl(kind, ws1_get0(req, "builtin_names"),
                                            ws1_get0(req, "cfg"));
}

/* ================================================================
 * 6. _schema_with_dynamic_provider_options (+ _schema_select_options)
 * ================================================================ */

static json_t *ws1_schema_select_options(const json_t *schema, const char *key)
{
    json_t *entry = ws1_get0(schema, key);
    json_t *options = ws1_is_dict(entry) ? ws1_get0(entry, "options") : NULL;
    return ws1_is_list(options) ? options : NULL;
}

/* PoP: ws_u_schema_with_dynamic_provider_options @ hermes_cli/web_server_config.py:_schema_with_dynamic_provider_options */
json_t *ws_u_schema_with_dynamic_provider_options(json_t *req)
{
    json_t *schema = ws1_get0(req, "schema");
    if (!ws1_is_dict(schema)) schema = NULL;
    json_t *overlay = json_object();
    json_t *out = NULL;

    json_t *cfg = ws1_call("load_config", ws1_args0());
    if (ws1_is_raise(cfg)) {
        if (ws1_is_base_exc(cfg)) { out = cfg; goto done; }
        json_free(cfg);
        out = schema ? json_copy(schema) : json_object();
        goto done;
    }

    /* merge(key, options): overlay only when the entry is a select and the
     * option list actually changed */
    #define WS1_MERGE(key, opts) do { \
        json_t *existing = ws1_schema_select_options(schema, key); \
        if (existing && !ws1_eq(opts, existing)) { \
            json_t *entry = json_copy(json_obj_get(schema, key)); \
            json_set(entry, "options", json_copy(opts)); \
            json_set(overlay, key, entry); \
        } \
    } while (0)

    const char *kinds[2] = {"tts", "stt"};
    for (int ki = 0; ki < 2 && !out; ki++) {
        char key[32];
        snprintf(key, sizeof key, "%s.provider", kinds[ki]);
        json_t *existing = ws1_schema_select_options(schema, key);
        if (existing) {
            json_t *opts = ws1_custom_provider_options_impl(kinds[ki], existing, cfg);
            if (ws1_is_raise(opts)) { out = opts; break; }
            WS1_MERGE(key, opts);
            json_free(opts);
        }
    }
    if (out) { json_free(cfg); goto done; }

    json_t *mem_opts = ws1_call("memory_provider_schema_options", ws1_args1(json_copy(cfg)));
    if (ws1_is_raise(mem_opts)) { out = mem_opts; json_free(cfg); goto done; }
    WS1_MERGE("memory.provider", mem_opts);
    json_free(mem_opts);

    json_t *tb_options = ws1_schema_select_options(schema, "terminal.backend");
    if (tb_options) {
        json_t *rows = ws1_call("plugin_terminal_backend_rows", ws1_args0());
        json_t *plugin_names = json_array();
        int failed = 0;
        if (ws1_is_raise(rows)) {
            if (ws1_is_base_exc(rows)) { out = rows; json_free(cfg); goto done; }
            json_free(rows);
            failed = 1;
        } else if (!ws1_is_list(rows)) {
            json_free(rows);
            failed = 1;
        } else {
            /* sorted({row["name"] for row in rows} - set(tb_options)) */
            json_t *set = json_array();
            for (size_t i = 0; i < rows->c.count && !failed; i++) {
                json_t *row = rows->c.items[i];
                if (!ws1_is_dict(row)) { failed = 1; break; }
                json_t *nm = json_obj_get(row, "name");
                if (!nm) { failed = 1; break; }
                if (ws1_is_list(nm) || ws1_is_dict(nm)) { failed = 1; break; }
                int in_tb = 0;
                for (size_t j = 0; j < tb_options->c.count; j++)
                    if (ws1_eq(nm, tb_options->c.items[j])) { in_tb = 1; break; }
                if (in_tb) continue;
                int dup = 0;
                for (size_t j = 0; j < set->c.count; j++)
                    if (ws1_eq(nm, set->c.items[j])) { dup = 1; break; }
                if (!dup) json_append(set, json_copy(nm));
            }
            json_free(rows);
            if (!failed) {
                /* sorted(): homogeneous comparable types only (str / number /
                 * null); a mix raises TypeError -> plugin_names = [] */
                int kind0 = -1;   /* 0 str, 1 number/bool, 2 null */
                for (size_t i = 0; i < set->c.count && !failed; i++) {
                    json_t *v = set->c.items[i];
                    int k = ws1_is_str(v) ? 0 : (v->type == JSON_NULL ? 2 : 1);
                    if (kind0 == -1) kind0 = k;
                    else if (k != kind0) failed = 1;
                }
                if (!failed) {
                    for (size_t i = 1; i < set->c.count; i++) {
                        json_t *key = set->c.items[i];
                        size_t j = i;
                        while (j > 0) {
                            json_t *prev = set->c.items[j - 1];
                            int cmp = kind0 == 0
                                ? strcmp(prev->str_val, key->str_val)
                                : (prev->num_val > key->num_val ? 1
                                   : prev->num_val < key->num_val ? -1 : 0);
                            if (cmp <= 0) break;
                            set->c.items[j] = prev; j--;
                        }
                        set->c.items[j] = key;
                    }
                }
                if (!failed) {
                    json_free(plugin_names);
                    plugin_names = set;
                    set = NULL;
                }
            }
            json_free(set);
        }
        if (!failed && plugin_names->c.count > 0) {
            json_t *merged = json_copy(tb_options);
            for (size_t i = 0; i < plugin_names->c.count; i++)
                json_append(merged, json_copy(plugin_names->c.items[i]));
            WS1_MERGE("terminal.backend", merged);
            json_free(merged);
        }
        json_free(plugin_names);
    }
    #undef WS1_MERGE
    json_free(cfg);

    if (overlay->c.count > 0) {
        out = json_copy(schema);
        for (size_t i = 0; i < overlay->c.count; i++)
            json_set(out, overlay->c.keys[i], json_copy(overlay->c.items[i]));
    } else {
        out = schema ? json_copy(schema) : json_object();
    }
done:
    json_free(overlay);
    return out;
}

/* ================================================================
 * 7. normalize_mcp_server_create (+ _strip_bearer_prefix)
 * ================================================================ */

static char *ws1_strip_bearer_prefix(const char *token)
{
    char *stripped = ws1_strip(token);
    size_t n = strlen(stripped);
    if (n >= 7) {
        char head[8];
        memcpy(head, stripped, 7);
        head[7] = 0;
        for (char *p = head; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strcmp(head, "bearer ") == 0) {
            char *rest = ws1_strip(stripped + 7);
            free(stripped);
            return rest;
        }
    }
    return stripped;
}

/* PoP: ws_u_normalize_mcp_server_create @ hermes_cli/web_server_mcp.py:normalize_mcp_server_create */
json_t *ws_u_normalize_mcp_server_create(json_t *req)
{
    json_t *body = ws1_get0(req, "body");
    json_t *out = NULL;
    json_t *server_config = NULL;
    char *name = NULL, *url = NULL, *command = NULL, *auth = NULL;
    char *bearer_token = NULL;
    int has_bearer = 0;

    /* attribute access order: name, url, command, auth, bearer_token */
    const char *attrs[5] = {"name", "url", "command", "auth", "bearer_token"};
    json_t *vals[5];
    for (int i = 0; i < 5; i++) {
        vals[i] = ws1_is_dict(body) ? json_obj_get(body, attrs[i]) : NULL;
        if (!vals[i]) { out = ws1_attr_error(body, attrs[i]); goto done; }
    }
    json_t *args_val = ws1_is_dict(body) ? json_obj_get(body, "args") : NULL;
    json_t *env_val = ws1_is_dict(body) ? json_obj_get(body, "env") : NULL;

    char *n0 = ws1_str_or_empty(vals[0]);
    name = ws1_strip(n0); free(n0);
    if (!name[0]) { out = ws1_raise("ValueError", "Server name is required"); goto done; }
    char *u0 = ws1_str_or_empty(vals[1]);
    url = ws1_strip(u0); free(u0);
    char *c0 = ws1_str_or_empty(vals[2]);
    command = ws1_strip(c0); free(c0);
    char *a0 = ws1_str_or_empty(vals[3]);
    if (!a0[0]) { free(a0); a0 = strdup("none"); }
    char *as = ws1_strip(a0); free(a0);
    auth = ws1_lower(as); free(as);
    if (vals[4]->type != JSON_NULL) {
        has_bearer = 1;
        bearer_token = ws1_pystr(vals[4]);
        if (!bearer_token) bearer_token = strdup("");
    }

    if ((url[0] != 0) == (command[0] != 0)) {
        out = ws1_raise("ValueError",
                        "Provide exactly one of URL (HTTP/SSE) or command (stdio)");
        goto done;
    }
    if (strcmp(auth, "none") != 0 && strcmp(auth, "header") != 0 && strcmp(auth, "oauth") != 0) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "Unsupported auth mode: %s", auth);
        char *m = ws1_sb_take(&b);
        out = ws1_raise("ValueError", m);
        free(m);
        goto done;
    }

    server_config = json_object();
    if (url[0]) {
        if (ws1_jtruth(args_val)) {
            out = ws1_raise("ValueError", "Arguments are only supported for stdio MCP servers");
            goto done;
        }
        if (ws1_jtruth(env_val)) {
            out = ws1_raise("ValueError", "Environment variables are only supported for stdio MCP servers");
            goto done;
        }
        if (strcmp(auth, "header") == 0) {
            char *normalized = bearer_token ? ws1_strip_bearer_prefix(bearer_token) : strdup("");
            char *lo = ws1_lower(normalized);
            int bad = !normalized[0] || strcmp(lo, "bearer") == 0;
            free(lo); free(normalized);
            if (bad) { out = ws1_raise("ValueError", "Bearer token is required"); goto done; }
            json_t *headers = ws1_call("bearer_auth_headers", ws1_args1(json_string(name)));
            if (ws1_is_raise(headers)) { out = headers; goto done; }
            json_set(server_config, "headers", headers);
        } else if (has_bearer) {
            out = ws1_raise("ValueError", "Bearer token requires header authentication");
            goto done;
        }
        json_set(server_config, "url", json_string(url));
        if (strcmp(auth, "oauth") == 0)
            json_set(server_config, "auth", json_string("oauth"));
    } else {
        if (strcmp(auth, "none") != 0 || has_bearer) {
            out = ws1_raise("ValueError",
                            "HTTP authentication is not supported for stdio MCP servers");
            goto done;
        }
        json_set(server_config, "command", json_string(command));
        if (ws1_jtruth(args_val)) {
            if (!ws1_is_list(args_val)) { out = ws1_not_iterable(args_val); goto done; }
            json_set(server_config, "args", json_copy(args_val));
        }
        if (ws1_jtruth(env_val)) {
            if (!ws1_is_dict(env_val)) { out = ws1_not_iterable(env_val); goto done; }
            json_set(server_config, "env", json_copy(env_val));
        }
    }

    json_t *issues = ws1_call("validate_mcp_server_entry",
                              ws1_args2(json_string(name), json_copy(server_config)));
    if (ws1_is_raise(issues)) { out = issues; goto done; }
    if (ws1_jtruth(issues)) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "Server '%s' rejected: ", name);
        if (!ws1_is_list(issues)) {
            json_free(issues);
            out = ws1_not_iterable(NULL);
            char *m = ws1_sb_take(&b); free(m);
            goto done;
        }
        for (size_t i = 0; i < issues->c.count; i++) {
            if (!ws1_is_str(issues->c.items[i])) {
                ws1_sb bb; ws1_sb_init(&bb);
                ws1_sb_printf(&bb, "sequence item %zu: expected str instance, %s found",
                              i, ws1_type_name(issues->c.items[i]));
                char *m = ws1_sb_take(&bb);
                out = ws1_raise("TypeError", m);
                free(m);
                json_free(issues);
                char *m2 = ws1_sb_take(&b); free(m2);
                goto done;
            }
            if (i > 0) ws1_sb_puts(&b, "; ");
            ws1_sb_puts(&b, issues->c.items[i]->str_val);
        }
        json_free(issues);
        char *m = ws1_sb_take(&b);
        out = ws1_raise("ValueError", m);
        free(m);
        goto done;
    }
    json_free(issues);

    out = json_array();
    json_append(out, json_string(name));
    json_append(out, server_config);
    server_config = NULL;
    json_append(out, bearer_token ? json_string(bearer_token) : json_null());
done:
    free(name); free(url); free(command); free(auth); free(bearer_token);
    json_free(server_config);
    return out;
}

/* ================================================================
 * 8. _denormalize_config_from_web
 * ================================================================ */

/* unpack a scripted 2-tuple; returns NULL ok, else raise */
static json_t *ws1_unpack2(json_t *v, json_t **a, json_t **b)
{
    if (ws1_is_list(v)) {
        if (v->c.count == 2) { *a = v->c.items[0]; *b = v->c.items[1]; return NULL; }
        ws1_sb sb; ws1_sb_init(&sb);
        if (v->c.count > 2) ws1_sb_puts(&sb, "too many values to unpack (expected 2)");
        else ws1_sb_printf(&sb, "not enough values to unpack (expected 2, got %zu)", v->c.count);
        char *m = ws1_sb_take(&sb);
        json_t *r = ws1_raise("ValueError", m);
        free(m);
        return r;
    }
    if (ws1_is_dict(v)) {
        if (v->c.count == 2) { *a = json_string(v->c.keys[0]); *b = json_string(v->c.keys[1]); return NULL; }
        ws1_sb sb; ws1_sb_init(&sb);
        if (v->c.count > 2) ws1_sb_puts(&sb, "too many values to unpack (expected 2)");
        else ws1_sb_printf(&sb, "not enough values to unpack (expected 2, got %zu)", v->c.count);
        char *m = ws1_sb_take(&sb);
        json_t *r = ws1_raise("ValueError", m);
        free(m);
        return r;
    }
    ws1_sb sb; ws1_sb_init(&sb);
    ws1_sb_printf(&sb, "cannot unpack non-iterable %s object", ws1_type_name(v));
    char *m = ws1_sb_take(&sb);
    json_t *r = ws1_raise("TypeError", m);
    free(m);
    return r;
}

/* PoP: ws_u_denormalize_config_from_web @ hermes_cli/web_server_config.py:_denormalize_config_from_web */
json_t *ws_u_denormalize_config_from_web(json_t *req)
{
    json_t *config_in = ws1_get0(req, "config");
    if (!ws1_is_dict(config_in)) return ws1_not_iterable(config_in);
    json_t *config = json_copy(config_in);
    json_t *out = NULL;
    json_t *disk_cfg = NULL;
    char *model_val_str = NULL;
    json_t *model_val = NULL;   /* borrowed or owned copy */
    int model_val_owned = 0;
    long long ctx_override = 0;
    int ctx_is_bool = 0, ctx_bool_val = 0;

    json_obj_del(config, "_model_meta");
    int ctx_sent = json_obj_get(config, "model_context_length") != NULL;
    json_t *ctx_raw = json_obj_get(config, "model_context_length");
    json_t *ctx_copy = ctx_raw ? json_copy(ctx_raw) : json_number(0);
    json_obj_del(config, "model_context_length");
    /* isinstance(ctx_override, int): bool IS int; integral JSON numbers are ints */
    if (ctx_copy->type == JSON_BOOL) {
        ctx_is_bool = 1;
        ctx_bool_val = ctx_copy->bool_val;
        ctx_override = ctx_bool_val ? 1 : 0;
    } else if (ctx_copy->type == JSON_NUMBER && ws1_num_is_int(ctx_copy)) {
        ctx_override = (long long)ctx_copy->num_val;
    } else {
        long long v = 0;
        json_t *err = ws1_pyint(ctx_copy, &v);
        if (err) json_free(err);   /* except (TypeError, ValueError): 0 */
        else ctx_override = v;
    }
    json_free(ctx_copy);

    model_val = ws1_get0(config, "model");
    int has_model = ws1_is_str(model_val) && model_val->str_val[0];
    if (!has_model && !ctx_sent) { out = config; config = NULL; goto done; }

    disk_cfg = ws1_call("load_config", ws1_args0());
    if (ws1_is_raise(disk_cfg)) {
        if (ws1_is_base_exc(disk_cfg)) { out = disk_cfg; disk_cfg = NULL; goto done; }
        json_free(disk_cfg);
        disk_cfg = NULL;
        out = config; config = NULL;
        goto done;
    }
    if (!ws1_is_dict(disk_cfg)) {
        out = ws1_attr_error(disk_cfg, "get");
        goto done;
    }
    json_t *disk_model = ws1_get0(disk_cfg, "model");
    if (ws1_is_dict(disk_model)) {
        json_t *dm = json_copy(disk_model);
        if (has_model) {
            char *pd0 = ws1_str_or_empty(ws1_get0(dm, "default"));
            char *prev_default = ws1_strip(pd0); free(pd0);
            char *pp0 = ws1_str_or_empty(ws1_get0(dm, "provider"));
            char *prev_provider = ws1_strip(pp0); free(pp0);
            if (strcmp(model_val->str_val, prev_default) != 0 && prev_provider[0]) {
                json_t *infer = ws1_call("infer_provider_on_model_change",
                                         ws1_args2(json_string(model_val->str_val),
                                                   json_string(prev_provider)));
                if (ws1_is_raise(infer)) {
                    out = infer; free(prev_default); free(prev_provider); json_free(dm); goto done;
                }
                json_t *np = NULL, *rm = NULL;
                json_t *uerr = ws1_unpack2(infer, &np, &rm);
                if (uerr) {
                    out = uerr; json_free(infer);
                    free(prev_default); free(prev_provider); json_free(dm); goto done;
                }
                char *np_lo = NULL;
                int switch_provider = 0;
                if (ws1_jtruth(np)) {
                    if (!ws1_is_str(np)) {
                        out = ws1_attr_error(np, "strip");
                        json_free(infer); free(prev_default); free(prev_provider); json_free(dm);
                        goto done;
                    }
                    char *ns = ws1_strip(np->str_val);
                    np_lo = ws1_lower(ns); free(ns);
                    char *pp_lo = ws1_lower(prev_provider);
                    switch_provider = strcmp(np_lo, pp_lo) != 0;
                    free(pp_lo);
                }
                if (switch_provider) {
                    json_t *norm = ws1_call("normalize_main_model_assignment",
                                            ws1_args2(json_copy(np), json_copy(rm)));
                    if (ws1_is_raise(norm)) {
                        out = norm; json_free(infer); free(np_lo);
                        free(prev_default); free(prev_provider); json_free(dm); goto done;
                    }
                    json_t *nprov = NULL, *nmodel = NULL;
                    uerr = ws1_unpack2(norm, &nprov, &nmodel);
                    if (uerr) {
                        out = uerr; json_free(norm); json_free(infer); free(np_lo);
                        free(prev_default); free(prev_provider); json_free(dm); goto done;
                    }
                    json_t *result = ws1_call("validated_main_model_selection",
                                              ws1_args3(json_copy(disk_cfg), json_copy(nprov),
                                                        json_copy(nmodel)));
                    if (ws1_is_raise(result)) {
                        out = result; json_free(norm); json_free(infer); free(np_lo);
                        free(prev_default); free(prev_provider); json_free(dm); goto done;
                    }
                    json_t *applied = ws1_call("apply_main_model_assignment",
                                               ws1_args2(json_copy(dm), json_copy(result)));
                    if (ws1_is_raise(applied)) {
                        out = applied; json_free(result); json_free(norm); json_free(infer); free(np_lo);
                        free(prev_default); free(prev_provider); json_free(dm); goto done;
                    }
                    json_t *new_model = ws1_get0(result, "new_model");
                    if (!new_model) {
                        out = ws1_attr_error(result, "new_model");
                        json_free(applied); json_free(result); json_free(norm); json_free(infer); free(np_lo);
                        free(prev_default); free(prev_provider); json_free(dm); goto done;
                    }
                    json_free(dm);
                    dm = applied;
                    model_val_str = ws1_pystr(new_model);
                    if (!model_val_str) model_val_str = strdup("");
                    model_val = json_string(model_val_str);
                    model_val_owned = 1;
                    json_free(result);
                    json_free(norm);
                }
                json_free(infer);
                free(np_lo);
            }
            free(prev_default);
            free(prev_provider);
            json_set(dm, "default", model_val ? json_copy(model_val) : json_null());
        }
        if (ctx_sent) {
            if (ctx_override > 0)
                json_set(dm, "context_length",
                         ctx_is_bool ? json_bool(ctx_bool_val) : json_number((double)ctx_override));
            else
                json_obj_del(dm, "context_length");
        }
        json_set(config, "model", dm);
    } else if (ctx_sent && ctx_override > 0) {
        const char *dflt = "";
        char *dflt_alloc = NULL;
        if (has_model) {
            dflt = model_val->str_val;
        } else if (ws1_is_str(disk_model) && disk_model->str_val[0]) {
            dflt = disk_model->str_val;
        }
        json_t *m = json_object();
        json_set(m, "default", json_string(dflt));
        json_set(m, "context_length",
                 ctx_is_bool ? json_bool(ctx_bool_val) : json_number((double)ctx_override));
        json_set(config, "model", m);
        free(dflt_alloc);
    }
    out = config;
    config = NULL;
done:
    json_free(config);
    json_free(disk_cfg);
    if (model_val_owned) json_free(model_val);
    free(model_val_str);
    return out;
}

/* ================================================================
 * 9. _resolve_provider_status (+ _status_card, _truncate_token,
 *    _epoch_ms_to_iso, _PROVIDER_STATUS shapes)
 * ================================================================ */

static char *ws1_truncate_token(const json_t *value)
{
    if (!ws1_jtruth(value)) return strdup("");
    if (ws1_is_dict(value)) {
        json_t *c = json_obj_get(value, "__callable__");
        if (c && c->type == JSON_BOOL && c->bool_val)
            return strdup("<entra-id-bearer>");
    }
    char *s = ws1_pystr(value);
    if (!s) s = strdup("");
    /* JWT: >= 2 dots -> last segment */
    int dots = 0;
    for (const char *p = s; *p; p++) if (*p == '.') dots++;
    if (dots >= 2) {
        const char *last = strrchr(s, '.');
        char *seg = strdup(last + 1);
        free(s);
        s = seg;
    }
    char *r;
    if (ws1_utf8_len(s) <= 6) {
        r = s;
    } else {
        char *tail = ws1_utf8_tail(s, 6);
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_puts(&b, "\xE2\x80\xA6");   /* U+2026 HORIZONTAL ELLIPSIS */
        ws1_sb_puts(&b, tail);
        free(tail); free(s);
        r = ws1_sb_take(&b);
    }
    return r;
}

static json_t *ws1_epoch_ms_to_iso(const json_t *value)
{
    if (!ws1_jtruth(value)) return json_null();
    if (!ws1_is_str(value) && value->type != JSON_NUMBER && value->type != JSON_BOOL) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "unsupported operand type(s) for /: '%s' and 'int'",
                      ws1_type_name(value));
        char *m = ws1_sb_take(&b);
        json_t *r = ws1_raise("TypeError", m);
        free(m);
        return r;
    }
    double ms = value->type == JSON_NUMBER ? value->num_val
              : value->type == JSON_BOOL ? (value->bool_val ? 1.0 : 0.0) : 0.0;
    if (ws1_is_str(value)) {
        /* str / int is a TypeError before any parsing */
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_puts(&b, "unsupported operand type(s) for /: 'str' and 'int'");
        char *m = ws1_sb_take(&b);
        json_t *r = ws1_raise("TypeError", m);
        free(m);
        return r;
    }
    long long total_us = (long long)llround(ms * 1000.0);
    time_t sec = (time_t)(total_us / 1000000);
    long us = (long)(total_us % 1000000);
    if (us < 0) { us += 1000000; sec -= 1; }
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &sec);
#else
    gmtime_r(&sec, &tmv);
#endif
    char buf[64];
    snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_puts(&b, buf);
    if (us != 0) ws1_sb_printf(&b, ".%06ld", us);
    ws1_sb_puts(&b, "+00:00");
    char *s = ws1_sb_take(&b);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* _status_card(raw, source, source_label, token_preview, expires_at,
 *              has_refresh_token, last_refresh=_OMIT) */
static json_t *ws1_status_card(const json_t *raw, const char *source,
                               const char *source_label, const json_t *token_preview,
                               const json_t *expires_at, int has_refresh_token,
                               int include_last_refresh, const json_t *last_refresh)
{
    json_t *card = json_object();
    json_set(card, "logged_in", json_bool(ws1_jtruth(ws1_get0(raw, "logged_in"))));
    json_set(card, "source", json_string(source));
    json_set(card, "source_label", json_string(source_label));
    json_set(card, "token_preview", token_preview ? json_copy(token_preview) : json_null());
    json_set(card, "expires_at", expires_at ? json_copy(expires_at) : json_null());
    json_set(card, "has_refresh_token", json_bool(has_refresh_token));
    if (include_last_refresh)
        json_set(card, "last_refresh", last_refresh ? json_copy(last_refresh) : json_null());
    return card;
}

/* _PROVIDER_STATUS getter names, pinned (oracle fail-closed check) */
static const char *WS1_STATUS_GETTERS[] = {
    "get_nous_auth_status_local", "get_codex_auth_status", "get_qwen_auth_status",
    "get_minimax_oauth_auth_status", "get_xai_oauth_auth_status"
};
static const char *WS1_STATUS_IDS[] = {
    "nous", "openai-codex", "qwen-oauth", "minimax-oauth", "xai-oauth"
};

static json_t *ws1_provider_status_shape(int idx, const json_t *r)
{
    char *tok = NULL;
    json_t *tok_node = NULL;
    json_t *card = NULL;
    switch (idx) {
    case 0: { /* nous */
        char *portal = ws1_str_or_empty(ws1_get0(r, "portal_base_url"));
        if (!portal[0]) { free(portal); portal = strdup("Nous Portal"); }
        tok = ws1_truncate_token(ws1_get0(r, "access_token"));
        tok_node = json_string(tok);
        card = ws1_status_card(r, "nous_portal", portal, tok_node,
                               ws1_get0(r, "access_expires_at"),
                               ws1_jtruth(ws1_get0(r, "has_refresh_token")), 0, NULL);
        free(portal);
        json_set(card, "free_tier", json_bool(ws1_jtruth(ws1_get0(r, "free_tier"))));
        json_t *tier = ws1_get0(r, "account_tier");
        json_set(card, "account_tier", tier ? json_copy(tier) : json_null());
        break;
    }
    case 1: { /* openai-codex */
        char *src = ws1_str_or_empty(ws1_get0(r, "source"));
        if (!src[0]) { free(src); src = strdup("openai_codex"); }
        char *mode = ws1_str_or_empty(ws1_get0(r, "auth_mode"));
        if (!mode[0]) { free(mode); mode = strdup("OpenAI Codex"); }
        tok = ws1_truncate_token(ws1_get0(r, "api_key"));
        tok_node = json_string(tok);
        card = ws1_status_card(r, src, mode, tok_node, NULL, 0, 1, ws1_get0(r, "last_refresh"));
        free(src); free(mode);
        break;
    }
    case 2: { /* qwen-oauth */
        char *af = ws1_str_or_empty(ws1_get0(r, "auth_file"));
        if (!af[0]) { free(af); af = strdup("Qwen CLI"); }
        tok = ws1_truncate_token(ws1_get0(r, "api_key"));
        tok_node = json_string(tok);
        json_t *iso = ws1_epoch_ms_to_iso(ws1_get0(r, "expires_at_ms"));
        if (ws1_is_raise(iso)) { free(af); free(tok); json_free(tok_node); return iso; }
        card = ws1_status_card(r, "qwen_cli", af, tok_node, iso,
                               ws1_jtruth(ws1_get0(r, "has_refresh_token")), 0, NULL);
        json_free(iso);
        free(af);
        break;
    }
    case 3: { /* minimax-oauth */
        json_t *region = ws1_get0(r, "region");
        char *rs = region ? ws1_pystr(region) : NULL;
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "MiniMax (%s)", rs ? rs : "global");
        free(rs);
        char *label = ws1_sb_take(&b);
        card = ws1_status_card(r, "minimax_oauth", label, NULL,
                               ws1_get0(r, "expires_at"), 1, 0, NULL);
        free(label);
        break;
    }
    default: { /* xai-oauth */
        char *src = ws1_str_or_empty(ws1_get0(r, "source"));
        if (!src[0]) { free(src); src = strdup("xai_oauth"); }
        json_t *store = ws1_get0(r, "auth_store");
        char *label;
        if (ws1_jtruth(store)) {
            label = ws1_pystr(store);
            if (!label) label = strdup("");
        } else if (ws1_jtruth(ws1_get0(r, "source"))) {
            label = ws1_pystr(ws1_get0(r, "source"));
            if (!label) label = strdup("");
        } else {
            label = strdup("xAI Grok OAuth");
        }
        tok = ws1_truncate_token(ws1_get0(r, "api_key"));
        tok_node = json_string(tok);
        card = ws1_status_card(r, src, label, tok_node, NULL, 1, 1, ws1_get0(r, "last_refresh"));
        free(src); free(label);
        break;
    }
    }
    free(tok);
    json_free(tok_node);
    return card;
}

/* PoP: ws_u_resolve_provider_status @ hermes_cli/web_routers/oauth.py:_resolve_provider_status */
json_t *ws_u_resolve_provider_status(json_t *req)
{
    const char *provider_id = json_get_str(req, "provider_id", "");
    json_t *status_fn = ws1_get0(req, "status_fn");
    json_t *out = NULL;

    /* try: whole body; except Exception -> {"logged_in": False, "error": str(e)} */
    if (status_fn && status_fn->type != JSON_NULL) {
        int is_fn_marker = ws1_is_str(status_fn) && strcmp(status_fn->str_val, "fn") == 0;
        if (!is_fn_marker) {
            out = ws1_not_callable(status_fn);
        } else {
            out = ws1_call("status_fn", ws1_args0());
        }
        goto wrap;
    }
    int idx = -1;
    for (size_t i = 0; i < 5; i++)
        if (strcmp(provider_id, WS1_STATUS_IDS[i]) == 0) { idx = (int)i; break; }
    if (idx >= 0) {
        json_t *raw = ws1_call(WS1_STATUS_GETTERS[idx], ws1_args0());
        if (ws1_is_raise(raw)) { out = raw; goto wrap; }
        out = ws1_provider_status_shape(idx, raw);
        json_free(raw);
        goto wrap;
    }
    json_t *raw = ws1_call("get_auth_status", ws1_args1(json_string(provider_id)));
    if (ws1_is_raise(raw)) { out = raw; goto wrap; }
    if (ws1_is_dict(raw) && json_obj_get(raw, "logged_in")) {
        char *source = ws1_str_or_empty(ws1_get0(raw, "source"));
        if (!source[0]) { free(source); source = ws1_str_or_empty(ws1_get0(raw, "provider")); }
        if (!source[0]) { free(source); source = strdup(provider_id); }
        const char *label_keys[5] = {"source_label", "auth_store", "auth_store_path", "base_url", "name"};
        char *label = NULL;
        for (int i = 0; i < 5; i++) {
            json_t *v = ws1_get0(raw, label_keys[i]);
            if (ws1_jtruth(v)) { label = ws1_pystr(v); if (!label) label = strdup(""); break; }
        }
        if (!label) label = strdup("");
        json_t *tok_val = ws1_get0(raw, "access_token");
        if (!ws1_jtruth(tok_val)) tok_val = ws1_get0(raw, "api_key");
        char *tok = ws1_truncate_token(tok_val);
        json_t *tok_node = json_string(tok);
        json_t *expires = ws1_get0(raw, "expires_at");
        if (!ws1_jtruth(expires)) expires = ws1_get0(raw, "access_expires_at");
        out = ws1_status_card(raw, source, label, tok_node, expires,
                              ws1_jtruth(ws1_get0(raw, "has_refresh_token")), 0, NULL);
        free(source); free(label); free(tok);
        json_free(tok_node);
    } else {
        out = json_object();
        json_set(out, "logged_in", json_bool(false));
    }
    json_free(raw);
wrap:
    if (ws1_is_raise(out) && !ws1_is_base_exc(out)) {
        const char *msg = json_get_str(out, "msg", "");
        json_free(out);
        out = json_object();
        json_set(out, "logged_in", json_bool(false));
        json_set(out, "error", json_string(msg));
    }
    return out;
}/* ================================================================
 * 10. _maybe_auto_archive_for_profile
 * ================================================================ */

static json_t *ws1_state_obj(const char *key)
{
    if (!ws1_state) return NULL;
    json_t *v = json_obj_get(ws1_state, key);
    if (!v) { v = json_object(); json_set(ws1_state, key, v); }
    return v;
}

static json_t *ws1_state_get(const char *key)
{
    return ws1_state ? json_obj_get(ws1_state, key) : NULL;
}

/* PoP: ws_u_maybe_auto_archive_for_profile @ hermes_cli/web_server_sessions.py:_maybe_auto_archive_for_profile */
json_t *ws_u_maybe_auto_archive_for_profile(json_t *req)
{
    json_t *profile = ws1_get0(req, "profile");
    const char *key = (ws1_is_str(profile) && profile->str_val[0]) ? profile->str_val : "";
    json_t *check = ws1_state_obj("last_auto_archive_check");
    json_t *out = NULL;
    json_t *err = NULL;
    json_t *now = NULL, *home_token = NULL, *store = NULL;
    json_t *full_cfg = NULL, *cfg_owned = NULL, *cfg = NULL;
    char *parent = NULL;
    int have_home_token = 0;

    now = ws1_call("time_monotonic", ws1_args0());
    if (ws1_is_raise(now)) { err = now; now = NULL; goto except; }
    json_t *last = check ? ws1_get0(check, key) : NULL;
    if (last) {
        if (last->type != JSON_NUMBER || now->type != JSON_NUMBER) {
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b, "unsupported operand type(s) for -: '%s' and '%s'",
                          ws1_type_name(now), ws1_type_name(last));
            char *m = ws1_sb_take(&b);
            err = ws1_raise("TypeError", m);
            free(m);
            goto except;
        }
        if (now->num_val - last->num_val < WS1_AUTO_ARCHIVE_CHECK_INTERVAL_S) goto done;
    }
    if (check) json_set(check, key, json_copy(now));

    json_t *dbpath = ws1_call("session_db_path_for_profile",
                              ws1_args1(profile ? json_copy(profile) : json_null()));
    if (ws1_is_raise(dbpath)) { err = dbpath; goto except; }
    if (!dbpath || dbpath->type == JSON_NULL) {
        json_free(dbpath);
        err = ws1_attr_error(NULL, "parent");
        goto except;
    }
    char *dbs = ws1_pystr(dbpath);
    json_free(dbpath);
    if (!dbs) dbs = strdup("");
    parent = ws1_path_parent(dbs);
    free(dbs);

    home_token = ws1_call("set_hermes_home_override", ws1_args1(json_string(parent)));
    if (ws1_is_raise(home_token)) { err = home_token; home_token = NULL; goto except; }
    have_home_token = 1;

    full_cfg = ws1_call("load_config", ws1_args0());
    if (ws1_is_raise(full_cfg)) { err = full_cfg; full_cfg = NULL; goto reset; }
    if (!ws1_is_dict(full_cfg)) {
        err = ws1_attr_error(full_cfg, "get");
        goto reset;
    }
    json_t *sessions = ws1_get0(full_cfg, "sessions");
    if (ws1_jtruth(sessions)) {
        cfg = sessions;   /* borrowed from full_cfg */
    } else {
        cfg_owned = json_object();
        cfg = cfg_owned;
    }

reset:
    if (have_home_token) {
        json_t *reset_r = ws1_call("reset_hermes_home_override", ws1_args1(json_copy(home_token)));
        have_home_token = 0;
        if (ws1_is_raise(reset_r)) {
            json_free(err);
            err = reset_r;
            goto except;
        }
        json_free(reset_r);
    }
    if (err) goto except;

    if (!ws1_is_dict(cfg)) { err = ws1_attr_error(cfg, "get"); goto except; }
    json_t *aa = ws1_get0(cfg, "auto_archive");
    if (!ws1_jtruth(aa)) goto done;

    json_t *running = ws1_call("check_gateway_running", ws1_args1(json_string(parent)));
    if (ws1_is_raise(running)) { err = running; goto except; }
    int live = ws1_jtruth(running);
    json_free(running);
    if (live) goto done;

    json_t *scope = ws1_call("profile_scope", ws1_args1(json_string(parent)));
    if (ws1_is_raise(scope)) { err = scope; goto except; }
    store = ws1_call("open_persistence", ws1_args1(scope));
    if (ws1_is_raise(store)) { err = store; store = NULL; goto except; }

    {
        json_t *days_raw = ws1_get0(cfg, "auto_archive_days");
        double idle_days = 3.0;
        if (days_raw) {
            json_t *fe = ws1_pyfloat(days_raw, &idle_days);
            if (fe) { err = fe; goto close_store; }
        }
        json_t *hours_raw = ws1_get0(cfg, "min_interval_hours");
        long long min_hours = 24;
        if (hours_raw) {
            json_t *ie = ws1_pyint(hours_raw, &min_hours);
            if (ie) { err = ie; goto close_store; }
        }
        json_t *run = ws1_call("run_auto_archive",
                               ws1_args3(json_copy(store), json_number(idle_days),
                                         json_number((double)min_hours)));
        if (ws1_is_raise(run)) err = run;
        else json_free(run);
    }
close_store:
    {
        json_t *closed = ws1_call("store_close", ws1_args1(json_copy(store)));
        if (ws1_is_raise(closed)) {
            json_free(err);
            err = closed;
        } else {
            json_free(closed);
        }
    }
    if (err) goto except;
    goto done;

except:
    if (err && ws1_is_base_exc(err)) { out = err; err = NULL; goto cleanup; }
    {
        const char *msg = err ? json_get_str(err, "msg", "") : "";
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "opportunistic auto-archive skipped: %s", msg);
        char *m = ws1_sb_take(&b);
        json_t *ld = ws1_call("log_debug", ws1_args1(json_string(m)));
        free(m);
        if (ws1_is_raise(ld)) out = ld;
        else { json_free(ld); out = json_null(); }
    }
    json_free(err);
    err = NULL;
    goto cleanup;
done:
    out = json_null();
cleanup:
    json_free(now);
    json_free(home_token);
    json_free(store);
    json_free(full_cfg);
    json_free(cfg_owned);
    free(parent);
    return out;
}

/* ================================================================
 * 11. _spawn_gateway_restart
 * ================================================================ */

/* PoP: ws_u_spawn_gateway_restart @ hermes_cli/web_server.py:_spawn_gateway_restart */
json_t *ws_u_spawn_gateway_restart(json_t *req)
{
    json_t *profile = ws1_get0(req, "profile");
    int interrupt = ws1_jtruth(ws1_get0(req, "interrupt"));
    json_t *procs = ws1_state_obj("action_procs");
    json_t *commands = ws1_state_obj("action_commands");
    json_t *out = NULL;

    json_t *sub = ws1_call("gateway_subcommand",
                           ws1_args2(profile ? json_copy(profile) : json_null(),
                                     json_string("restart")));
    if (ws1_is_raise(sub)) return sub;
    if (!ws1_is_list(sub)) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "can only concatenate list (not \"%s\") to list", ws1_type_name(sub));
        char *m = ws1_sb_take(&b);
        out = ws1_raise("TypeError", m);
        free(m);
        json_free(sub);
        return out;
    }
    if (interrupt) json_append(sub, json_string("--interrupt"));

    json_t *existing = procs ? ws1_get0(procs, "gateway-restart") : NULL;
    if (existing) {
        if (!ws1_is_dict(existing)) {
            out = ws1_attr_error(existing, "poll");
            json_free(sub);
            return out;
        }
        json_t *poll = ws1_get0(existing, "poll");
        if (!poll || poll->type == JSON_NULL) {
            json_t *ec = commands ? ws1_get0(commands, "gateway-restart") : NULL;
            if (!ec || ws1_eq(ec, sub)) {
                out = json_array();
                json_append(out, json_copy(existing));
                json_append(out, json_bool(true));
                json_free(sub);
                return out;
            }
            json_free(sub);
            return ws1_raise("RuntimeError",
                             "gateway restart already in progress for another profile");
        }
    }

    json_t *recent = ws1_state_get("last_gateway_restart");
    if (recent && recent->type != JSON_NULL) {
        if (!ws1_is_list(recent) || recent->c.count != 3) {
            ws1_sb b; ws1_sb_init(&b);
            size_t got = ws1_is_list(recent) ? recent->c.count : 0;
            if (ws1_is_list(recent) && recent->c.count > 3)
                ws1_sb_puts(&b, "too many values to unpack (expected 3)");
            else if (ws1_is_list(recent))
                ws1_sb_printf(&b, "not enough values to unpack (expected 3, got %zu)", got);
            else
                ws1_sb_printf(&b, "cannot unpack non-iterable %s object", ws1_type_name(recent));
            char *m = ws1_sb_take(&b);
            out = ws1_raise(ws1_is_list(recent) ? "ValueError" : "TypeError", m);
            free(m);
            json_free(sub);
            return out;
        }
        json_t *spawned_at = recent->c.items[0];
        json_t *recent_proc = recent->c.items[1];
        json_t *recent_command = recent->c.items[2];
        double age = 0;
        int have_age = 0;
        if (ws1_eq(recent_command, sub)) {
            json_t *mono = ws1_call("time_monotonic", ws1_args0());
            if (ws1_is_raise(mono)) { json_free(sub); return mono; }
            if (mono->type != JSON_NUMBER || spawned_at->type != JSON_NUMBER) {
                ws1_sb b; ws1_sb_init(&b);
                ws1_sb_printf(&b, "unsupported operand type(s) for -: '%s' and '%s'",
                              ws1_type_name(mono), ws1_type_name(spawned_at));
                char *m = ws1_sb_take(&b);
                out = ws1_raise("TypeError", m);
                free(m);
                json_free(mono);
                json_free(sub);
                return out;
            }
            age = mono->num_val - spawned_at->num_val;
            have_age = 1;
            json_free(mono);
        }
        if (have_age && age < WS1_GATEWAY_RESTART_COOLDOWN_SECONDS) {
            json_t *pid = ws1_is_dict(recent_proc) ? ws1_get0(recent_proc, "pid") : NULL;
            char *pids = pid ? ws1_pystr(pid) : strdup("?");
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b,
                "Coalescing gateway restart: one was started %.1fs ago "
                "(pid %s) and the gateway may still be coming back; not "
                "spawning another (#89034).", age, pids ? pids : "?");
            free(pids);
            char *m = ws1_sb_take(&b);
            json_t *li = ws1_call("log_info", ws1_args1(json_string(m)));
            free(m);
            if (ws1_is_raise(li)) { json_free(sub); return li; }
            json_free(li);
            out = json_array();
            json_append(out, json_copy(recent_proc));
            json_append(out, json_bool(true));
            json_free(sub);
            return out;
        }
    }

    json_t *proc = ws1_call("spawn_hermes_action",
                            ws1_args2(json_copy(sub), json_string("gateway-restart")));
    if (ws1_is_raise(proc)) { json_free(sub); return proc; }
    json_t *mono2 = ws1_call("time_monotonic", ws1_args0());
    if (ws1_is_raise(mono2)) { json_free(proc); json_free(sub); return mono2; }
    json_t *last = json_array();
    json_append(last, mono2);
    json_append(last, json_copy(proc));
    json_append(last, json_copy(sub));
    if (ws1_state) json_set(ws1_state, "last_gateway_restart", last);
    else json_free(last);
    out = json_array();
    json_append(out, proc);
    json_append(out, json_bool(false));
    json_free(sub);
    return out;
}

/* ================================================================
 * 12. _custom_endpoint_response (+ siblings)
 * ================================================================ */

/* hermes_cli/config_providers.py:coerce_provider_id */
static char *ws1_coerce_provider_id(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return strdup("");
    char *s = ws1_pystr(v);
    if (!s) s = strdup("");
    char *r = ws1_strip(s);
    free(s);
    return r;
}

/* hermes_cli/config_providers.py:_canonical_api_mode */
static char *ws1_canonical_api_mode(const char *raw)
{
    char *cleaned = ws1_strip(raw);
    char *lo = ws1_lower(cleaned);
    const char *canonical = NULL;
    for (size_t i = 0; i < sizeof WS1_API_MODE_ALIASES / sizeof WS1_API_MODE_ALIASES[0]; i++)
        if (strcmp(lo, WS1_API_MODE_ALIASES[i].alias) == 0) {
            canonical = WS1_API_MODE_ALIASES[i].canonical;
            break;
        }
    free(lo);
    if (canonical) { free(cleaned); return strdup(canonical); }
    return cleaned;
}

/* _custom_endpoint_id: re.sub(r"[^A-Za-z0-9_-]+", "-", coerce(raw)).strip("-_").lower() */
static char *ws1_custom_endpoint_id(const char *raw)
{
    /* coerce_provider_id(raw) on the string value */
    char *cv = ws1_strip(raw ? raw : "");
    ws1_sb b; ws1_sb_init(&b);
    int in_run = 0;
    for (const char *p = cv; *p; p++) {
        unsigned char c = (unsigned char)*p;
        int allowed = isalnum(c) || c == '_' || c == '-';
        if (allowed) { ws1_sb_putc(&b, (char)c); in_run = 0; }
        else if (!in_run) { ws1_sb_putc(&b, '-'); in_run = 1; }
    }
    free(cv);
    char *sub = ws1_sb_take(&b);
    size_t n = strlen(sub);
    size_t a = 0, z = n;
    while (a < z && (sub[a] == '-' || sub[a] == '_')) a++;
    while (z > a && (sub[z - 1] == '-' || sub[z - 1] == '_')) z--;
    char *trimmed = ws1_strndup(sub + a, z - a);
    free(sub);
    char *lo = ws1_lower(trimmed);
    free(trimmed);
    if (!lo[0]) { free(lo); return strdup("custom"); }
    return lo;
}

/* _models_from_custom_endpoint_entry */
static json_t *ws1_models_from_entry(const json_t *entry)
{
    json_t *models = json_array();
    json_t *raw_models = ws1_get0(entry, "models");
    if (ws1_is_dict(raw_models)) {
        for (size_t i = 0; i < raw_models->c.count; i++) {
            char *s = ws1_strip(raw_models->c.keys[i]);
            json_append(models, json_string(s));
            free(s);
        }
    } else if (ws1_is_list(raw_models)) {
        for (size_t i = 0; i < raw_models->c.count; i++) {
            char *s0 = ws1_pystr(raw_models->c.items[i]);
            char *s = ws1_strip(s0 ? s0 : "");
            free(s0);
            json_append(models, json_string(s));
            free(s);
        }
    }
    char *d0 = ws1_str_or_empty(ws1_get0(entry, "model"));
    if (!d0[0]) { free(d0); d0 = ws1_str_or_empty(ws1_get0(entry, "default_model")); }
    char *default_model = ws1_strip(d0);
    free(d0);
    if (default_model[0]) {
        /* models.insert(0, default_model) */
        json_t *shifted = json_array();
        json_append(shifted, json_string(default_model));
        for (size_t i = 0; i < models->c.count; i++)
            json_append(shifted, json_copy(models->c.items[i]));
        json_free(models);
        models = shifted;
    }
    free(default_model);
    /* dedupe, dropping empties, first occurrence wins */
    json_t *dedup = json_array();
    for (size_t i = 0; i < models->c.count; i++) {
        const char *m = models->c.items[i]->str_val;
        if (!m[0]) continue;
        int seen = 0;
        for (size_t j = 0; j < dedup->c.count; j++)
            if (strcmp(dedup->c.items[j]->str_val, m) == 0) { seen = 1; break; }
        if (!seen) json_append(dedup, json_copy(models->c.items[i]));
    }
    json_free(models);
    return dedup;
}

/* _endpoint_api_mode */
static char *ws1_endpoint_api_mode(const json_t *entry)
{
    char *r0 = ws1_str_or_empty(ws1_get0(entry, "api_mode"));
    if (!r0[0]) { free(r0); r0 = ws1_str_or_empty(ws1_get0(entry, "transport")); }
    char *canon = ws1_canonical_api_mode(r0);
    free(r0);
    char *mode = ws1_lower(canon);
    free(canon);
    if (!ws1_in_strset(mode, WS1_DESKTOP_API_MODES, 3)) { free(mode); return strdup(""); }
    return mode;
}

/* _api_key_display -> (has_api_key, api_key_preview); raise spec on seam error */
static json_t *ws1_api_key_display(const json_t *entry, int *has, json_t **preview)
{
    *has = 0;
    *preview = NULL;
    char *p0 = ws1_str_or_empty(ws1_get0(entry, "api_key"));
    char *plaintext = ws1_strip(p0);
    free(p0);
    if (plaintext[0]) {
        json_t *pv = ws1_call("redacted_credential_preview", ws1_args1(json_string(plaintext)));
        free(plaintext);
        if (ws1_is_raise(pv)) return pv;
        *has = 1;
        *preview = pv;
        return NULL;
    }
    free(plaintext);
    char *k0 = ws1_str_or_empty(ws1_get0(entry, "key_env"));
    char *key_env = ws1_strip(k0);
    free(k0);
    if (key_env[0]) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "${%s}", key_env);
        char *s = ws1_sb_take(&b);
        *has = 1;
        *preview = json_string(s);
        free(s);
    }
    free(key_env);
    return NULL;
}

/* _model_names_provider */
static int ws1_model_names_provider(const json_t *model_cfg, const char *provider_key,
                                    const json_t *entry)
{
    char *key_coerced = ws1_strip(provider_key ? provider_key : "");
    char *key_lo = ws1_lower(key_coerced);
    free(key_coerced);
    char *name_lo = NULL;
    if (ws1_is_dict(entry)) {
        char *nc = ws1_coerce_provider_id(ws1_get0(entry, "name"));
        if (nc[0]) name_lo = ws1_lower(nc);
        free(nc);
    }
    char *c0 = ws1_str_or_empty(ws1_get0(model_cfg, "provider"));
    char *cs = ws1_strip(c0);
    free(c0);
    char *current = ws1_lower(cs);
    free(cs);
    char *bare = ws1_removeprefix_custom(current);
    free(current);
    int match = strcmp(bare, key_lo) == 0 || (name_lo && strcmp(bare, name_lo) == 0);
    free(bare);
    free(key_lo);
    free(name_lo);
    return match;
}

/* _endpoint_row */
static json_t *ws1_endpoint_row(const char *endpoint_id, const char *name,
                                const char *base_url, const char *model,
                                const json_t *models, const json_t *context_length,
                                int discover_models, const json_t *key_entry,
                                int is_current, const char *source, json_t **err)
{
    int has = 0;
    json_t *preview = NULL;
    json_t *e = ws1_api_key_display(key_entry, &has, &preview);
    if (e) { *err = e; return NULL; }
    char *api_mode = ws1_endpoint_api_mode(key_entry);
    json_t *row = json_object();
    json_set(row, "id", json_string(endpoint_id));
    json_set(row, "name", json_string(name));
    json_set(row, "base_url", json_string(base_url));
    json_set(row, "model", json_string(model));
    json_set(row, "models", json_copy(models));
    json_set(row, "api_mode", json_string(api_mode));
    json_set(row, "context_length", context_length ? json_copy(context_length) : json_null());
    json_set(row, "discover_models", json_bool(discover_models));
    json_set(row, "has_api_key", json_bool(has));
    json_set(row, "api_key_preview", preview ? preview : json_null());
    json_set(row, "is_current", json_bool(is_current));
    json_set(row, "source", json_string(source));
    free(api_mode);
    *err = NULL;
    return row;
}

/* PoP: ws_u_custom_endpoint_response @ hermes_cli/web_routers/config_env.py:_custom_endpoint_response */
json_t *ws_u_custom_endpoint_response(json_t *req)
{
    json_t *cfg = ws1_get0(req, "cfg");
    json_t *out = NULL;
    json_t *endpoints = json_array();
    char *current_provider = NULL, *current_model = NULL, *current_base_url = NULL;

    json_t *model_cfg = ws1_is_dict(ws1_get0(cfg, "model")) ? ws1_get0(cfg, "model") : NULL;
    current_provider = ws1_str_or_empty(ws1_get0(model_cfg, "provider"));
    {
        json_t *dflt = ws1_get0(model_cfg, "default");
        if (!dflt) dflt = ws1_get0(model_cfg, "name");
        current_model = ws1_str_or_empty(dflt);
    }
    current_base_url = ws1_str_or_empty(ws1_get0(model_cfg, "base_url"));

    json_t *providers = ws1_get0(cfg, "providers");
    if (ws1_is_dict(providers)) {
        for (size_t i = 0; i < providers->c.count && !out; i++) {
            const char *provider_id = providers->c.keys[i];
            json_t *raw_entry = providers->c.items[i];
            if (!ws1_is_dict(raw_entry)) continue;
            char *b0 = ws1_str_or_empty(ws1_get0(raw_entry, "base_url"));
            if (!b0[0]) { free(b0); b0 = ws1_str_or_empty(ws1_get0(raw_entry, "url")); }
            if (!b0[0]) { free(b0); b0 = ws1_str_or_empty(ws1_get0(raw_entry, "api")); }
            char *base_url = ws1_strip(b0);
            free(b0);
            if (!base_url[0]) { free(base_url); continue; }
            json_t *models = ws1_models_from_entry(raw_entry);
            char *m0 = ws1_str_or_empty(ws1_get0(raw_entry, "model"));
            if (!m0[0]) { free(m0); m0 = ws1_str_or_empty(ws1_get0(raw_entry, "default_model")); }
            if (!m0[0] && models->c.count > 0) {
                free(m0);
                m0 = strdup(models->c.items[0]->str_val);
            }
            char *nm0 = ws1_str_or_empty(ws1_get0(raw_entry, "name"));
            if (!nm0[0]) { free(nm0); nm0 = strdup(provider_id); }
            json_t *dm = ws1_get0(raw_entry, "discover_models");
            int discover = dm ? ws1_jtruth(dm) : 1;
            int is_current = ws1_model_names_provider(model_cfg, provider_id, raw_entry);
            json_t *err = NULL;
            json_t *row = ws1_endpoint_row(provider_id, nm0, base_url, m0, models,
                                           ws1_get0(raw_entry, "context_length"), discover,
                                           raw_entry, is_current, "providers", &err);
            free(m0); free(nm0); free(base_url);
            json_free(models);
            if (err) { out = err; break; }
            json_append(endpoints, row);
        }
    }
    if (out) goto done;

    char *cp_lo = ws1_lower(current_provider);
    int is_bare_custom = strcmp(cp_lo, "custom") == 0 && current_base_url[0];
    free(cp_lo);

    json_t *legacy = ws1_call("get_compatible_custom_providers",
                              ws1_args1(cfg ? json_copy(cfg) : json_null()));
    if (ws1_is_raise(legacy)) { out = legacy; goto done; }
    if (!ws1_is_list(legacy)) {
        out = ws1_not_iterable(legacy);
        json_free(legacy);
        goto done;
    }
    char *cur_rstrip = ws1_rstrip_slash(current_base_url);
    for (size_t i = 0; i < legacy->c.count && !out; i++) {
        json_t *entry = legacy->c.items[i];
        if (!ws1_is_dict(entry)) { out = ws1_attr_error(entry, "get"); break; }
        if (ws1_jtruth(ws1_get0(entry, "provider_key"))) continue;
        json_t *err = NULL;
        json_t *name_v = ws1_item(entry, "name", &err);
        if (err) { out = err; break; }
        char *name = ws1_pystr(name_v);
        if (!name) name = strdup("");
        char *endpoint_id = ws1_custom_endpoint_id(name);
        int seen = 0;
        for (size_t j = 0; j < endpoints->c.count; j++)
            if (strcmp(json_get_str(endpoints->c.items[j], "id", ""), endpoint_id) == 0) {
                seen = 1; break;
            }
        if (seen) { free(name); free(endpoint_id); continue; }
        json_t *models = ws1_models_from_entry(entry);
        json_t *bu_v = ws1_item(entry, "base_url", &err);
        if (err) { out = err; free(name); free(endpoint_id); json_free(models); break; }
        char *bu = ws1_pystr(bu_v);
        if (!bu) bu = strdup("");
        char *bu_rstrip = ws1_rstrip_slash(bu);
        int is_current = is_bare_custom && strcmp(bu_rstrip, cur_rstrip) == 0;
        free(bu_rstrip);
        char *m0 = ws1_str_or_empty(ws1_get0(entry, "model"));
        if (!m0[0] && models->c.count > 0) { free(m0); m0 = strdup(models->c.items[0]->str_val); }
        json_t *dm = ws1_get0(entry, "discover_models");
        int discover = dm ? ws1_jtruth(dm) : 1;
        json_t *row = ws1_endpoint_row(endpoint_id, name, bu, m0, models,
                                       ws1_get0(entry, "context_length"), discover,
                                       entry, is_current, "custom_providers", &err);
        free(m0); free(bu); free(name); free(endpoint_id);
        json_free(models);
        if (err) { out = err; break; }
        json_append(endpoints, row);
    }
    free(cur_rstrip);
    json_free(legacy);
    if (out) goto done;

    if (is_bare_custom) {
        int covered = 0;
        for (size_t j = 0; j < endpoints->c.count; j++) {
            json_t *e = endpoints->c.items[j];
            if (strcmp(json_get_str(e, "id", ""), "custom") == 0
                || ws1_jtruth(ws1_get0(e, "is_current"))) { covered = 1; break; }
        }
        if (!covered) {
            json_t *models = json_array();
            if (current_model[0]) json_append(models, json_string(current_model));
            json_t *err = NULL;
            json_t *row = ws1_endpoint_row("custom", "Custom", current_base_url, current_model,
                                           models, ws1_get0(model_cfg, "context_length"), 1,
                                           model_cfg, 1, "direct-config", &err);
            json_free(models);
            if (err) { out = err; goto done; }
            /* endpoints.insert(0, row) */
            json_t *shifted = json_array();
            json_append(shifted, row);
            for (size_t j = 0; j < endpoints->c.count; j++)
                json_append(shifted, json_copy(endpoints->c.items[j]));
            json_free(endpoints);
            endpoints = shifted;
        }
    }

    out = json_object();
    json_set(out, "endpoints", endpoints);
    endpoints = NULL;
    json_t *current = json_object();
    json_set(current, "provider", json_string(current_provider));
    json_set(current, "model", json_string(current_model));
    json_set(current, "base_url", json_string(current_base_url));
    json_set(out, "current", current);
done:
    json_free(endpoints);
    free(current_provider);
    free(current_model);
    free(current_base_url);
    return out;
}

/* ================================================================
 * 13. _get_models_analytics (+ _merge_aux_into_rows,
 *     _fold_session_only_rows, _has_usage)
 * ================================================================ */

static int ws1_has_usage(const json_t *row)
{
    for (size_t i = 0; i < sizeof WS1_USAGE_KEYS / sizeof WS1_USAGE_KEYS[0]; i++)
        if (ws1_jtruth(ws1_get0(row, WS1_USAGE_KEYS[i]))) return 1;
    return 0;
}

/* (v or 0) as a number; truthy non-numbers raise like Python's + would */
static json_t *ws1_num_or_zero(const json_t *v, double *out)
{
    if (!ws1_jtruth(v)) { *out = 0.0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = v->num_val; return NULL; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return NULL; }
    return ws1_raise("TypeError", "unsupported operand type(s) for +: 'int' and 'str'");
}

static json_t *ws1_merge_aux_into_rows(json_t *raw_rows, const json_t *aux_rows)
{
    if (!ws1_is_list(aux_rows)) return ws1_not_iterable(aux_rows);
    for (size_t ai = 0; ai < aux_rows->c.count; ai++) {
        const json_t *aux = aux_rows->c.items[ai];
        char *amodel = ws1_str_or_empty(ws1_get0(aux, "model"));
        if (!amodel[0]) { free(amodel); amodel = strdup("unknown"); }
        char *aprov = ws1_str_or_empty(ws1_get0(aux, "billing_provider"));
        json_t *target = NULL;
        for (size_t ri = 0; ri < raw_rows->c.count; ri++) {
            json_t *row = raw_rows->c.items[ri];
            char *rm = ws1_str_or_empty(ws1_get0(row, "model"));
            char *rp = ws1_str_or_empty(ws1_get0(row, "billing_provider"));
            int match = strcmp(rm, amodel) == 0 && strcmp(rp, aprov) == 0;
            free(rm); free(rp);
            if (match) target = row;   /* dict index: LAST duplicate wins */
        }
        if (!target) {
            target = json_object();
            json_set(target, "model", json_string(amodel));
            json_set(target, "billing_provider", json_string(aprov));
            for (size_t k = 0; k < sizeof WS1_AUX_SUMMED_KEYS / sizeof WS1_AUX_SUMMED_KEYS[0]; k++)
                json_set(target, WS1_AUX_SUMMED_KEYS[k], json_number(0));
            json_set(target, "actual_cost", json_number(0));
            double sess = 0;
            json_t *se = ws1_num_or_zero(ws1_get0(aux, "sessions"), &sess);
            if (se) { free(amodel); free(aprov); json_free(target); return se; }
            json_set(target, "sessions", json_number(sess));
            json_set(target, "tool_calls", json_number(0));
            json_set(target, "last_used_at", json_null());
            json_set(target, "avg_tokens_per_session", json_number(0));
            json_append(raw_rows, target);
        }
        free(amodel); free(aprov);
        for (size_t k = 0; k < sizeof WS1_AUX_SUMMED_KEYS / sizeof WS1_AUX_SUMMED_KEYS[0]; k++) {
            const char *key = WS1_AUX_SUMMED_KEYS[k];
            double tv = 0, av = 0;
            json_t *e1 = ws1_num_or_zero(ws1_get0(target, key), &tv);
            if (e1) return e1;
            json_t *e2 = ws1_num_or_zero(ws1_get0(aux, key), &av);
            if (e2) return e2;
            json_set(target, key, json_number(tv + av));
        }
        json_t *alu = ws1_get0(aux, "last_used_at");
        if (alu && alu->type != JSON_NULL) {
            double tv = 0;
            json_t *e1 = ws1_num_or_zero(ws1_get0(target, "last_used_at"), &tv);
            if (e1) return e1;
            double av = alu->type == JSON_NUMBER ? alu->num_val : 0.0;
            json_set(target, "last_used_at", json_number(tv > av ? tv : av));
        }
        double sess = 0;
        json_t *se = ws1_num_or_zero(ws1_get0(target, "sessions"), &sess);
        if (se) return se;
        if (sess != 0) {
            double in = 0, outv = 0;
            json_t *e1 = ws1_num_or_zero(ws1_get0(target, "input_tokens"), &in);
            if (e1) return e1;
            json_t *e2 = ws1_num_or_zero(ws1_get0(target, "output_tokens"), &outv);
            if (e2) return e2;
            json_set(target, "avg_tokens_per_session", json_number((in + outv) / sess));
        }
    }
    return NULL;
}

static json_t *ws1_fold_session_only_rows(json_t *raw_rows, json_t **result)
{
    /* group by (model or "") preserving first-appearance order */
    json_t *rows = json_array();
    size_t n = raw_rows->c.count;
    int *used = calloc(n ? n : 1, sizeof(int));
    for (size_t i = 0; i < n; i++) {
        if (used[i]) continue;
        json_t *row = raw_rows->c.items[i];
        char *model = ws1_str_or_empty(ws1_get0(row, "model"));
        /* collect the group */
        size_t *group = malloc(n * sizeof(size_t));
        size_t gcount = 0;
        for (size_t j = i; j < n; j++) {
            if (used[j]) continue;
            char *jm = ws1_str_or_empty(ws1_get0(raw_rows->c.items[j], "model"));
            if (strcmp(jm, model) == 0) { group[gcount++] = j; used[j] = 1; }
            free(jm);
        }
        free(model);
        /* provider_rows = [r for r in group if r.billing_provider] */
        size_t prov_count = 0;
        size_t target_idx = 0;
        for (size_t g = 0; g < gcount; g++) {
            json_t *r = raw_rows->c.items[group[g]];
            if (ws1_jtruth(ws1_get0(r, "billing_provider"))) {
                prov_count++;
                target_idx = group[g];
            }
        }
        if (prov_count != 1) {
            for (size_t g = 0; g < gcount; g++)
                json_append(rows, json_copy(raw_rows->c.items[group[g]]));
            free(group);
            continue;
        }
        json_t *target = raw_rows->c.items[target_idx];
        for (size_t g = 0; g < gcount; g++) {
            json_t *r = raw_rows->c.items[group[g]];
            if (r == target || ws1_jtruth(ws1_get0(r, "billing_provider")) || ws1_has_usage(r))
                continue;
            double ts = 0, rs = 0;
            json_t *e1 = ws1_num_or_zero(ws1_get0(target, "sessions"), &ts);
            if (e1) { free(group); free(used); *result = rows; return e1; }
            json_t *e2 = ws1_num_or_zero(ws1_get0(r, "sessions"), &rs);
            if (e2) { free(group); free(used); *result = rows; return e2; }
            json_set(target, "sessions", json_number(ts + rs));
            double tl = 0, rl = 0;
            e1 = ws1_num_or_zero(ws1_get0(target, "last_used_at"), &tl);
            if (e1) { free(group); free(used); *result = rows; return e1; }
            e2 = ws1_num_or_zero(ws1_get0(r, "last_used_at"), &rl);
            if (e2) { free(group); free(used); *result = rows; return e2; }
            json_set(target, "last_used_at", json_number(tl > rl ? tl : rl));
            double in = 0, outv = 0;
            e1 = ws1_num_or_zero(ws1_get0(target, "input_tokens"), &in);
            if (e1) { free(group); free(used); *result = rows; return e1; }
            e2 = ws1_num_or_zero(ws1_get0(target, "output_tokens"), &outv);
            if (e2) { free(group); free(used); *result = rows; return e2; }
            double sessions = ts + rs;
            json_set(target, "avg_tokens_per_session",
                     json_number(sessions != 0 ? (in + outv) / sessions : 0.0));
        }
        json_append(rows, json_copy(target));
        for (size_t g = 0; g < gcount; g++) {
            json_t *r = raw_rows->c.items[group[g]];
            if (r != target && (ws1_jtruth(ws1_get0(r, "billing_provider")) || ws1_has_usage(r)))
                json_append(rows, json_copy(r));
        }
        free(group);
    }
    free(used);
    *result = rows;
    return NULL;
}

/* PoP: ws_u_get_models_analytics @ hermes_cli/web_routers/analytics.py:_get_models_analytics */
json_t *ws_u_get_models_analytics(json_t *req)
{
    json_t *days_v = ws1_get0(req, "days");
    double days = 30;
    if (days_v) {
        if (days_v->type != JSON_NUMBER)
            return ws1_raise("TypeError", "unsupported operand type(s) for *: 'str' and 'int'");
        days = days_v->num_val;
    }
    json_t *profile = ws1_get0(req, "profile");
    json_t *out = NULL;
    json_t *persistence = NULL;
    json_t *raw_rows = NULL;
    json_t *rows = NULL;
    json_t *models = NULL;
    json_t *totals = NULL;

    persistence = ws1_call("open_session_db",
                           ws1_args2(profile ? json_copy(profile) : json_null(), json_bool(true)));
    if (ws1_is_raise(persistence)) return persistence;

    json_t *tt = ws1_call("time_time", ws1_args0());
    if (ws1_is_raise(tt)) { out = tt; goto close; }
    if (tt->type != JSON_NUMBER) {
        out = ws1_raise("TypeError", "unsupported operand type(s) for -: 'str' and 'float'");
        json_free(tt);
        goto close;
    }
    double cutoff = tt->num_val - days * 86400.0;
    json_free(tt);

    raw_rows = ws1_call("call_usage_by_route", ws1_args1(json_number(cutoff)));
    if (ws1_is_raise(raw_rows)) {
        const char *kind = json_get_str(raw_rows, "__raise__", "");
        if (ws1_is_base_exc(raw_rows)) { out = raw_rows; raw_rows = NULL; goto close; }
        if (strcmp(kind, "TransientStoreError") == 0) {
            json_free(raw_rows);
            raw_rows = json_array();
        } else {
            out = raw_rows;
            raw_rows = NULL;
            goto close;
        }
    }
    if (!ws1_jtruth(raw_rows)) {
        json_free(raw_rows);
        raw_rows = ws1_call("session_usage_by_route", ws1_args1(json_number(cutoff)));
        if (ws1_is_raise(raw_rows)) { out = raw_rows; raw_rows = NULL; goto close; }
    } else {
        json_t *att = ws1_call("attach_tool_calls",
                               ws1_args3(json_copy(persistence), json_number(cutoff),
                                         json_copy(raw_rows)));
        if (ws1_is_raise(att)) { out = att; goto close; }
        json_free(att);
    }
    if (!ws1_is_list(raw_rows)) {
        out = ws1_not_iterable(raw_rows);
        goto close;
    }

    json_t *aux_rows = ws1_call("aux_usage_rows",
                                ws1_args2(json_copy(persistence), json_number(cutoff)));
    if (ws1_is_raise(aux_rows)) { out = aux_rows; goto close; }
    json_t *merge_err = ws1_merge_aux_into_rows(raw_rows, aux_rows);
    json_free(aux_rows);
    if (merge_err) { out = merge_err; goto close; }

    json_t *fold_err = ws1_fold_session_only_rows(raw_rows, &rows);
    if (fold_err) { out = fold_err; goto close; }

    /* rows.sort(key=input+output, reverse=True) — stable */
    {
        size_t rn = rows->c.count;
        double *keys = malloc((rn ? rn : 1) * sizeof(double));
        for (size_t i = 0; i < rn; i++) {
            double in = 0, outv = 0;
            json_t *e1 = ws1_num_or_zero(ws1_get0(rows->c.items[i], "input_tokens"), &in);
            if (!e1) e1 = ws1_num_or_zero(ws1_get0(rows->c.items[i], "output_tokens"), &outv);
            if (e1) { out = e1; free(keys); goto close; }
            keys[i] = in + outv;
        }
        for (size_t i = 1; i < rn; i++) {
            json_t *row = rows->c.items[i];
            double key = keys[i];
            size_t j = i;
            while (j > 0 && keys[j - 1] < key) {
                rows->c.items[j] = rows->c.items[j - 1];
                keys[j] = keys[j - 1];
                j--;
            }
            rows->c.items[j] = row;
            keys[j] = key;
        }
        free(keys);
    }

    models = json_array();
    for (size_t i = 0; i < rows->c.count && !out; i++) {
        json_t *row = rows->c.items[i];
        json_t *err = NULL;
        json_t *model_v = ws1_item(row, "model", &err);
        if (err) { out = err; break; }
        char *prov = ws1_str_or_empty(ws1_get0(row, "billing_provider"));
        json_t *card = json_object();
        json_set(card, "model", json_copy(model_v));
        json_set(card, "provider", json_string(prov));
        for (size_t k = 0; k < sizeof WS1_MODEL_CARD_KEYS / sizeof WS1_MODEL_CARD_KEYS[0]; k++) {
            json_t *v = ws1_item(row, WS1_MODEL_CARD_KEYS[k], &err);
            if (err) break;
            json_set(card, WS1_MODEL_CARD_KEYS[k], json_copy(v));
        }
        if (err) { out = err; free(prov); json_free(card); break; }
        json_t *caps = ws1_call("model_capabilities",
                                ws1_args2(json_string(prov), json_copy(model_v)));
        free(prov);
        if (ws1_is_raise(caps)) { out = caps; json_free(card); break; }
        json_set(card, "capabilities", caps);
        json_append(models, card);
    }
    if (out) goto close;

    totals = ws1_call("session_usage_totals",
                      ws1_args2(json_number(cutoff), json_bool(true)));
    if (ws1_is_raise(totals)) { out = totals; totals = NULL; goto close; }
    if (!ws1_is_dict(totals)) {
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "'%s' object does not support item assignment", ws1_type_name(totals));
        char *m = ws1_sb_take(&b);
        out = ws1_raise("TypeError", m);
        free(m);
        goto close;
    }
    {
        /* len({row["model"] for row in rows}) */
        json_t *distinct = json_array();
        for (size_t i = 0; i < rows->c.count && !out; i++) {
            json_t *mv = ws1_get0(rows->c.items[i], "model");
            if (ws1_is_list(mv) || ws1_is_dict(mv)) { out = ws1_unhashable(mv); break; }
            int dup = 0;
            for (size_t j = 0; j < distinct->c.count; j++)
                if (ws1_eq(mv, distinct->c.items[j])) { dup = 1; break; }
            if (!dup) json_append(distinct, json_copy(mv));
        }
        if (!out) json_set(totals, "distinct_models", json_number((double)distinct->c.count));
        json_free(distinct);
    }
    if (out) goto close;

    out = json_object();
    json_set(out, "models", models);
    models = NULL;
    json_set(out, "totals", totals);
    totals = NULL;
    json_set(out, "period_days", json_number(days));
close:
    {
        json_t *closed = ws1_call("persistence_close", ws1_args1(json_copy(persistence)));
        if (ws1_is_raise(closed)) {
            json_free(out);
            out = closed;
        } else {
            json_free(closed);
        }
    }
    json_free(persistence);
    json_free(raw_rows);
    json_free(rows);
    json_free(models);
    json_free(totals);
    return out;
}

/* ================================================================
 * 14. _telegram_onboarding_request_sync
 * ================================================================ */

static const char *ws1_telegram_error_message(const char *error)
{
    if (strcmp(error, "not_found") == 0)
        return "Telegram pairing was not found. Start a new setup.";
    if (strcmp(error, "expired") == 0)
        return "Telegram setup expired. Start a new setup.";
    if (strcmp(error, "claimed") == 0)
        return "Telegram setup was already claimed. Start a new setup.";
    if (strcmp(error, "unauthorized") == 0)
        return "Telegram setup service rejected this request.";
    if (strcmp(error, "telegram_manager_bot_token_not_configured") == 0)
        return "Telegram setup service is not configured.";
    if (strcmp(error, "telegram_token_fetch_failed") == 0)
        return "Telegram could not finish bot setup. Try again.";
    return "Telegram setup service returned an error.";
}

static json_t *ws1_http_exception(int code, const char *detail)
{
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_printf(&b, "%d: %s", code, detail);
    char *m = ws1_sb_take(&b);
    json_t *r = ws1_raise("HTTPException", m);
    free(m);
    return r;
}

/* PoP: ws_u_telegram_onboarding_request_sync @ hermes_cli/web_server_messaging.py:_telegram_onboarding_request_sync */
json_t *ws_u_telegram_onboarding_request_sync(json_t *req)
{
    const char *method = json_get_str(req, "method", "");
    const char *path = json_get_str(req, "path", "");
    json_t *body = ws1_get0(req, "body");
    json_t *bearer = ws1_get0(req, "bearer_token");
    json_t *out = NULL;

    json_t *headers = json_object();
    json_set(headers, "Accept", json_string("application/json"));
    json_t *ua = ws1_call("hermes_user_agent", ws1_args1(json_string("HermesDashboard")));
    if (ws1_is_raise(ua)) { json_free(headers); return ua; }
    json_set(headers, "User-Agent", ua);
    int has_body = body && body->type != JSON_NULL;
    if (has_body) json_set(headers, "Content-Type", json_string("application/json"));
    if (ws1_jtruth(bearer)) {
        char *bs = ws1_pystr(bearer);
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "Bearer %s", bs ? bs : "");
        free(bs);
        char *s = ws1_sb_take(&b);
        json_set(headers, "Authorization", json_string(s));
        free(s);
    }

    /* try: the whole HTTP exchange */
    json_t *base = ws1_call("telegram_onboarding_base_url", ws1_args0());
    if (ws1_is_raise(base)) {
        if (ws1_is_base_exc(base)) { out = base; goto done; }
        json_free(base);
        out = ws1_http_exception(502, WS1_TELEGRAM_UNAVAILABLE);
        goto done;
    }
    char *base_s = ws1_pystr(base);
    json_free(base);
    if (!base_s) base_s = strdup("");
    ws1_sb ub; ws1_sb_init(&ub);
    ws1_sb_puts(&ub, base_s);
    ws1_sb_puts(&ub, path);
    free(base_s);
    char *url = ws1_sb_take(&ub);

    json_t *spec = ws1_call("httpx_request",
                            ws1_args4(method, url, json_copy(headers),
                                      has_body ? json_copy(body) : json_null()));
    free(url);
    if (ws1_is_raise(spec)) {
        if (ws1_is_base_exc(spec)) { out = spec; goto done; }
        json_free(spec);
        out = ws1_http_exception(502, WS1_TELEGRAM_UNAVAILABLE);
        goto done;
    }

    /* response double: {"status_code": N, "json": value} / "json_error" */
    double status = 200;
    json_t *status_v = ws1_get0(spec, "status_code");
    if (status_v) {
        if (status_v->type != JSON_NUMBER) {
            json_free(spec);
            out = ws1_http_exception(502, WS1_TELEGRAM_UNAVAILABLE);
            goto done;
        }
        status = status_v->num_val;
    }
    int has_json = ws1_is_dict(spec) && json_obj_get(spec, "json") != NULL;
    json_t *json_error = ws1_get0(spec, "json_error");

    if (status >= 400 && status < 600) {
        /* raise_for_status -> HTTPStatusError branch */
        json_t *parsed = NULL;
        if (json_error) {
            parsed = json_object();   /* except Exception: parsed = {} */
        } else if (has_json) {
            parsed = json_copy(json_obj_get(spec, "json"));
        } else {
            parsed = json_object();   /* json() ValueError -> {} */
        }
        if (!ws1_is_dict(parsed)) {
            out = ws1_attr_error(parsed, "get");
            json_free(parsed);
            json_free(spec);
            goto done;
        }
        json_t *err_v = ws1_get0(parsed, "error");
        if (!ws1_jtruth(err_v)) err_v = ws1_get0(parsed, "status");
        char *error = ws1_str_or_empty(err_v);
        const char *detail = ws1_telegram_error_message(error);
        int code;
        if (strcmp(error, "expired") == 0 || strcmp(error, "claimed") == 0) code = 410;
        else code = (status == 404) ? 404 : 502;
        free(error);
        json_free(parsed);
        json_free(spec);
        out = ws1_http_exception(code, detail);
        goto done;
    }

    /* success path: response.json() */
    if (json_error || !has_json) {
        json_free(spec);
        out = ws1_http_exception(502, WS1_TELEGRAM_INVALID);
        goto done;
    }
    json_t *parsed = json_copy(json_obj_get(spec, "json"));
    json_free(spec);
    if (!ws1_is_dict(parsed)) {
        json_free(parsed);
        out = ws1_http_exception(502, WS1_TELEGRAM_INVALID);
        goto done;
    }
    out = parsed;
done:
    json_free(headers);
    return out;
}

/* ================================================================
 * 15. get_recommended_default_model
 * ================================================================ */

/* PoP: ws_get_recommended_default_model @ hermes_cli/web_routers/models.py:get_recommended_default_model */
json_t *ws_get_recommended_default_model(json_t *req)
{
    json_t *provider = ws1_get0(req, "provider");
    json_t *profile = ws1_get0(req, "profile");
    char *p0 = ws1_str_or_empty(provider);
    if (ws1_jtruth(provider) && !ws1_is_str(provider)) {
        free(p0);
        return ws1_attr_error(provider, "strip");
    }
    char *ps = ws1_strip(p0);
    free(p0);
    char *slug = ws1_lower(ps);
    free(ps);
    json_t *out = NULL;
    int nous = strcmp(slug, "nous") == 0;

    /* scope enter (inside try) */
    json_t *enter = ws1_call("profile_scope_enter",
                             ws1_args1(profile ? json_copy(profile) : json_null()));
    if (ws1_is_raise(enter)) {
        const char *kind = json_get_str(enter, "__raise__", "");
        if (strcmp(kind, "HTTPException") == 0 || ws1_is_base_exc(enter)) {
            out = enter;
            goto done;
        }
        json_free(enter);
        goto log_fallback;
    }
    json_free(enter);

    if (nous) {
        json_t *rec = ws1_call("nous_recommended_default", ws1_args0());
        json_t *exit_r = ws1_call("profile_scope_exit",
                                  ws1_args1(profile ? json_copy(profile) : json_null()));
        if (ws1_is_raise(exit_r)) {
            json_free(rec);
            rec = exit_r;
        } else {
            json_free(exit_r);
        }
        if (ws1_is_raise(rec)) {
            const char *kind = json_get_str(rec, "__raise__", "");
            if (strcmp(kind, "HTTPException") == 0 || ws1_is_base_exc(rec)) {
                out = rec;
                goto done;
            }
            json_free(rec);
            goto log_fallback;
        }
        out = rec;
        goto done;
    }

    json_t *picker = ws1_call("load_picker_context", ws1_args0());
    json_t *payload = NULL;
    if (!ws1_is_raise(picker)) {
        payload = ws1_call("build_models_payload", ws1_args1(picker));
    } else {
        payload = picker;
        picker = NULL;
    }
    json_t *exit_r = ws1_call("profile_scope_exit",
                              ws1_args1(profile ? json_copy(profile) : json_null()));
    if (ws1_is_raise(exit_r)) {
        json_free(payload);
        payload = exit_r;
    } else {
        json_free(exit_r);
    }
    if (ws1_is_raise(payload)) {
        const char *kind = json_get_str(payload, "__raise__", "");
        if (strcmp(kind, "HTTPException") == 0 || ws1_is_base_exc(payload)) {
            out = payload;
            goto done;
        }
        json_free(payload);
        goto log_fallback;
    }
    if (!ws1_is_dict(payload)) {
        json_free(payload);
        goto log_fallback;   /* AttributeError .get -> except Exception */
    }
    json_t *prov_rows = ws1_get0(payload, "providers");
    if (!prov_rows) prov_rows = json_array();   /* default [] — owned below */
    int prov_rows_owned = !ws1_get0(payload, "providers");
    if (!ws1_is_list(prov_rows)) {
        if (prov_rows_owned) json_free(prov_rows);
        json_free(payload);
        goto log_fallback;
    }
    for (size_t i = 0; i < prov_rows->c.count; i++) {
        json_t *row = prov_rows->c.items[i];
        if (!ws1_is_dict(row)) {
            if (prov_rows_owned) json_free(prov_rows);
            json_free(payload);
            goto log_fallback;
        }
        char *rs = ws1_str_or_empty(ws1_get0(row, "slug"));
        char *rlo = ws1_lower(rs);
        free(rs);
        int match = strcmp(rlo, slug) == 0;
        free(rlo);
        if (!match) continue;
        json_t *models_raw = ws1_get0(row, "models");
        if (!ws1_jtruth(models_raw)) models_raw = NULL;
        if (models_raw && !ws1_is_list(models_raw)) {
            if (prov_rows_owned) json_free(prov_rows);
            json_free(payload);
            goto log_fallback;
        }
        json_t *models = json_array();
        if (models_raw) {
            for (size_t j = 0; j < models_raw->c.count; j++) {
                char *ms = ws1_pystr(models_raw->c.items[j]);
                json_append(models, json_string(ms ? ms : ""));
                free(ms);
            }
        }
        json_t *picked = ws1_call("pick_silent_default_model",
                                  ws1_args2(models, json_string(slug)));
        if (prov_rows_owned) json_free(prov_rows);
        json_free(payload);
        if (ws1_is_raise(picked)) {
            const char *kind = json_get_str(picked, "__raise__", "");
            if (strcmp(kind, "HTTPException") == 0 || ws1_is_base_exc(picked)) {
                out = picked;
                goto done;
            }
            json_free(picked);
            goto log_fallback;
        }
        out = json_object();
        json_set(out, "provider", json_string(slug));
        json_set(out, "model", picked);
        json_set(out, "free_tier", json_null());
        goto done;
    }
    if (prov_rows_owned) json_free(prov_rows);
    json_free(payload);
    out = json_object();
    json_set(out, "provider", json_string(slug));
    json_set(out, "model", json_string(""));
    json_set(out, "free_tier", json_null());
    goto done;

log_fallback:
    {
        const char *msg = nous
            ? "GET /api/model/recommended-default (nous) failed"
            : "GET /api/model/recommended-default failed";
        json_t *le = ws1_call("log_exception", ws1_args1(json_string(msg)));
        if (ws1_is_raise(le)) { out = le; goto done; }
        json_free(le);
        out = json_object();
        json_set(out, "provider", json_string(slug));
        json_set(out, "model", json_string(""));
        json_set(out, "free_tier", json_null());
    }
done:
    free(slug);
    return out;
}

/* ================================================================
 * 16. _discover_dashboard_plugins (+ _dashboard_plugin_entry)
 * ================================================================ */

/* Python repr() of a string (ASCII fixture domain) */
static char *ws1_pyrepr_str(const char *s)
{
    int has_sq = strchr(s, '\'') != NULL;
    int has_dq = strchr(s, '"') != NULL;
    char q = (has_sq && !has_dq) ? '"' : '\'';
    ws1_sb b; ws1_sb_init(&b);
    ws1_sb_putc(&b, q);
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == (unsigned char)q || c == '\\') { ws1_sb_putc(&b, '\\'); ws1_sb_putc(&b, (char)c); }
        else if (c == '\n') ws1_sb_puts(&b, "\\n");
        else if (c == '\r') ws1_sb_puts(&b, "\\r");
        else if (c == '\t') ws1_sb_puts(&b, "\\t");
        else ws1_sb_putc(&b, (char)c);
    }
    ws1_sb_putc(&b, q);
    return ws1_sb_take(&b);
}

static json_t *ws1_dashboard_plugin_entry(const json_t *data, const json_t *name,
                                          const char *dashboard_dir, const json_t *source,
                                          json_t **err)
{
    *err = NULL;
    json_t *raw_tab = ws1_is_dict(ws1_get0(data, "tab")) ? ws1_get0(data, "tab") : NULL;
    json_t *tab_info = json_object();
    json_t *path = raw_tab ? ws1_get0(raw_tab, "path") : NULL;
    if (!path) {
        char *ns = ws1_pystr(name);
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b, "/%s", ns ? ns : "");
        free(ns);
        char *s = ws1_sb_take(&b);
        path = json_string(s);
        free(s);
        json_set(tab_info, "path", path);
    } else {
        json_set(tab_info, "path", json_copy(path));
    }
    json_t *pos = raw_tab ? ws1_get0(raw_tab, "position") : NULL;
    json_set(tab_info, "position", pos ? json_copy(pos) : json_string("end"));
    json_t *override = raw_tab ? ws1_get0(raw_tab, "override") : NULL;
    if (ws1_is_str(override) && override->str_val[0] == '/')
        json_set(tab_info, "override", json_copy(override));
    if (raw_tab && ws1_jtruth(ws1_get0(raw_tab, "hidden")))
        json_set(tab_info, "hidden", json_bool(true));

    json_t *slots = json_array();
    json_t *slots_src = ws1_get0(data, "slots");
    if (ws1_is_list(slots_src)) {
        for (size_t i = 0; i < slots_src->c.count; i++) {
            json_t *s = slots_src->c.items[i];
            if (ws1_is_str(s) && s->str_val[0]) json_append(slots, json_copy(s));
        }
    }

    json_t *raw_api = ws1_get0(data, "api");
    json_t *safe_api = ws1_call("safe_plugin_api_relpath",
                                ws1_args2(raw_api ? json_copy(raw_api) : json_null(),
                                          json_string(dashboard_dir)));
    if (ws1_is_raise(safe_api)) { *err = safe_api; json_free(tab_info); json_free(slots); return NULL; }
    if (ws1_jtruth(raw_api) && (!safe_api || safe_api->type == JSON_NULL)) {
        char *ns = ws1_pystr(name);
        char *as = ws1_pystr(raw_api);
        char *repr = ws1_pyrepr_str(as ? as : "");
        free(as);
        ws1_sb b; ws1_sb_init(&b);
        ws1_sb_printf(&b,
            "Plugin %s: refusing unsafe api path %s (must be a "
            "relative file inside the plugin's dashboard/ "
            "directory); backend routes from this plugin will "
            "not be mounted", ns ? ns : "", repr);
        free(repr); free(ns);
        char *m = ws1_sb_take(&b);
        json_t *w = ws1_call("log_warning", ws1_args1(json_string(m)));
        free(m);
        if (ws1_is_raise(w)) {
            *err = w;
            json_free(safe_api); json_free(tab_info); json_free(slots);
            return NULL;
        }
        json_free(w);
    }

    json_t *entry = json_object();
    json_set(entry, "name", json_copy(name));
    json_t *label = ws1_get0(data, "label");
    json_set(entry, "label", label ? json_copy(label) : json_copy(name));
    json_t *desc = ws1_get0(data, "description");
    json_set(entry, "description", desc ? json_copy(desc) : json_string(""));
    json_t *icon = ws1_get0(data, "icon");
    json_set(entry, "icon", icon ? json_copy(icon) : json_string("Puzzle"));
    json_t *ver = ws1_get0(data, "version");
    json_set(entry, "version", ver ? json_copy(ver) : json_string("0.0.0"));
    json_set(entry, "tab", tab_info);
    json_set(entry, "slots", slots);
    json_t *entry_js = ws1_get0(data, "entry");
    json_set(entry, "entry", entry_js ? json_copy(entry_js) : json_string("dist/index.js"));
    json_t *css = ws1_get0(data, "css");
    json_set(entry, "css", css ? json_copy(css) : json_null());
    json_set(entry, "has_api", json_bool(ws1_jtruth(safe_api)));
    json_set(entry, "source", source ? json_copy(source) : json_null());
    json_set(entry, "_dir", json_string(dashboard_dir));
    json_set(entry, "_api_file", safe_api ? safe_api : json_null());
    return entry;
}

/* PoP: ws_u_discover_dashboard_plugins @ hermes_cli/web_server_dashboard.py:_discover_dashboard_plugins */
json_t *ws_u_discover_dashboard_plugins(json_t *req)
{
    json_t *search_dirs = ws1_get0(req, "search_dirs");
    json_t *roots = NULL;
    int roots_owned = 0;
    json_t *plugins = json_array();
    json_t *seen = json_array();
    json_t *out = NULL;

    if (!search_dirs || search_dirs->type == JSON_NULL) {
        roots = ws1_call("dashboard_plugin_search_dirs", ws1_args0());
        roots_owned = 1;
        if (ws1_is_raise(roots)) { out = roots; roots = NULL; goto done; }
    } else {
        roots = search_dirs;
    }
    if (!ws1_is_list(roots)) {
        out = ws1_not_iterable(roots);
        goto done;
    }

    for (size_t ri = 0; ri < roots->c.count && !out; ri++) {
        json_t *pair = roots->c.items[ri];
        if (!ws1_is_list(pair) || pair->c.count != 2) {
            ws1_sb b; ws1_sb_init(&b);
            if (ws1_is_list(pair) && pair->c.count > 2)
                ws1_sb_puts(&b, "too many values to unpack (expected 2)");
            else if (ws1_is_list(pair))
                ws1_sb_printf(&b, "not enough values to unpack (expected 2, got %zu)", pair->c.count);
            else
                ws1_sb_printf(&b, "cannot unpack non-iterable %s object", ws1_type_name(pair));
            char *m = ws1_sb_take(&b);
            out = ws1_raise(ws1_is_list(pair) ? "ValueError" : "TypeError", m);
            free(m);
            break;
        }
        char *root = ws1_pystr(pair->c.items[0]);
        if (!root) root = strdup("");
        json_t *source = pair->c.items[1];

        /* outer try: root is_dir + scandir; except OSError only */
        json_t *isdir = ws1_call("path_is_dir", ws1_args1(json_string(root)));
        if (ws1_is_raise(isdir)) {
            const char *kind = json_get_str(isdir, "__raise__", "");
            if (strcmp(kind, "OSError") == 0) {
                const char *msg = json_get_str(isdir, "msg", "");
                ws1_sb b; ws1_sb_init(&b);
                ws1_sb_printf(&b, "Skipping unreadable dashboard plugin root %s: %s", root, msg);
                char *m = ws1_sb_take(&b);
                json_t *w = ws1_call("log_warning", ws1_args1(json_string(m)));
                free(m);
                json_free(isdir);
                if (ws1_is_raise(w)) { out = w; free(root); break; }
                json_free(w);
                free(root);
                continue;
            }
            out = isdir;
            free(root);
            break;
        }
        int root_is_dir = ws1_jtruth(isdir);
        json_free(isdir);
        if (!root_is_dir) { free(root); continue; }

        json_t *entries = ws1_call("scandir", ws1_args1(json_string(root)));
        if (ws1_is_raise(entries)) {
            const char *kind = json_get_str(entries, "__raise__", "");
            if (strcmp(kind, "OSError") == 0) {
                const char *msg = json_get_str(entries, "msg", "");
                ws1_sb b; ws1_sb_init(&b);
                ws1_sb_printf(&b, "Skipping unreadable dashboard plugin root %s: %s", root, msg);
                char *m = ws1_sb_take(&b);
                json_t *w = ws1_call("log_warning", ws1_args1(json_string(m)));
                free(m);
                json_free(entries);
                if (ws1_is_raise(w)) { out = w; free(root); break; }
                json_free(w);
                free(root);
                continue;
            }
            out = entries;
            free(root);
            break;
        }
        free(root);
        if (!ws1_is_list(entries)) {
            out = ws1_not_iterable(entries);
            json_free(entries);
            break;
        }
        /* children sorted by basename (stable) */
        size_t en = entries->c.count;
        char **paths = malloc((en ? en : 1) * sizeof(char *));
        char **names = malloc((en ? en : 1) * sizeof(char *));
        for (size_t i = 0; i < en; i++) {
            paths[i] = ws1_pystr(entries->c.items[i]);
            if (!paths[i]) paths[i] = strdup("");
            names[i] = ws1_path_name(paths[i]);
        }
        for (size_t i = 1; i < en; i++) {
            char *p = paths[i], *nm = names[i];
            size_t j = i;
            while (j > 0 && strcmp(names[j - 1], nm) > 0) {
                paths[j] = paths[j - 1]; names[j] = names[j - 1]; j--;
            }
            paths[j] = p; names[j] = nm;
        }
        json_free(entries);

        for (size_t ci = 0; ci < en && !out; ci++) {
            char *child = paths[ci];
            char *dash_dir = ws1_path_join(child, "dashboard");
            char *manifest = ws1_path_join(dash_dir, "manifest.json");
            /* inner try: except OSError -> skipping; except Exception -> bad */
            json_t *cerr = NULL;
            int skipping = 0;   /* 1 = OSError wording, 2 = bad-manifest wording */
            json_t *entry = NULL;

            json_t *cd = ws1_call("path_is_dir", ws1_args1(json_string(child)));
            if (ws1_is_raise(cd)) { cerr = cd; goto inner_exc; }
            if (!ws1_jtruth(cd)) { json_free(cd); goto inner_done; }
            json_free(cd);
            json_t *ex = ws1_call("path_exists", ws1_args1(json_string(manifest)));
            if (ws1_is_raise(ex)) { cerr = ex; goto inner_exc; }
            if (!ws1_jtruth(ex)) { json_free(ex); goto inner_done; }
            json_free(ex);
            json_t *text = ws1_call("read_text",
                                    ws1_args2(json_string(manifest), json_string("utf-8-sig")));
            if (ws1_is_raise(text)) { cerr = text; goto inner_exc; }
            char *ts = ws1_pystr(text);
            json_free(text);
            if (!ts) ts = strdup("");
            /* utf-8-sig decoding strips a leading BOM */
            char *body_text = ts;
            if (strncmp(ts, "\xEF\xBB\xBF", 3) == 0) body_text = ts + 3;
            json_t *data = ws1_call("json_loads", ws1_args1(json_string(body_text)));
            free(ts);
            if (ws1_is_raise(data)) { cerr = data; goto inner_exc; }
            if (!ws1_is_dict(data)) {
                cerr = ws1_attr_error(data, "get");
                json_free(data);
                goto inner_exc;
            }
            {
                json_t *name = ws1_get0(data, "name");
                if (!name) name = json_string(names[ci]);
                int name_owned = !ws1_get0(data, "name");
                if (ws1_is_list(name) || ws1_is_dict(name)) {
                    cerr = ws1_unhashable(name);
                    if (name_owned) json_free(name);
                    json_free(data);
                    goto inner_exc;
                }
                int dup = 0;
                for (size_t si = 0; si < seen->c.count; si++)
                    if (ws1_eq(name, seen->c.items[si])) { dup = 1; break; }
                if (dup) {
                    if (name_owned) json_free(name);
                    json_free(data);
                    goto inner_done;
                }
                json_append(seen, json_copy(name));
                json_t *eerr = NULL;
                entry = ws1_dashboard_plugin_entry(data, name, dash_dir, source, &eerr);
                if (name_owned) json_free(name);
                json_free(data);
                if (eerr) { cerr = eerr; goto inner_exc; }
            }
            goto inner_done;

inner_exc:
            {
                const char *kind = cerr ? json_get_str(cerr, "__raise__", "") : "";
                if (ws1_is_base_exc(cerr)) { out = cerr; cerr = NULL; goto inner_done; }
                skipping = strcmp(kind, "OSError") == 0 ? 1 : 2;
                const char *msg = cerr ? json_get_str(cerr, "msg", "") : "";
                ws1_sb b; ws1_sb_init(&b);
                if (skipping == 1)
                    ws1_sb_printf(&b, "Skipping unreadable dashboard plugin %s: %s", manifest, msg);
                else
                    ws1_sb_printf(&b, "Bad dashboard plugin manifest %s: %s", manifest, msg);
                char *m = ws1_sb_take(&b);
                json_t *w = ws1_call("log_warning", ws1_args1(json_string(m)));
                free(m);
                json_free(cerr);
                cerr = NULL;
                if (ws1_is_raise(w)) { out = w; goto inner_done; }
                json_free(w);
            }
inner_done:
            if (entry) json_append(plugins, entry);
            free(dash_dir);
            free(manifest);
            (void)skipping;
        }
        for (size_t i = 0; i < en; i++) { free(paths[i]); free(names[i]); }
        free(paths);
        free(names);
    }
    if (!out) out = plugins;
    else json_free(plugins);
done:
    if (roots_owned) json_free(roots);
    json_free(seen);
    return out;
}

/* ================================================================
 * 17. _catalog_provider_env_metadata (+ _provider_card)
 * ================================================================ */

static json_t *ws1_provider_card(const json_t *d, const json_t *description,
                                 const json_t *url, int is_password,
                                 const json_t *advanced, json_t **err)
{
    *err = NULL;
    json_t *slug = ws1_is_dict(d) ? json_obj_get(d, "slug") : NULL;
    if (!slug) { *err = ws1_attr_error(d, "slug"); return NULL; }
    json_t *label = json_obj_get(d, "label");
    if (!label) { *err = ws1_attr_error(d, "label"); return NULL; }
    json_t *card = json_object();
    json_set(card, "provider", json_copy(slug));
    json_set(card, "provider_label", json_copy(label));
    json_set(card, "description", description ? json_copy(description) : json_null());
    json_set(card, "url", url ? json_copy(url) : json_null());
    json_set(card, "is_password", json_bool(is_password));
    json_set(card, "advanced", advanced ? json_copy(advanced) : json_null());
    json_set(card, "category", json_string("provider"));
    return card;
}

static json_t *ws1_catalog_profile(const json_t *entry)
{
    json_t *p = json_object();
    json_set(p, "provider", json_copy(ws1_get0(entry, "provider")));
    json_set(p, "provider_label", json_copy(ws1_get0(entry, "provider_label")));
    json_set(p, "description", json_copy(ws1_get0(entry, "description")));
    json_set(p, "url", json_copy(ws1_get0(entry, "url")));
    json_set(p, "primary", json_bool(ws1_jtruth(ws1_get0(entry, "provider_primary"))));
    return p;
}

/* PoP: ws_u_catalog_provider_env_metadata @ hermes_cli/web_routers/config_env.py:_catalog_provider_env_metadata */
json_t *ws_u_catalog_provider_env_metadata(json_t *req)
{
    if (ws1_jtruth(ws1_get0(req, "catalog_import_fails"))) return json_object();
    json_t *oev = ws1_get0(req, "optional_env_vars");
    if (!ws1_is_dict(oev)) oev = NULL;
    json_t *meta = json_object();
    json_t *out = NULL;

    /* _non_provider_keys */
    json_t *non_provider = json_array();
    if (oev) {
        for (size_t i = 0; i < oev->c.count && !out; i++) {
            json_t *v = oev->c.items[i];
            json_t *vd = ws1_jtruth(v) ? v : NULL;
            if (vd && !ws1_is_dict(vd)) { out = ws1_attr_error(vd, "get"); break; }
            json_t *cat = vd ? ws1_get0(vd, "category") : NULL;
            if (ws1_jtruth(cat) && (!ws1_is_str(cat) || strcmp(cat->str_val, "provider") != 0))
                json_append(non_provider, json_string(oev->c.keys[i]));
        }
    }
    if (out) { json_free(non_provider); json_free(meta); return out; }

    json_t *cards = ws1_call("provider_catalog", ws1_args0());
    if (ws1_is_raise(cards)) { out = cards; goto done; }
    if (!ws1_is_list(cards)) {
        out = ws1_not_iterable(cards);
        json_free(cards);
        goto done;
    }

    for (size_t di = 0; di < cards->c.count && !out; di++) {
        json_t *d = cards->c.items[di];
        json_t *tab = ws1_is_dict(d) ? json_obj_get(d, "tab") : NULL;
        if (!tab) { out = ws1_attr_error(d, "tab"); break; }
        if (!ws1_is_str(tab) || strcmp(tab->str_val, "keys") != 0) continue;
        json_t *env_vars = json_obj_get(d, "api_key_env_vars");
        if (!env_vars) { out = ws1_attr_error(d, "api_key_env_vars"); break; }
        if (!ws1_is_list(env_vars)) { out = ws1_not_iterable(env_vars); break; }
        for (size_t ei = 0; ei < env_vars->c.count && !out; ei++) {
            json_t *env_var = env_vars->c.items[ei];
            if (ws1_is_list(env_var) || ws1_is_dict(env_var)) { out = ws1_unhashable(env_var); break; }
            int excluded = 0;
            for (size_t ni = 0; ni < non_provider->c.count; ni++)
                if (ws1_eq(env_var, non_provider->c.items[ni])) { excluded = 1; break; }
            if (excluded) continue;
            json_t *description = json_obj_get(d, "description");
            if (!description) { out = ws1_attr_error(d, "description"); break; }
            json_t *signup = json_obj_get(d, "signup_url");
            if (!signup) { out = ws1_attr_error(d, "signup_url"); break; }
            json_t *url = ws1_jtruth(signup) ? signup : NULL;
            json_t *cerr = NULL;
            json_t *adv_false = json_bool(false);
            json_t *entry = ws1_provider_card(d, description, url, 1, adv_false, &cerr);
            json_free(adv_false);
            if (cerr) { out = cerr; break; }
            json_set(entry, "provider_primary", json_bool(ei == 0));
            /* _add_provider_env */
            char *evs = ws1_pystr(env_var);
            if (!evs) evs = strdup("");
            json_t *existing = json_obj_get(meta, evs);
            if (!existing) {
                json_set(meta, evs, entry);
            } else {
                json_t *ep = ws1_get0(existing, "provider");
                json_t *np = ws1_get0(entry, "provider");
                if (ws1_eq(ep, np)) {
                    json_free(entry);
                } else {
                    json_t *profiles = ws1_get0(existing, "provider_profiles");
                    if (!profiles) {
                        profiles = json_array();
                        json_append(profiles, ws1_catalog_profile(existing));
                        json_set(existing, "provider_profiles", profiles);
                    }
                    int found = 0;
                    for (size_t pi = 0; pi < profiles->c.count; pi++)
                        if (ws1_eq(ws1_get0(profiles->c.items[pi], "provider"), np)) {
                            found = 1; break;
                        }
                    if (!found) json_append(profiles, ws1_catalog_profile(entry));
                    json_free(entry);
                }
            }
            free(evs);
        }
        if (out) break;
        json_t *burl = json_obj_get(d, "base_url_env_var");
        if (!burl) { out = ws1_attr_error(d, "base_url_env_var"); break; }
        if (ws1_jtruth(burl)) {
            json_t *label = json_obj_get(d, "label");
            if (!label) { out = ws1_attr_error(d, "label"); break; }
            char *ls = ws1_pystr(label);
            ws1_sb b; ws1_sb_init(&b);
            ws1_sb_printf(&b, "%s base URL override", ls ? ls : "");
            free(ls);
            char *desc = ws1_sb_take(&b);
            json_t *cerr = NULL;
            json_t *desc_node = json_string(desc);
            json_t *adv_true = json_bool(true);
            json_t *card = ws1_provider_card(d, desc_node, NULL, 0, adv_true, &cerr);
            json_free(desc_node);
            json_free(adv_true);
            free(desc);
            if (cerr) { out = cerr; break; }
            char *bus = ws1_pystr(burl);
            if (!json_obj_get(meta, bus ? bus : ""))
                json_set(meta, bus ? bus : "", card);
            else
                json_free(card);
            free(bus);
        }
        json_t *auth_type = json_obj_get(d, "auth_type");
        if (!auth_type) { out = ws1_attr_error(d, "auth_type"); break; }
        if (ws1_is_list(auth_type) || ws1_is_dict(auth_type)) { out = ws1_unhashable(auth_type); break; }
        /* _AUTH_TYPE_ENV_VARS.get(auth_type, ()) */
        const char *vars[2] = {NULL, NULL};
        int nvars = 0, fmt = 0;
        if (ws1_is_str(auth_type) && strcmp(auth_type->str_val, "aws_sdk") == 0) {
            vars[0] = "AWS_REGION"; vars[1] = "AWS_PROFILE"; nvars = 2; fmt = 1;
        } else if (ws1_is_str(auth_type) && strcmp(auth_type->str_val, "vertex") == 0) {
            vars[0] = "VERTEX_CREDENTIALS_PATH"; nvars = 1; fmt = 2;
        }
        for (int vi = 0; vi < nvars && !out; vi++) {
            const char *var = vars[vi];
            json_t *existing = json_obj_get(meta, var);
            json_t *edesc = existing ? ws1_get0(existing, "description") : NULL;
            json_t *desc_node = NULL;
            if (ws1_jtruth(edesc)) {
                desc_node = json_copy(edesc);
            } else {
                json_t *label = json_obj_get(d, "label");
                if (!label) { out = ws1_attr_error(d, "label"); break; }
                char *ls = ws1_pystr(label);
                ws1_sb b; ws1_sb_init(&b);
                if (fmt == 1) ws1_sb_printf(&b, "%s (%s)", ls ? ls : "", var);
                else ws1_sb_printf(&b, "%s \xE2\x80\x94 service account JSON path (or use ADC)",
                                   ls ? ls : "");
                free(ls);
                char *s = ws1_sb_take(&b);
                desc_node = json_string(s);
                free(s);
            }
            json_t *eurl = existing ? ws1_get0(existing, "url") : NULL;
            json_t *adv = NULL;
            int adv_owned = 0;
            if (existing && json_obj_get(existing, "advanced")) {
                adv = json_obj_get(existing, "advanced");
            } else {
                adv = json_bool(true);
                adv_owned = 1;
            }
            json_t *cerr = NULL;
            json_t *card = ws1_provider_card(d, desc_node, eurl, 0, adv, &cerr);
            json_free(desc_node);
            if (adv_owned) json_free(adv);
            if (cerr) { out = cerr; break; }
            json_set(meta, var, card);
        }
    }
    json_free(cards);
done:
    json_free(non_provider);
    if (out) { json_free(meta); return out; }
    return meta;
}