/*
 * discord_adapter_flow.c -- FLOW cores + leaves of
 * plugins/platforms/discord/adapter.py (hermes-agent): module-level helpers
 * (image extension, markdown link, intent guidance, snowflake time,
 * non-conversational metadata, ID cleaning, transport-error classification,
 * dependency probes, timeout budgets), the liveness-config knob chain
 * (_config_value -> _liveness_knob -> _finite_positive_config_float /
 * _config_int), connect-exception classification, the gateway_platform_event
 * fire chain (_platform_events_subscribed -> _emit_platform_event ->
 * _source_for_platform_event / _fire_platform_event, message/thread event
 * part normalizers) and command-sync skip / retry-after / rate-limit /
 * unknown-interaction classification.
 *
 * Decision sequences only: every live I/O / injected callable / sibling
 * method enters through ONE universal seam: da_call(ctx, name, args). The
 * harness wires it to a recording stub fed the case's scripted results
 * (in.io); the oracle installs recording stubs emitting the identical event
 * shape {"name", "args", "ret"} from the SAME scripts.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and da_-prefixed.
 *
 * Seam name table (name -> live call):
 *   os.getenv                     os.getenv(key, "")
 *   logger.warning / logger.debug logger.<level>(fmt, *args) (exc_info dropped)
 *   time.time                     time.time()
 *   is_multiplex_active           agent.secret_scope.is_multiplex_active()
 *   has_hook                      hermes_cli.lifecycle.has_hook("gateway_platform_event")
 *   _prefix_within_utf16_limit    gateway.platforms.base._prefix_within_utf16_limit
 *   _scoped_gate_env              gateway.platforms._shared.platform_gate_env
 *   parse_retry_after_seconds     agent.retry_utils.parse_retry_after_seconds
 *   is_automated_message          adapter_authorization.is_automated_message
 *   _config_value / _liveness_knob / _warn_liveness_config_disabled
 *   _read_command_sync_state / _command_sync_state_key
 *   _platform_events_subscribed / _source_for_platform_event
 *   _fire_platform_event / _thread_id_and_chat_for_channel
 *   _needs_server_members_intent / _format_privileged_intents_guidance
 *                                 sibling method / module calls (scripted)
 *   build_source                  self.build_source(**kwargs)
 *   build / extra_payload         injected callback params of the normalizers
 *   platform_event_handler        self._platform_event_handler(event, source)
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real null VALUE.
 * - An exception is a {"__raise__": cls, "message": str(exc)} object
 *   (OSError adds errno, message "[Errno N] msg"). BaseException kinds
 *   (SystemExit, KeyboardInterrupt, GeneratorExit, CancelledError) pass
 *   through every except-Exception site.
 * - Exception doubles (args.exc): {"cls": name, "msg": str(exc),
 *   "discord_base": null|<discord class name>, "attrs": {...}}. cls is the
 *   concrete class name (isinstance-of-builtin keyed on it when
 *   discord_base is null); discord_base carries the discord.py base class
 *   for the isinstance paths (only meaningful when state.discord_available).
 * - Attribute doubles (message/channel/thread/author/guild/config): JSON
 *   objects materialized as SimpleNamespace; an OBJECT double is ALWAYS
 *   truthy (even empty), matching a live attribute carrier; scalars keep
 *   Python value truthiness. Harness-only tags (is_thread) are never read
 *   by live code.
 * - cast params travel as the strings "int"/"float".
 * - _discord_snowflake_time projects its datetime return to .isoformat().
 * - Seam args keep explicit nulls (env_key: null etc.); logger exc_info
 *   kwargs are dropped on both sides.
 * - state flags: discord_available, multiplex_import_ok,
 *   lifecycle_import_ok (absent import flags default true); state.name is
 *   the adapter log name; state.config.extra / state._allowed_user_ids /
 *   state._allowed_role_ids / state._platform_event_handler feed the reads.
 *   _platform_event_handler: null -> absent, "seam" -> async recording
 *   stub, any other value -> non-callable attribute (TypeError at call).
 * - No ported function mutates adapter state; out.state echoes in.state.
 *
 * Fixture corpus domains (documented restrictions):
 * - str() positions carry strings / integral numbers / bools / None only
 *   (no non-integral float repr, no container repr).
 * - float()/int() string parsing folds ASCII digits and the full Unicode
 *   White_Space strip set; Unicode Nd digits are outside the corpus.
 * - str.lower() folds ASCII (exc messages / class names / marker text).
 * - isdigit() folds ASCII digits (superscript/Unicode digits excluded).
 * - thread_id seam results are strings or null; command_sync_state_key
 *   scripts return strings; app_id is string/integral/bool/null.
 * - float-string ValueError messages quote ASCII-printable originals.
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

/* -- universal seam context -- */
typedef struct da_ctx {
    json_t *state;      /* owned; input state copy, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} da_ctx_t;

/* -- basic helpers -- */
static bool da_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

static const char *da_raise_kind(const json_t *v) {
    return da_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}

/* BaseException kinds pass through every except-Exception site. */
static bool da_is_base_exc(const char *kind) {
    return kind && (strcmp(kind, "SystemExit") == 0
                 || strcmp(kind, "KeyboardInterrupt") == 0
                 || strcmp(kind, "GeneratorExit") == 0
                 || strcmp(kind, "CancelledError") == 0);
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool da_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

/* Attribute-double truthiness: an OBJECT carrier is always truthy. */
static bool da_attr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    if (v->type == JSON_OBJECT) return true;
    return da_truthy(v);
}

static bool da_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static const char *da_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool da_str_eq(const json_t *v, const char *s) {
    const char *t = da_str(v);
    return t && s && strcmp(t, s) == 0;
}

static json_t *da_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static bool da_is_oserror_type(const char *t) {
    static const char *fam[] = {
        "OSError", "IOError", "TimeoutError", "PermissionError",
        "FileNotFoundError", "FileExistsError", "BlockingIOError",
        "ConnectionError", "ConnectionAbortedError", "ConnectionRefusedError",
        "ConnectionResetError", "BrokenPipeError", "InterruptedError",
        "NotADirectoryError", "IsADirectoryError", "ChildProcessError",
        "ProcessLookupError", "DirectoryNotEmptyError",
    };
    for (size_t i = 0; i < sizeof(fam) / sizeof(fam[0]); i++)
        if (strcmp(t, fam[i]) == 0) return true;
    return false;
}

/* Materialize a scripted raise spec exactly as the oracle's make_exc does. */
static json_t *da_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER && da_is_oserror_type(type)) {
        char buf[512];
        snprintf(buf, sizeof buf, "[Errno %.0f] %s", en->num_val, msg);
        json_t *e = da_err(type, buf);
        json_set(e, "errno", json_copy(en));
        return e;
    }
    return da_err(type, msg);
}

/* The one universal seam. args is ADOPTED into the trace event; the return
 * is a fresh owned node (scripted value copy or raise descriptor). A
 * missing/exhausted script RETURNS a fail-closed RuntimeError spec (the
 * event is recorded before the caller propagates it). */
static json_t *da_call(da_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da_make_exc(raise);
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

/* -- string helpers -- */
static char *da_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

/* decode the codepoint at byte p (valid UTF-8 fixture corpus). */
static unsigned int da_decode_cp(const unsigned char *p) {
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xe0) == 0xc0)
        return ((unsigned)(p[0] & 0x1f) << 6) | (p[1] & 0x3f);
    if ((p[0] & 0xf0) == 0xe0)
        return ((unsigned)(p[0] & 0x0f) << 12) | ((unsigned)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
    return ((unsigned)(p[0] & 0x07) << 18) | ((unsigned)(p[1] & 0x3f) << 12)
        | ((unsigned)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}

static size_t da_cp_len(const unsigned char *p) {
    if (p[0] < 0x80) return 1;
    if ((p[0] & 0xe0) == 0xc0) return 2;
    if ((p[0] & 0xf0) == 0xe0) return 3;
    return 4;
}

/* Python str.strip() White_Space set (full Unicode, not just ASCII). */
static bool da_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x1f))
        return true;
    if (cp == 0x85 || cp == 0xa0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200a) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000)
        return true;
    return false;
}

/* str.strip(): full Unicode White_Space both ends. Caller frees. */
static char *da_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *p = (const unsigned char *)s;
    while (*p && da_is_ws_cp(da_decode_cp(p))) p += da_cp_len(p);
    const unsigned char *end = p + strlen((const char *)p);
    while (end > p) {
        const unsigned char *q = end - 1;
        while (q > p && (*q & 0xc0) == 0x80) q--;
        if (!da_is_ws_cp(da_decode_cp(q))) break;
        end = q;
    }
    size_t n = (size_t)(end - p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, p, n);
    o[n] = '\0';
    return o;
}

/* str.lower(): ASCII fold (fixture corpus folds ASCII only). */
static char *da_lower(const char *s) {
    char *o = da_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* UTF-8 codepoint count (Python len counts characters). */
static size_t da_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xc0) != 0x80) n++;
    return n;
}

/* byte pointer at codepoint index i (clamped to [0, len]). */
static const char *da_cp_ptr(const char *s, long long i) {
    if (!s) return "";
    if (i < 0) i = 0;
    const char *p = s;
    while (i > 0 && *p) {
        p++;
        while (*p && ((unsigned char)*p & 0xc0) == 0x80) p++;
        i--;
    }
    return p;
}

/* Python s[:b] on codepoints (b clamped; negative b counts from end). */
static char *da_slice_head(const char *s, long long b) {
    if (!s) s = "";
    long long n = (long long)da_utf8_len(s);
    if (b < 0) b = n + b;
    if (b < 0) b = 0;
    if (b > n) b = n;
    const char *pb = da_cp_ptr(s, b);
    size_t len = (size_t)(pb - s);
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s, len);
    o[len] = '\0';
    return o;
}

static bool da_contains(const char *hay, const char *needle) {
    return hay && needle && strstr(hay, needle) != NULL;
}

static bool da_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool da_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return false;
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

/* case-insensitive prefix test (ASCII). */
static bool da_starts_with_ci(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    for (size_t i = 0; prefix[i]; i++) {
        if (!s[i]) return false;
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

/* JSON type name for Python error messages (fixture scalar/container set). */
static const char *da_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && isfinite(d) && fabs(d) < 9.2e18) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* -- Python numeric coercion on the fixture corpus -- */

/* float(str): ASCII digits, underscores BETWEEN digits, full Unicode
 * White_Space strip, inf/infinity/nan (case-insensitive), sign carried into
 * the clean buffer. Returns false on ValueError. */
static bool da_float_str(const char *s, double *out) {
    char *t = da_strip(s);
    if (!t) return false;
    const char *p = t;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    /* inf / infinity / nan special forms (exact, case-insensitive). */
    {
        char low[16];
        size_t i = 0;
        for (; p[i] && i < 15; i++) low[i] = (char)tolower((unsigned char)p[i]);
        low[i] = '\0';
        if (p[i] == '\0') {
            if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
                *out = neg ? -HUGE_VAL : HUGE_VAL;
                free(t);
                return true;
            }
            if (strcmp(low, "nan") == 0) {
                *out = NAN;
                free(t);
                return true;
            }
        }
    }
    /* numeric scan: digits[_digits][.digits[_digits]][eE[+-]digits] */
    char clean[512];
    size_t k = 0;
    if (neg) clean[k++] = '-';
    const char *start = p;
    size_t ndigits = 0;
    bool prev_digit = false, seen_dot = false;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') {
            if (k + 1 >= sizeof clean) { free(t); return false; }
            clean[k++] = (char)c;
            ndigits++;
            prev_digit = true;
            p++;
        } else if (c == '_') {
            /* underscore must sit BETWEEN two digits */
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { free(t); return false; }
            prev_digit = false;
            p++;
        } else if (c == '.' && !seen_dot) {
            seen_dot = true;
            prev_digit = false;
            if (k + 1 >= sizeof clean) { free(t); return false; }
            clean[k++] = '.';
            p++;
        } else {
            break;
        }
    }
    if (ndigits == 0) { free(t); return false; }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (k + 1 >= sizeof clean) { free(t); return false; }
        clean[k++] = 'e';
        if (*p == '+' || *p == '-') {
            if (k + 1 >= sizeof clean) { free(t); return false; }
            clean[k++] = *p++;
        }
        size_t edigits = 0;
        prev_digit = false;
        while (*p) {
            unsigned char c = (unsigned char)*p;
            if (c >= '0' && c <= '9') {
                if (k + 1 >= sizeof clean) { free(t); return false; }
                clean[k++] = (char)c;
                edigits++;
                prev_digit = true;
                p++;
            } else if (c == '_') {
                if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { free(t); return false; }
                prev_digit = false;
                p++;
            } else {
                break;
            }
        }
        if (edigits == 0) { free(t); return false; }
    }
    if (*p != '\0') { free(t); return false; }
    (void)start;
    clean[k] = '\0';
    *out = strtod(clean, NULL);
    free(t);
    return true;
}

/* int(str): ASCII digits, underscores BETWEEN digits, Unicode strip, sign. */
static bool da_int_str(const char *s, long long *out) {
    char *t = da_strip(s);
    if (!t) return false;
    const char *p = t;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    char clean[512];
    size_t k = 0;
    if (neg) clean[k++] = '-';
    size_t ndigits = 0;
    bool prev_digit = false;
    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c >= '0' && c <= '9') {
            if (k + 1 >= sizeof clean) { free(t); return false; }
            clean[k++] = (char)c;
            ndigits++;
            prev_digit = true;
            p++;
        } else if (c == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) { free(t); return false; }
            prev_digit = false;
            p++;
        } else {
            free(t);
            return false;
        }
    }
    if (ndigits == 0) { free(t); return false; }
    clean[k] = '\0';
    *out = strtoll(clean, NULL, 10);
    free(t);
    return true;
}

/* repr of an ASCII-printable string for float()/int() ValueError messages. */
static char *da_repr_ascii(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n * 2 + 3);
    if (!o) return NULL;
    size_t k = 0;
    o[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' || s[i] == '\'') o[k++] = '\\';
        o[k++] = s[i];
    }
    o[k++] = '\'';
    o[k] = '\0';
    return o;
}

/* float() of a JSON value. kind: 0 ok, 1 TypeError, 2 ValueError.
 * On error *err carries the raise spec (owned). */
static int da_float_of_json(const json_t *v, double *out, json_t **err) {
    *err = NULL;
    if (v && v->type == JSON_NUMBER) { *out = v->num_val; return 0; }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 0; }
    if (v && v->type == JSON_STRING) {
        if (da_float_str(v->str_val, out)) return 0;
        char *r = da_repr_ascii(v->str_val);
        char buf[600];
        snprintf(buf, sizeof buf, "could not convert string to float: %s", r ? r : "''");
        free(r);
        *err = da_err("ValueError", buf);
        return 2;
    }
    char buf[160];
    snprintf(buf, sizeof buf,
             "float() argument must be a string or a real number, not '%s'",
             da_type_name(v));
    *err = da_err("TypeError", buf);
    return 1;
}

/* int() of a JSON value. kind: 0 ok, 1 TypeError, 2 ValueError. */
static int da_int_of_json(const json_t *v, long long *out, json_t **err) {
    *err = NULL;
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val;
        *out = (long long)(d < 0 ? ceil(d) : floor(d));
        return 0;
    }
    if (v && v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 0; }
    if (v && v->type == JSON_STRING) {
        if (da_int_str(v->str_val, out)) return 0;
        char *r = da_repr_ascii(v->str_val);
        char buf[600];
        snprintf(buf, sizeof buf, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        *err = da_err("ValueError", buf);
        return 2;
    }
    char buf[192];
    snprintf(buf, sizeof buf,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'",
             da_type_name(v));
    *err = da_err("TypeError", buf);
    return 1;
}

/* str() projection: strings verbatim, integral numbers as ints, bools,
 * None -> "None". Non-integral floats / containers are outside the corpus. */
static char *da_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da_strdup("None");
    if (v->type == JSON_STRING) return da_strdup(v->str_val);
    if (v->type == JSON_BOOL) return da_strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%.0f", v->num_val);
        return da_strdup(buf);
    }
    return da_strdup("");
}

/* Python max(a, b) / min(a, b) on doubles: NaN comparisons are false. */
static double da_max2(double a, double b) { return b > a ? b : a; }
static double da_min2(double a, double b) { return b < a ? b : a; }

/* -- snowflake -> UTC isoformat projection -- */
/* Howard Hinnant civil_from_days: days since 1970-01-01 -> y/m/d. */
static void da_civil_from_days(long long z, int *y, unsigned *m, unsigned *d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int)(yy + (*m <= 2));
}

/* PoP: da_discord_snowflake_time @ plugins/platforms/discord/adapter.py:_discord_snowflake_time */
static json_t *da_discord_snowflake_time(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *sfv = json_obj_get(args, "snowflake");
    double sfd = (sfv && sfv->type == JSON_NUMBER) ? sfv->num_val : 0.0;
    long long sf = (long long)sfd;
    /* Python >> is a FLOOR shift (arithmetic on negatives). */
    long long q = sf / 4194304;
    if (sf < 0 && sf % 4194304 != 0) q--;
    long long ms = q + 1420070400000LL;
    double epoch_s = (double)ms / 1000.0;
    /* CPython fromtimestamp rounds the double to microseconds (half-even). */
    long long us_total = (long long)llrint(epoch_s * 1e6);
    long long day = us_total / 86400000000LL;
    long long rem = us_total % 86400000000LL;
    if (rem < 0) { rem += 86400000000LL; day--; }
    int y; unsigned mo, dd;
    da_civil_from_days(day, &y, &mo, &dd);
    unsigned hh = (unsigned)(rem / 3600000000LL);
    unsigned mi = (unsigned)((rem / 60000000LL) % 60);
    unsigned ss = (unsigned)((rem / 1000000LL) % 60);
    unsigned us = (unsigned)(rem % 1000000LL);
    char buf[96];
    if (us) {
        snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02u:%02u:%02u.%06u+00:00",
                 y, mo, dd, hh, mi, ss, us);
    } else {
        snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02u:%02u:%02u+00:00",
                 y, mo, dd, hh, mi, ss);
    }
    return json_string(buf);
}

/* -- urllib.parse.quote(url, safe=":/?#[]@!$&'*+,;=%") -- */
static bool da_quote_safe(unsigned char c) {
    static const char safe[] = ":/?#[]@!$&'*+,;=%";
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        return true;
    if (c == '-' || c == '.' || c == '_' || c == '~') return true;
    return strchr(safe, c) != NULL && c != '\0';
}

static char *da_quote(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n * 3 + 1);
    if (!o) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (da_quote_safe(c)) {
            o[k++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            o[k++] = '%';
            o[k++] = hex[c >> 4];
            o[k++] = hex[c & 0xf];
        }
    }
    o[k] = '\0';
    return o;
}

/* PoP: da_image_ext_from_content_type @ plugins/platforms/discord/adapter.py:_image_ext_from_content_type */
static json_t *da_image_ext_from_content_type(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *ct = json_get_str(args, "content_type", "");
    if (da_contains(ct, "jpeg") || da_contains(ct, "jpg")) return json_string("jpg");
    if (da_contains(ct, "gif")) return json_string("gif");
    if (da_contains(ct, "webp")) return json_string("webp");
    return json_string("png");
}

/* PoP: da_format_discord_markdown_link @ plugins/platforms/discord/adapter.py:_format_discord_markdown_link */
static json_t *da_format_discord_markdown_link(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *label = json_get_str(args, "label", "");
    const char *url = json_get_str(args, "url", "");
    /* _DISCORD_URL_LABEL_SCHEME_RE.sub("", label, count=1): ^https?:// (I). */
    const char *lp = label;
    if (da_starts_with_ci(lp, "https://")) lp += 8;
    else if (da_starts_with_ci(lp, "http://")) lp += 7;
    /* escape backslash / [ / ] with a preceding backslash. */
    size_t n = strlen(lp);
    char *esc = malloc(n * 2 + 1);
    if (!esc) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        if (lp[i] == '\\' || lp[i] == '[' || lp[i] == ']') esc[k++] = '\\';
        esc[k++] = lp[i];
    }
    esc[k] = '\0';
    char *qurl = da_quote(url);
    size_t cap = k + (qurl ? strlen(qurl) : 0) + 8;
    char *o = malloc(cap);
    if (o) snprintf(o, cap, "[%s](<%s>)", esc, qurl ? qurl : "");
    free(esc);
    free(qurl);
    json_t *r = json_string(o ? o : "");
    free(o);
    return r;
}

/* PoP: da_needs_server_members_intent @ plugins/platforms/discord/adapter.py:_needs_server_members_intent */
static json_t *da_needs_server_members_intent(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *users = json_obj_get(args, "allowed_user_ids");
    const json_t *roles = json_obj_get(args, "allowed_role_ids");
    bool need = false;
    if (da_truthy(users) && users->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(users); i++) {
            const json_t *e = json_get(users, i);
            if (da_str_eq(e, "*")) continue;
            char *s = da_py_str(e);
            bool digits = s && s[0] != '\0';
            if (s) for (const char *p = s; *p; p++)
                if (!(*p >= '0' && *p <= '9')) { digits = false; break; }
            free(s);
            if (!digits) { need = true; break; }
        }
    }
    if (!need) need = da_truthy(roles);
    return json_bool(need);
}

/* PoP: da_format_privileged_intents_guidance @ plugins/platforms/discord/adapter.py:_format_privileged_intents_guidance */
static json_t *da_format_privileged_intents_guidance(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    bool needs = da_truthy(json_obj_get(args, "needs_members"));
    /* U+2192 RIGHTWARDS ARROW in the Fix line (live constant). */
    const char *arrow = "\xe2\x86\x92";
    char buf[1200];
    if (needs) {
        snprintf(buf, sizeof buf,
            "Discord rejected the connection because privileged Gateway Intents "
            "are not enabled for this bot in the Developer Portal.\n"
            "Hermes is requesting:\n"
            "  - Message Content Intent (required to read message text)\n"
            "  - Server Members Intent (required for username allowlists "
            "and/or DISCORD_ALLOWED_ROLES)\n"
            "Fix: https://discord.com/developers/applications %s your application "
            "%s Bot %s Privileged Gateway Intents %s enable the intent(s) listed "
            "above %s Save Changes, then restart the gateway.\n"
            "Docs: https://hermes-agent.nousresearch.com/docs/user-guide/messaging/discord",
            arrow, arrow, arrow, arrow, arrow);
    } else {
        snprintf(buf, sizeof buf,
            "Discord rejected the connection because privileged Gateway Intents "
            "are not enabled for this bot in the Developer Portal.\n"
            "Hermes is requesting:\n"
            "  - Message Content Intent (required to read message text)\n"
            "Fix: https://discord.com/developers/applications %s your application "
            "%s Bot %s Privileged Gateway Intents %s enable the intent(s) listed "
            "above %s Save Changes, then restart the gateway.\n"
            "Docs: https://hermes-agent.nousresearch.com/docs/user-guide/messaging/discord",
            arrow, arrow, arrow, arrow, arrow);
    }
    return json_string(buf);
}

/* PoP: da_metadata_marks_nonconversational @ plugins/platforms/discord/adapter.py:_metadata_marks_nonconversational */
static json_t *da_metadata_marks_nonconversational(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *md = json_obj_get(args, "metadata");
    if (!md || md->type != JSON_OBJECT) return json_bool(false);
    if (da_truthy(json_obj_get(md, "non_conversational"))) return json_bool(true);
    if (da_truthy(json_obj_get(md, "non_conversational_history"))) return json_bool(true);
    return json_bool(false);
}

/* PoP: da_prompt_target_id @ plugins/platforms/discord/adapter.py:_prompt_target_id */
static json_t *da_prompt_target_id(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *md = json_obj_get(args, "metadata");
    const json_t *chat = json_obj_get(args, "chat_id");
    if (da_truthy(md)) {
        if (md->type != JSON_OBJECT) {
            char buf[128];
            snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", da_type_name(md));
            return da_err("AttributeError", buf);
        }
        const json_t *tid = json_obj_get(md, "thread_id");
        if (da_truthy(tid)) return json_copy(tid);
    }
    return chat ? json_copy(chat) : json_null();
}

/* PoP: da_clean_discord_id @ plugins/platforms/discord/adapter.py:_clean_discord_id */
static json_t *da_clean_discord_id(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *entry = json_get_str(args, "entry", "");
    char *s = da_strip(entry);
    if (!s) return json_string("");
    if (da_starts_with(s, "<@") && da_ends_with(s, ">")) {
        /* lstrip("<@!").rstrip(">") */
        char *p = s;
        while (*p == '<' || *p == '@' || *p == '!') p++;
        size_t n = strlen(p);
        while (n > 0 && p[n - 1] == '>') n--;
        char *tmp = malloc(n + 1);
        if (tmp) { memcpy(tmp, p, n); tmp[n] = '\0'; free(s); s = tmp; }
    }
    if (da_starts_with_ci(s, "user:") && strlen(s) >= 5) {
        char *tmp = da_strdup(s + 5);
        free(s);
        s = tmp;
    }
    char *out = da_strip(s);
    free(s);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: da_command_sync_state_key @ plugins/platforms/discord/adapter.py:_command_sync_state_key */
static json_t *da_command_sync_state_key(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *app = json_obj_get(args, "app_id");
    if (!da_truthy(app)) return json_string("unknown");
    char *s = da_py_str(app);
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}

/* PoP: da_command_sync_mutation_interval_seconds @ plugins/platforms/discord/adapter.py:_command_sync_mutation_interval_seconds */
static json_t *da_command_sync_mutation_interval_seconds(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    (void)args;
    return json_number(4.5); /* _DISCORD_COMMAND_SYNC_MUTATION_INTERVAL_SECONDS */
}
/* -- seam-driven leaves -- */

/* PoP: da_truncate_discord_component_text @ plugins/platforms/discord/adapter.py:_truncate_discord_component_text */
static json_t *da_truncate_discord_component_text(da_ctx_t *ctx, const json_t *args) {
    const json_t *text = json_obj_get(args, "text");
    char *s;
    if (da_truthy(text)) s = da_py_str(text);
    else s = da_strdup("");
    const json_t *lim = json_obj_get(args, "limit");
    long long limit = (lim && lim->type == JSON_NUMBER) ? (long long)lim->num_val : 0;
    if (limit < 0) limit = 0; /* max(0, limit) */
    json_t *a = json_object();
    json_set_hdr(a, "text", s ? s : "");
    free(s);
    json_set(a, "limit", json_number((double)limit));
    json_t *r = da_call(ctx, "_prefix_within_utf16_limit", a);
    return r; /* raise specs propagate (no try in live) */
}

/* os.getenv seam result must be a string; .strip() on anything else raises
 * AttributeError (propagates -- no try around the getenv call). */
static json_t *da_getenv_strip(da_ctx_t *ctx, const char *key, char **out) {
    *out = NULL;
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set_hdr(a, "default", "");
    json_t *r = da_call(ctx, "os.getenv", a);
    if (da_is_raise(r)) return r;
    const char *s = da_str(r);
    if (!s) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'strip'", da_type_name(r));
        json_free(r);
        return da_err("AttributeError", buf);
    }
    *out = da_strip(s);
    json_free(r);
    return NULL;
}

/* PoP: da_discord_ready_timeout_seconds @ plugins/platforms/discord/adapter.py:_discord_ready_timeout_seconds */
static json_t *da_discord_ready_timeout_seconds(da_ctx_t *ctx, const json_t *args) {
    (void)args;
    char *raw = NULL;
    json_t *err = da_getenv_strip(ctx, "HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT", &raw);
    if (err) return err;
    if (raw && raw[0]) {
        double v;
        if (da_float_str(raw, &v)) {
            free(raw);
            return json_number(da_max2(0.0, v));
        }
        /* ValueError -> logger.warning (args verbatim, %r NOT applied). */
        json_t *a = json_object();
        json_set_hdr(a, "fmt", "Ignoring invalid HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT=%r");
        json_t *aa = json_array();
        json_append(aa, json_string(raw));
        json_set(a, "args", aa);
        free(raw);
        json_t *lr = da_call(ctx, "logger.warning", a);
        if (da_is_raise(lr)) return lr;
        json_free(lr);
        return json_number(30.0);
    }
    free(raw);
    return json_number(30.0);
}

/* PoP: da_text_batch_flush_deadline_seconds @ plugins/platforms/discord/adapter.py:_text_batch_flush_deadline_seconds */
static json_t *da_text_batch_flush_deadline_seconds(da_ctx_t *ctx, const json_t *args) {
    (void)args;
    char *raw = NULL;
    json_t *err = da_getenv_strip(ctx, "HERMES_GATEWAY_ADAPTER_DISCONNECT_TIMEOUT", &raw);
    if (err) return err;
    double budget = 5.0;
    if (raw && raw[0]) {
        double parsed;
        if (da_float_str(raw, &parsed) && parsed > 0) budget = parsed;
        /* ValueError -> pass */
    }
    free(raw);
    double headroom = da_max2(0.5, budget * 0.2);
    double deadline = da_max2(1.0, budget - headroom);
    return json_number(da_min2(deadline, budget * 0.9));
}

static bool da_state_flag(const da_ctx_t *ctx, const char *key, bool def) {
    const json_t *v = json_obj_get(ctx->state, key);
    if (!v || v->type == JSON_NULL) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    return da_truthy(v);
}

/* PoP: da_multiplex_active @ plugins/platforms/discord/adapter.py:_multiplex_active */
static json_t *da_multiplex_active(da_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (!da_state_flag(ctx, "multiplex_import_ok", true)) return json_bool(false);
    json_t *r = da_call(ctx, "is_multiplex_active", json_object());
    if (da_is_raise(r)) {
        if (da_is_base_exc(da_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false); /* except Exception */
    }
    bool b = da_truthy(r);
    json_free(r);
    return json_bool(b);
}

/* PoP: da_discord_deps_present @ plugins/platforms/discord/adapter.py:discord_deps_present */
static json_t *da_discord_deps_present(da_ctx_t *ctx, const json_t *args) {
    (void)args;
    return json_bool(da_state_flag(ctx, "discord_available", false));
}

/* PoP: da_is_discord_transport_error @ plugins/platforms/discord/adapter.py:_is_discord_transport_error */
static json_t *da_is_discord_transport_error(da_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const char *cls = json_get_str(exc, "cls", "");
    const char *dbase = json_get_str(exc, "discord_base", "");
    const char *msg = json_get_str(exc, "msg", "");
    bool avail = da_state_flag(ctx, "discord_available", false);
    /* isinstance(exc, asyncio.TimeoutError) -- builtin construction only. */
    if (!dbase[0] && strcmp(cls, "TimeoutError") == 0) return json_bool(false);
    /* isinstance(exc, (ConnectionError, OSError)) -- TimeoutError handled. */
    if (!dbase[0] && da_is_oserror_type(cls)) return json_bool(true);
    if (avail && dbase[0]) {
        if (strcmp(dbase, "ConnectionClosed") == 0
            || strcmp(dbase, "GatewayNotFound") == 0
            || strcmp(dbase, "DiscordServerError") == 0)
            return json_bool(true);
    }
    char *text = da_lower(msg);
    static const char *markers[] = {
        "websocket closed", "connection reset", "connection closed",
        "session is closed", "cannot write to closing transport", "not connected",
    };
    bool hit = false;
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
        if (da_contains(text, markers[i])) { hit = true; break; }
    free(text);
    return json_bool(hit);
}

/* -- liveness-config chain -- */

/* PoP: da_config_value @ plugins/platforms/discord/adapter.py:_config_value */
static json_t *da_config_value(da_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *def = json_obj_get(args, "default");
    const json_t *env_key = json_obj_get(args, "env_key");
    const json_t *config = json_obj_get(ctx->state, "config");
    const json_t *extra = NULL;
    if (config && config->type == JSON_OBJECT) {
        const json_t *e = json_obj_get(config, "extra");
        if (e && e->type == JSON_OBJECT) extra = e;
    }
    const json_t *value = NULL;
    json_t *owned = NULL; /* seam result whose ownership we hold */
    if (extra && da_str(key)) value = json_obj_get(extra, key->str_val);
    if (da_is_null(value) && da_truthy(env_key)) {
        json_t *a = json_object();
        json_set(a, "key", json_copy(env_key));
        json_t *r = da_call(ctx, "_scoped_gate_env", a);
        if (da_is_raise(r)) return r;
        if (da_truthy(r)) { value = r; owned = r; } /* ... or None */
        else json_free(r);
    }
    json_t *ret;
    if (da_is_null(value) || da_str_eq(value, ""))
        ret = def ? json_copy(def) : json_null();
    else
        ret = json_copy(value);
    if (owned) json_free(owned);
    return ret;
}

/* PoP: da_warn_liveness_config_disabled @ plugins/platforms/discord/adapter.py:_warn_liveness_config_disabled */
static json_t *da_warn_liveness_config_disabled(da_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *raw = json_obj_get(args, "raw");
    const char *scope =
        da_str_eq(key, "websocket_event_max_silence_seconds")
            ? "the event-silence dimension of the websocket liveness probe"
            : "the websocket liveness probe";
    json_t *a = json_object();
    json_set_hdr(a, "fmt",
        "[%s] Discord liveness knob %s=%r is not a usable positive number; "
        "%s is disabled by this value");
    json_t *aa = json_array();
    const json_t *name = json_obj_get(ctx->state, "name");
    json_append(aa, name ? json_copy(name) : json_string("Discord"));
    json_append(aa, key ? json_copy(key) : json_null());
    json_append(aa, raw ? json_copy(raw) : json_null());
    json_append(aa, json_string(scope));
    json_set(a, "args", aa);
    json_t *r = da_call(ctx, "logger.warning", a);
    if (da_is_raise(r)) return r;
    json_free(r);
    return NULL; /* live returns None */
}

/* PoP: da_liveness_knob @ plugins/platforms/discord/adapter.py:_liveness_knob */
static json_t *da_liveness_knob(da_ctx_t *ctx, const json_t *args) {
    const json_t *key = json_obj_get(args, "key");
    const json_t *def = json_obj_get(args, "default");
    const json_t *env_key = json_obj_get(args, "env_key");
    /* Oracle maps every non-"int" cast name to float; it has no "unknown cast" path. */
    const char *cast = json_get_str(args, "cast", "float");
    if (!cast || strcmp(cast, "int") != 0) cast = "float";
    json_t *a = json_object();
    json_set(a, "key", key ? json_copy(key) : json_null());
    json_set(a, "default", def ? json_copy(def) : json_null());
    json_set(a, "env_key", env_key ? json_copy(env_key) : json_null());
    json_t *raw = da_call(ctx, "_config_value", a);
    if (da_is_raise(raw)) return raw;
    /* value = None if isinstance(raw, bool) else float(raw);
     * except (TypeError, ValueError): value = None */
    bool have = false;
    double v = 0.0;
    if (raw && raw->type != JSON_BOOL) {
        json_t *ferr = NULL;
        if (da_float_of_json(raw, &v, &ferr) == 0) have = true;
        if (ferr) json_free(ferr); /* swallowed */
    }
    bool usable = false;
    double casted = 0.0;
    if (have && isfinite(v) && v >= 0) {
        if (strcmp(cast, "int") == 0) {
            double t = v < 0 ? ceil(v) : floor(v);
            if (t == v) { usable = true; casted = t; }
        } else {
            usable = true;
            casted = v;
        }
    }
    if (usable) {
        json_free(raw);
        return json_number(casted);
    }
    if (!have || v != 0.0) {
        json_t *wa = json_object();
        json_set(wa, "key", key ? json_copy(key) : json_null());
        json_set(wa, "raw", json_copy(raw));
        json_t *wr = da_call(ctx, "_warn_liveness_config_disabled", wa);
        if (da_is_raise(wr)) { json_free(raw); return wr; }
        json_free(wr);
    }
    json_free(raw);
    return json_number(0.0); /* cast(0) */
}

/* PoP: da_finite_positive_config_float @ plugins/platforms/discord/adapter.py:_finite_positive_config_float */
static json_t *da_finite_positive_config_float(da_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *key = json_obj_get(args, "key");
    const json_t *def = json_obj_get(args, "default");
    const json_t *env_key = json_obj_get(args, "env_key");
    json_set(a, "key", key ? json_copy(key) : json_null());
    json_set(a, "default", def ? json_copy(def) : json_null());
    json_set_hdr(a, "cast", "float");
    json_set(a, "env_key", env_key ? json_copy(env_key) : json_null());
    return da_call(ctx, "_liveness_knob", a);
}

/* PoP: da_config_int @ plugins/platforms/discord/adapter.py:_config_int */
static json_t *da_config_int(da_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *key = json_obj_get(args, "key");
    const json_t *def = json_obj_get(args, "default");
    const json_t *env_key = json_obj_get(args, "env_key");
    json_set(a, "key", key ? json_copy(key) : json_null());
    json_set(a, "default", def ? json_copy(def) : json_null());
    json_set_hdr(a, "cast", "int");
    json_set(a, "env_key", env_key ? json_copy(env_key) : json_null());
    return da_call(ctx, "_liveness_knob", a);
}
/* -- connect-exception classification -- */

/* PoP: da_classify_connect_exception @ plugins/platforms/discord/adapter.py:_classify_connect_exception */
static json_t *da_classify_connect_exception(da_ctx_t *ctx, const json_t *args) {
    const json_t *error = json_obj_get(args, "error");
    const json_t *shown = json_obj_get(args, "shown");
    const char *cls = json_get_str(error, "cls", "");
    const char *dbase = json_get_str(error, "discord_base", "");
    const char *msg = json_get_str(error, "msg", "");
    bool avail = da_state_flag(ctx, "discord_available", false);
    /* shown or error (str(error) == msg on the corpus). */
    const char *shown_s = da_truthy(shown) ? da_str(shown) : NULL;
    const char *eff = shown_s ? shown_s : msg;
    /* _is("LoginFailure"): class-name match, else discord isinstance. */
    bool is_login = strcmp(cls, "LoginFailure") == 0
                 || (avail && strcmp(dbase, "LoginFailure") == 0);
    if (is_login) {
        char buf[768];
        /* U+2014 EM DASH in the live message. */
        snprintf(buf, sizeof buf,
            "Discord bot token rejected: %s. The token is invalid or "
            "was revoked \xe2\x80\x94 regenerate it in the Discord Developer Portal "
            "and update DISCORD_BOT_TOKEN.", eff);
        json_t *r = json_array();
        json_append(r, json_string("discord_auth_error"));
        json_append(r, json_string(buf));
        json_append(r, json_bool(false));
        return r;
    }
    bool is_priv = strcmp(cls, "PrivilegedIntentsRequired") == 0
                || (avail && strcmp(dbase, "PrivilegedIntentsRequired") == 0);
    if (is_priv) {
        json_t *na = json_object();
        const json_t *uids = json_obj_get(ctx->state, "_allowed_user_ids");
        const json_t *rids = json_obj_get(ctx->state, "_allowed_role_ids");
        json_set(na, "allowed_user_ids", uids ? json_copy(uids) : json_null());
        json_set(na, "allowed_role_ids", rids ? json_copy(rids) : json_null());
        json_t *needs = da_call(ctx, "_needs_server_members_intent", na);
        if (da_is_raise(needs)) return needs;
        json_t *ga = json_object();
        json_set(ga, "needs_members", json_copy(needs));
        json_free(needs);
        json_t *guidance = da_call(ctx, "_format_privileged_intents_guidance", ga);
        if (da_is_raise(guidance)) return guidance;
        json_t *r = json_array();
        json_append(r, json_string("discord_intents_required"));
        json_append(r, guidance);
        json_append(r, json_bool(false));
        return r;
    }
    char buf[512];
    snprintf(buf, sizeof buf, "Discord startup failed: %s", eff);
    json_t *r = json_array();
    json_append(r, json_string("discord_connect_error"));
    json_append(r, json_string(buf));
    json_append(r, json_bool(true));
    return r;
}

/* PoP: da_thread_id_and_chat_for_channel @ plugins/platforms/discord/adapter.py:_thread_id_and_chat_for_channel */
static json_t *da_thread_id_and_chat_for_channel(da_ctx_t *ctx, const json_t *args) {
    const json_t *channel = json_obj_get(args, "channel");
    json_t *r = json_array();
    if (da_is_null(channel)) {
        json_append(r, json_null());
        json_append(r, json_null());
        return r;
    }
    const json_t *id = json_obj_get(channel, "id");
    if (da_is_null(id)) {
        json_append(r, json_null());
        json_append(r, json_null());
        return r;
    }
    bool avail = da_state_flag(ctx, "discord_available", false);
    bool is_thread = avail && da_truthy(json_obj_get(channel, "is_thread"));
    char *sid = da_py_str(id);
    json_append(r, is_thread ? json_string(sid ? sid : "") : json_null());
    json_append(r, json_string(sid ? sid : ""));
    free(sid);
    return r;
}

/* -- keyword binding / unpack semantics (py3.14 messages probed) -- */

/* Validate **kwargs binding against _source_for_platform_event's signature:
 * required keyword-only chat_id, user_id, user_name, thread_id, guild_id;
 * optional message_id. Unexpected keys are reported BEFORE missing ones.
 * Returns a raise spec (owned) or NULL when the binding is valid. */
static json_t *da_validate_source_kwargs(const json_t *kw) {
    static const char *allowed[] = {
        "chat_id", "user_id", "user_name", "thread_id", "guild_id", "message_id",
    };
    static const char *required[] = {
        "chat_id", "user_id", "user_name", "thread_id", "guild_id",
    };
    if (!kw || kw->type != JSON_OBJECT) {
        char buf[224];
        snprintf(buf, sizeof buf,
            "discord_adapter_flow_oracle._source_for_platform_event() argument "
            "after ** must be a mapping, not %s", da_type_name(kw));
        return da_err("TypeError", buf);
    }
    for (size_t i = 0; i < kw->c.count; i++) {
        const char *k = kw->c.keys[i];
        bool ok = false;
        for (size_t j = 0; j < sizeof(allowed) / sizeof(allowed[0]); j++)
            if (strcmp(k, allowed[j]) == 0) { ok = true; break; }
        if (!ok) {
            char buf[256];
            snprintf(buf, sizeof buf,
                "_source_for_platform_event() got an unexpected keyword argument '%s'", k);
            return da_err("TypeError", buf);
        }
    }
    const char *missing[5];
    size_t nm = 0;
    for (size_t j = 0; j < sizeof(required) / sizeof(required[0]); j++)
        if (!json_obj_get(kw, required[j])) missing[nm++] = required[j];
    if (nm > 0) {
        char list[256];
        size_t k = 0;
        for (size_t i = 0; i < nm; i++) {
            const char *sep;
            if (i == 0) sep = "";
            else if (nm == 2) sep = " and ";
            else if (i == nm - 1) sep = ", and ";
            else sep = ", ";
            k += (size_t)snprintf(list + k, sizeof list - k, "%s'%s'", sep, missing[i]);
        }
        char buf[512];
        snprintf(buf, sizeof buf,
            "_source_for_platform_event() missing %zu required keyword-only argument%s: %s",
            nm, nm == 1 ? "" : "s", list);
        return da_err("TypeError", buf);
    }
    return NULL;
}

/* Unpack `built` into exactly 2 items (payload, source_kwargs). Returns a
 * raise spec on failure; on success *a and *b are BORROWED pointers into built
 * (or into the static single-codepoint strings for a 2-char str unpack). */
static json_t *da_unpack2(const json_t *built, const json_t **a, const json_t **b,
                          json_t **owned) {
    *a = *b = NULL;
    *owned = NULL;
    if (built && built->type == JSON_ARRAY) {
        size_t n = json_len(built);
        if (n == 2) { *a = json_get(built, 0); *b = json_get(built, 1); return NULL; }
        char buf[128];
        if (n > 2)
            snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", n);
        else
            snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        return da_err("ValueError", buf);
    }
    if (built && built->type == JSON_STRING) {
        size_t n = da_utf8_len(built->str_val);
        if (n == 2) {
            /* two single-codepoint strings */
            const char *p0 = built->str_val;
            const char *p1 = da_cp_ptr(p0, 1);
            json_t *arr = json_array();
            char *c0 = da_slice_head(p0, 1);
            char *c1 = da_strdup(p1);
            json_append(arr, json_string(c0 ? c0 : ""));
            json_append(arr, json_string(c1 ? c1 : ""));
            free(c0);
            free(c1);
            *owned = arr;
            *a = json_get(arr, 0);
            *b = json_get(arr, 1);
            return NULL;
        }
        char buf[128];
        if (n > 2)
            snprintf(buf, sizeof buf, "too many values to unpack (expected 2)");
        else
            snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        return da_err("ValueError", buf);
    }
    if (built && built->type == JSON_OBJECT) {
        size_t n = built->c.count; /* iterating a dict yields its KEYS */
        if (n == 2) {
            json_t *arr = json_array();
            json_append(arr, json_string(built->c.keys[0]));
            json_append(arr, json_string(built->c.keys[1]));
            *owned = arr;
            *a = json_get(arr, 0);
            *b = json_get(arr, 1);
            return NULL;
        }
        char buf[128];
        if (n > 2)
            snprintf(buf, sizeof buf, "too many values to unpack (expected 2, got %zu)", n);
        else
            snprintf(buf, sizeof buf, "not enough values to unpack (expected 2, got %zu)", n);
        return da_err("ValueError", buf);
    }
    char buf[128];
    snprintf(buf, sizeof buf, "cannot unpack non-iterable %s object", da_type_name(built));
    return da_err("TypeError", buf);
}

/* PoP: da_source_for_platform_event @ plugins/platforms/discord/adapter.py:_source_for_platform_event */
static json_t *da_source_for_platform_event(da_ctx_t *ctx, const json_t *args) {
    json_t *bind_err = da_validate_source_kwargs(args);
    if (bind_err) return bind_err;
    const json_t *user_id = json_obj_get(args, "user_id");
    const json_t *chat_id = json_obj_get(args, "chat_id");
    if (!da_truthy(user_id) || !da_truthy(chat_id))
        return da_err("ValueError", "gateway_platform_event requires actor and chat identities");
    const json_t *thread_id = json_obj_get(args, "thread_id");
    const json_t *message_id = json_obj_get(args, "message_id");
    json_t *a = json_object();
    json_set(a, "chat_id", json_copy(chat_id));
    json_set_hdr(a, "chat_type", da_truthy(thread_id) ? "thread" : "group");
    json_set(a, "user_id", json_copy(user_id));
    const json_t *user_name = json_obj_get(args, "user_name");
    json_set(a, "user_name", user_name ? json_copy(user_name) : json_null());
    json_set(a, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    const json_t *guild_id = json_obj_get(args, "guild_id");
    json_set(a, "guild_id", guild_id ? json_copy(guild_id) : json_null());
    json_set(a, "message_id", message_id ? json_copy(message_id) : json_null());
    return da_call(ctx, "build_source", a);
}

/* PoP: da_platform_events_subscribed @ plugins/platforms/discord/adapter.py:_platform_events_subscribed */
static json_t *da_platform_events_subscribed(da_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (!da_state_flag(ctx, "lifecycle_import_ok", true)) return json_bool(false);
    json_t *a = json_object();
    json_set_hdr(a, "name", "gateway_platform_event");
    json_t *r = da_call(ctx, "has_hook", a);
    if (da_is_raise(r)) {
        if (da_is_base_exc(da_raise_kind(r))) return r;
        json_free(r);
        return json_bool(false); /* except Exception */
    }
    return r; /* live returns has_hook's result verbatim */
}

/* logger.debug seam for the normalize/dispatch error paths. */
static json_t *da_debug_log(da_ctx_t *ctx, const char *fmt, const json_t *arg1,
                            const json_t *arg2) {
    json_t *a = json_object();
    json_set_hdr(a, "fmt", fmt);
    json_t *aa = json_array();
    const json_t *name = json_obj_get(ctx->state, "name");
    json_append(aa, name ? json_copy(name) : json_string("Discord"));
    if (arg1) json_append(aa, json_copy(arg1));
    if (arg2) json_append(aa, json_copy(arg2));
    json_set(a, "args", aa);
    return da_call(ctx, "logger.debug", a);
}

/* PoP: da_fire_platform_event @ plugins/platforms/discord/adapter.py:_fire_platform_event */
static json_t *da_fire_platform_event(da_ctx_t *ctx, const json_t *args) {
    const json_t *handler = json_obj_get(ctx->state, "_platform_event_handler");
    if (da_is_null(handler)) return NULL; /* no callback -> fail closed */
    const json_t *event = json_obj_get(args, "event");
    const json_t *source = json_obj_get(args, "source");
    json_t *r = NULL;
    if (da_str_eq(handler, "seam")) {
        json_t *a = json_object();
        json_set(a, "event", event ? json_copy(event) : json_null());
        json_set(a, "source", source ? json_copy(source) : json_null());
        r = da_call(ctx, "platform_event_handler", a);
    } else {
        /* non-callable attribute: TypeError at the call site, caught below */
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object is not callable", da_type_name(handler));
        r = da_err("TypeError", buf);
    }
    if (da_is_raise(r)) {
        if (da_is_base_exc(da_raise_kind(r))) return r;
        json_free(r);
        json_t *lr = da_debug_log(ctx, "[%s] gateway_platform_event dispatch error", NULL, NULL);
        if (da_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    json_free(r);
    return NULL;
}

/* PoP: da_emit_platform_event @ plugins/platforms/discord/adapter.py:_emit_platform_event */
static json_t *da_emit_platform_event(da_ctx_t *ctx, const json_t *args) {
    const json_t *event_type = json_obj_get(args, "event_type");
    json_t *sub = da_call(ctx, "_platform_events_subscribed", json_object());
    if (da_is_raise(sub)) return sub;
    bool subscribed = da_truthy(sub);
    json_free(sub);
    if (!subscribed) return NULL;
    /* try: normalize block (except Exception -> debug log, return) */
    json_t *built = da_call(ctx, "build", json_object());
    if (da_is_raise(built)) {
        if (da_is_base_exc(da_raise_kind(built))) return built;
        json_free(built);
        goto normalize_error;
    }
    if (da_is_null(built)) { json_free(built); return NULL; }
    const json_t *payload = NULL, *source_kwargs = NULL;
    json_t *unpack_owned = NULL;
    json_t *uerr = da_unpack2(built, &payload, &source_kwargs, &unpack_owned);
    if (uerr) {
        json_free(built);
        if (da_is_base_exc(da_raise_kind(uerr))) return uerr;
        json_free(uerr);
        goto normalize_error;
    }
    json_t *event = json_object();
    json_set_hdr(event, "platform", "discord");
    json_set(event, "event_type", event_type ? json_copy(event_type) : json_null());
    json_set(event, "payload", payload ? json_copy(payload) : json_null());
    json_t *bind_err = da_validate_source_kwargs(source_kwargs);
    if (bind_err) {
        json_free(event);
        json_free(built);
        if (unpack_owned) json_free(unpack_owned);
        if (da_is_base_exc(da_raise_kind(bind_err))) return bind_err;
        json_free(bind_err);
        goto normalize_error;
    }
    json_t *ska = json_copy(source_kwargs);
    if (!json_obj_get(ska, "message_id")) json_set(ska, "message_id", json_null());
    json_t *source = da_call(ctx, "_source_for_platform_event", ska);
    json_free(built);
    if (unpack_owned) json_free(unpack_owned);
    if (da_is_raise(source)) {
        json_free(event);
        if (da_is_base_exc(da_raise_kind(source))) return source;
        json_free(source);
        goto normalize_error;
    }
    /* await self._fire_platform_event(event, source) -- OUTSIDE the try */
    json_t *fa = json_object();
    json_set(fa, "event", event);
    json_set(fa, "source", source);
    json_t *fr = da_call(ctx, "_fire_platform_event", fa);
    if (da_is_raise(fr)) return fr;
    json_free(fr);
    return NULL;

normalize_error:;
    json_t *lr = da_debug_log(ctx, "[%s] %s normalize error", event_type, NULL);
    if (da_is_raise(lr)) return lr;
    json_free(lr);
    return NULL;
}
/* -- event-part normalizers -- */

/* getattr(obj, key, default) projection: objects carry attributes; any
 * non-object (incl. None) yields the default. Absent key -> default. */
static const json_t *da_attr(const json_t *obj, const char *key) {
    if (obj && obj->type == JSON_OBJECT) return json_obj_get(obj, key);
    return NULL;
}

/* str(getattr(obj, key, "")) -- absent -> "", present-null -> "None". */
static char *da_attr_str_empty(const json_t *obj, const char *key) {
    const json_t *v = da_attr(obj, key);
    if (!v) return da_strdup("");
    return da_py_str(v);
}

/* merge extra into payload (dict {**payload, **extra}: later keys win,
 * existing positions kept). extra must be an object. */
static json_t *da_merge_extra(json_t *payload, const json_t *extra) {
    if (!extra || extra->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object is not a mapping", da_type_name(extra));
        return da_err("TypeError", buf);
    }
    for (size_t i = 0; i < extra->c.count; i++)
        json_set(payload, extra->c.keys[i], json_copy(extra->c.items[i]));
    return NULL;
}

/* PoP: da_message_event_parts @ plugins/platforms/discord/adapter.py:_message_event_parts */
static json_t *da_message_event_parts(da_ctx_t *ctx, const json_t *args) {
    const json_t *message = json_obj_get(args, "message");
    const json_t *author = da_attr(message, "author");
    json_t *ia = json_object();
    json_set(ia, "message", message ? json_copy(message) : json_null());
    json_t *automated = da_call(ctx, "is_automated_message", ia);
    if (da_is_raise(automated)) return automated;
    bool is_auto = da_truthy(automated);
    json_free(automated);
    if (is_auto) return NULL;
    const json_t *channel = da_attr(message, "channel");
    json_t *ca = json_object();
    json_set(ca, "channel", channel ? json_copy(channel) : json_null());
    json_t *parts = da_call(ctx, "_thread_id_and_chat_for_channel", ca);
    if (da_is_raise(parts)) return parts;
    const json_t *thread_id = NULL, *chat_id = NULL;
    json_t *unpack_owned = NULL;
    json_t *uerr = da_unpack2(parts, &thread_id, &chat_id, &unpack_owned);
    if (uerr) { json_free(parts); return uerr; }
    const json_t *message_id = da_attr(message, "id");
    if (da_is_null(chat_id) || da_is_null(message_id)) {
        json_free(parts);
        if (unpack_owned) json_free(unpack_owned);
        return NULL;
    }
    const json_t *guild = da_attr(message, "guild");
    /* payload = {chat_id, message_id, thread_id, **extra_payload(...)} */
    char *sc = da_py_str(chat_id);
    char *sm = da_py_str(message_id);
    char *sc128 = da_slice_head(sc ? sc : "", 128);
    char *sm128 = da_slice_head(sm ? sm : "", 128);
    free(sc);
    free(sm);
    json_t *payload = json_object();
    json_set_hdr(payload, "chat_id", sc128 ? sc128 : "");
    json_set_hdr(payload, "message_id", sm128 ? sm128 : "");
    free(sc128);
    free(sm128);
    if (da_truthy(thread_id)) {
        const char *ts = da_str(thread_id);
        if (!ts) {
            char buf[128];
            snprintf(buf, sizeof buf, "'%s' object is not subscriptable", da_type_name(thread_id));
            json_free(payload);
            json_free(parts);
            if (unpack_owned) json_free(unpack_owned);
            return da_err("TypeError", buf);
        }
        char *t128 = da_slice_head(ts, 128);
        json_set_hdr(payload, "thread_id", t128 ? t128 : "");
        free(t128);
    } else {
        json_set(payload, "thread_id", json_null());
    }
    json_t *ea = json_object();
    json_set(ea, "message", message ? json_copy(message) : json_null());
    json_set(ea, "author", author ? json_copy(author) : json_null());
    json_t *extra = da_call(ctx, "extra_payload", ea);
    if (da_is_raise(extra)) {
        json_free(payload);
        json_free(parts);
        if (unpack_owned) json_free(unpack_owned);
        return extra;
    }
    json_t *merr = da_merge_extra(payload, extra);
    json_free(extra);
    if (merr) {
        json_free(payload);
        json_free(parts);
        if (unpack_owned) json_free(unpack_owned);
        return merr;
    }
    /* source kwargs */
    json_t *sk = json_object();
    char *chat_s = da_py_str(chat_id);
    json_set_hdr(sk, "chat_id", chat_s ? chat_s : "");
    free(chat_s);
    const json_t *aid = da_attr(author, "id");
    if (da_truthy(aid)) {
        char *us = da_py_str(aid);
        json_set_hdr(sk, "user_id", us ? us : "");
        free(us);
    } else {
        json_set(sk, "user_id", json_null());
    }
    const json_t *uname = da_attr(author, "display_name");
    json_set(sk, "user_name", uname ? json_copy(uname) : json_null());
    json_set(sk, "thread_id", thread_id ? json_copy(thread_id) : json_null());
    if (da_attr_truthy(guild)) {
        char *gs = da_attr_str_empty(guild, "id");
        json_set_hdr(sk, "guild_id", gs ? gs : "");
        free(gs);
    } else {
        json_set(sk, "guild_id", json_null());
    }
    char *mid_s = da_py_str(message_id);
    json_set_hdr(sk, "message_id", mid_s ? mid_s : "");
    free(mid_s);
    json_free(parts);
    if (unpack_owned) json_free(unpack_owned);
    json_t *r = json_array();
    json_append(r, payload);
    json_append(r, sk);
    return r;
}

/* PoP: da_thread_event_parts @ plugins/platforms/discord/adapter.py:_thread_event_parts */
static json_t *da_thread_event_parts(da_ctx_t *ctx, const json_t *args) {
    const json_t *thread = json_obj_get(args, "thread");
    const json_t *thread_id = da_attr(thread, "id");
    const json_t *owner_id = da_attr(thread, "owner_id");
    if (da_is_null(thread_id)) return NULL;
    const json_t *parent_id = da_attr(thread, "parent_id");
    const json_t *guild = da_attr(thread, "guild");
    char *ts = da_py_str(thread_id);
    char *t128 = da_slice_head(ts ? ts : "", 128);
    json_t *payload = json_object();
    json_set_hdr(payload, "thread_id", t128 ? t128 : "");
    free(t128);
    if (!da_is_null(parent_id)) {
        char *ps = da_py_str(parent_id);
        char *p128 = da_slice_head(ps ? ps : "", 128);
        json_set_hdr(payload, "parent_chat_id", p128 ? p128 : "");
        free(ps);
        free(p128);
    } else {
        json_set(payload, "parent_chat_id", json_null());
    }
    json_t *ea = json_object();
    json_set(ea, "thread", thread ? json_copy(thread) : json_null());
    json_set(ea, "owner_id", owner_id ? json_copy(owner_id) : json_null());
    json_t *extra = da_call(ctx, "extra_payload", ea);
    if (da_is_raise(extra)) { json_free(payload); free(ts); return extra; }
    json_t *merr = da_merge_extra(payload, extra);
    json_free(extra);
    if (merr) { json_free(payload); free(ts); return merr; }
    json_t *sk = json_object();
    json_set_hdr(sk, "chat_id", ts ? ts : "");
    if (!da_is_null(owner_id)) {
        char *os = da_py_str(owner_id);
        json_set_hdr(sk, "user_id", os ? os : "");
        free(os);
    } else {
        json_set(sk, "user_id", json_null());
    }
    json_set(sk, "user_name", json_null());
    json_set_hdr(sk, "thread_id", ts ? ts : "");
    if (da_attr_truthy(guild)) {
        char *gs = da_attr_str_empty(guild, "id");
        json_set_hdr(sk, "guild_id", gs ? gs : "");
        free(gs);
    } else {
        json_set(sk, "guild_id", json_null());
    }
    free(ts);
    json_t *r = json_array();
    json_append(r, payload);
    json_append(r, sk);
    return r;
}

/* -- command-sync skip / retry classification -- */

/* float(entry.get(key) or 0): falsy -> 0.0; conversion errors PROPAGATE. */
static json_t *da_float_or0(const json_t *entry, const char *key, double *out) {
    const json_t *raw = json_obj_get(entry, key);
    if (!da_truthy(raw)) { *out = 0.0; return NULL; }
    json_t *err = NULL;
    if (da_float_of_json(raw, out, &err) == 0) return NULL;
    return err;
}

/* PoP: da_command_sync_skip_reason @ plugins/platforms/discord/adapter.py:_command_sync_skip_reason */
static json_t *da_command_sync_skip_reason(da_ctx_t *ctx, const json_t *args) {
    const json_t *app_id = json_obj_get(args, "app_id");
    const json_t *fingerprint = json_obj_get(args, "fingerprint");
    json_t *state_r = da_call(ctx, "_read_command_sync_state", json_object());
    if (da_is_raise(state_r)) return state_r;
    if (!state_r || state_r->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof buf, "'%s' object has no attribute 'get'", da_type_name(state_r));
        json_free(state_r);
        return da_err("AttributeError", buf);
    }
    json_t *ka = json_object();
    json_set(ka, "app_id", app_id ? json_copy(app_id) : json_null());
    json_t *key_r = da_call(ctx, "_command_sync_state_key", ka);
    if (da_is_raise(key_r)) { json_free(state_r); return key_r; }
    const char *key = da_str(key_r);
    const json_t *entry = key ? json_obj_get(state_r, key) : NULL;
    json_free(key_r);
    if (!entry || entry->type != JSON_OBJECT) {
        json_free(state_r);
        return NULL; /* not isinstance(entry, dict) -> None */
    }
    json_t *now_r = da_call(ctx, "time.time", json_object());
    if (da_is_raise(now_r)) { json_free(state_r); return now_r; }
    double now = 0.0;
    bool now_num = now_r && now_r->type == JSON_NUMBER;
    if (now_num) now = now_r->num_val;
    const char *now_tname = da_type_name(now_r); /* static literal; safe after free */
    json_free(now_r);
    double rau = 0.0;
    json_t *fe = da_float_or0(entry, "retry_after_until", &rau);
    if (fe) { json_free(state_r); return fe; }
    if (!now_num) {
        char buf[160];
        snprintf(buf, sizeof buf,
            "'>' not supported between instances of 'float' and '%s'", now_tname);
        json_free(state_r);
        return da_err("TypeError", buf);
    }
    if (rau > now) {
        double diff = rau - now;
        long long t = (long long)(diff < 0 ? ceil(diff) : floor(diff));
        long long remaining = t > 1 ? t : 1; /* max(1, int(...)) */
        char buf[192];
        snprintf(buf, sizeof buf,
            "Discord asked us to wait before syncing slash commands; retry in %llds",
            remaining);
        json_free(state_r);
        return json_string(buf);
    }
    double lsa = 0.0, laa = 0.0;
    fe = da_float_or0(entry, "last_success_at", &lsa);
    if (fe) { json_free(state_r); return fe; }
    fe = da_float_or0(entry, "last_attempt_at", &laa);
    if (fe) { json_free(state_r); return fe; }
    const json_t *fp = json_obj_get(entry, "fingerprint");
    bool fp_eq = (da_is_null(fp) && da_is_null(fingerprint))
              || da_str_eq(fp, da_str(fingerprint));
    if (fp_eq && lsa != 0.0 && lsa >= laa) {
        json_free(state_r);
        return json_string("same slash-command fingerprint already synced");
    }
    json_free(state_r);
    return NULL;
}

/* max(1.0, parsed): non-numeric parsed raises TypeError (propagates). */
static json_t *da_retry_max1(const json_t *parsed) {
    if (da_is_null(parsed)) return NULL;
    if (!parsed || parsed->type != JSON_NUMBER) {
        char buf[160];
        snprintf(buf, sizeof buf,
            "'>' not supported between instances of '%s' and 'float'",
            da_type_name(parsed));
        return da_err("TypeError", buf);
    }
    return json_number(da_max2(1.0, parsed->num_val));
}

/* PoP: da_extract_discord_retry_after @ plugins/platforms/discord/adapter.py:_extract_discord_retry_after */
static json_t *da_extract_discord_retry_after(da_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *attrs = da_attr(exc, "attrs");
    const json_t *value = da_attr(attrs, "retry_after");
    if (!da_is_null(value)) {
        json_t *a = json_object();
        json_set(a, "value", json_copy(value));
        json_t *parsed = da_call(ctx, "parse_retry_after_seconds", a);
        if (da_is_raise(parsed)) return parsed;
        json_t *r = da_retry_max1(parsed);
        json_free(parsed);
        return r;
    }
    const json_t *response = da_attr(attrs, "response");
    const json_t *headers = da_attr(response, "headers");
    if (!da_truthy(headers)) return NULL;
    json_t *a = json_object();
    json_set(a, "value", json_copy(headers));
    json_t *parsed = da_call(ctx, "parse_retry_after_seconds", a);
    if (da_is_raise(parsed)) return parsed;
    if (da_is_null(parsed)) {
        json_free(parsed);
        /* try: headers.get("X-RateLimit-Reset-After") except Exception */
        if (!headers || headers->type != JSON_OBJECT) return NULL;
        const json_t *kv = json_obj_get(headers, "X-RateLimit-Reset-After");
        json_t *a2 = json_object();
        json_set(a2, "value", kv ? json_copy(kv) : json_null());
        json_t *p2 = da_call(ctx, "parse_retry_after_seconds", a2);
        if (da_is_raise(p2)) {
            if (da_is_base_exc(da_raise_kind(p2))) return p2;
            json_free(p2);
            return NULL; /* parsed = None */
        }
        json_t *r = da_retry_max1(p2);
        json_free(p2);
        return r;
    }
    json_t *r = da_retry_max1(parsed);
    json_free(parsed);
    return r;
}

/* PoP: da_is_discord_rate_limit @ plugins/platforms/discord/adapter.py:_is_discord_rate_limit */
static json_t *da_is_discord_rate_limit(da_ctx_t *ctx, const json_t *args) {
    const json_t *exc = json_obj_get(args, "exc");
    const char *cls = json_get_str(exc, "cls", "");
    const char *dbase = json_get_str(exc, "discord_base", "");
    const json_t *attrs = da_attr(exc, "attrs");
    bool avail = da_state_flag(ctx, "discord_available", false);
    if (avail) {
        if (strcmp(dbase, "RateLimited") == 0) return json_bool(true);
        if (strcmp(dbase, "HTTPException") == 0) {
            const json_t *status = da_attr(attrs, "status");
            if (status && status->type == JSON_NUMBER && status->num_val == 429.0)
                return json_bool(true);
        }
    }
    /* duck-type fallback */
    char *low = da_lower(cls);
    bool name_hit = da_contains(low, "ratelimit") || da_contains(low, "rate_limit");
    free(low);
    const json_t *ra = da_attr(attrs, "retry_after");
    if (name_hit && !da_is_null(ra)) return json_bool(true);
    const json_t *response = da_attr(attrs, "response");
    const json_t *st = da_attr(response, "status");
    if (!da_truthy(st)) st = da_attr(response, "status_code");
    return json_bool(st && st->type == JSON_NUMBER && st->num_val == 429.0);
}

/* PoP: da_is_discord_unknown_interaction @ plugins/platforms/discord/adapter.py:_is_discord_unknown_interaction */
static json_t *da_is_discord_unknown_interaction(da_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *exc = json_obj_get(args, "exc");
    const json_t *attrs = da_attr(exc, "attrs");
    const json_t *code = da_attr(attrs, "code");
    if (da_is_null(code)) {
        const json_t *data = da_attr(attrs, "data");
        if (data && data->type == JSON_OBJECT) code = json_obj_get(data, "code");
    }
    long long code_i = 0;
    bool code_ok = false;
    json_t *err = NULL;
    if (da_int_of_json(code, &code_i, &err) == 0) code_ok = true;
    if (err) json_free(err); /* except (TypeError, ValueError): code = None */
    const json_t *status = da_attr(attrs, "status");
    const json_t *response = da_attr(attrs, "response");
    if (da_is_null(status) && !da_is_null(response)) {
        const json_t *st = da_attr(response, "status");
        if (!da_truthy(st)) st = da_attr(response, "status_code");
        status = st;
    }
    long long status_i = 0;
    bool status_ok = false;
    err = NULL;
    if (da_int_of_json(status, &status_i, &err) == 0) status_ok = true;
    if (err) json_free(err);
    const char *msg = json_get_str(exc, "msg", "");
    char *low = da_lower(msg);
    bool hit = da_contains(low, "unknown interaction");
    free(low);
    return json_bool((code_ok && code_i == 10062)
                  || (status_ok && status_i == 404 && hit));
}
