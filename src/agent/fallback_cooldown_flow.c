/* ===========================================================================
 * fallback_cooldown_flow.c -- FLOW + leaf port of agent/fallback_cooldown.py
 * (primary rate-limit cooldown arming + per-session model entitlement
 * rejection markers) plus its UNCLAIMED leaf neighborhood:
 *   hermes_cli/route_identity.py  provider_owns_route, drop_stale_model_route,
 *                               drop_credentials_of_other_route
 *   agent/error_classifier.py     _error_obj, _json_dict,
 *                               _usage_window_display_message,
 *                               _has_usage_limit_transient_signal,
 *                               _rate_limit_reset_seconds
 *
 * Self-contained: libjson only. Every live dependency that is NOT ported here
 * is injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call);
 * the result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel installed, or a missing/exhausted script, yields
 * {"__raise__":"RuntimeError","message":"no script for <name>"} /
 * "no seam installed for <name>", recorded by the dispatcher BEFORE the port
 * propagates it.
 *
 * ENTRIES. json_t *fc_<fn>(fc_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection
 *          ({} for pure functions and on every raise path). Caller owns the
 *          returned json_t. Tuples project to JSON arrays; None to NULL.
 *
 * Ported siblings compose REAL (switch_deferred_by_reset -> _provider_reset_delay;
 * _on_primary_route -> ident; _arm_rate_limit_cooldown -> _on_primary_route +
 * _provider_reset_delay; drop_stale_model_route -> provider_owns_route;
 * _has_usage_limit_transient_signal / _rate_limit_reset_seconds -> _error_obj).
 * Claimed siblings are SEAMED: _parse_absolute_timestamp
 * (agent/credential_pool_timestamps.py, cpl_parse_absolute_timestamp),
 * normalize_route_base_url (hermes_cli/route_identity.py,
 * config_normalize_route_base_url), normalize_model_for_provider
 * (hermes_cli/model_normalize.py, model_normalize_for_provider).
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError) PASS THROUGH the one `except Exception` site
 * (switch_deferred_by_reset's load_config/threshold guard). No exception
 * parent table is needed: that site catches exactly Exception, so the check
 * is a direct kind test, not a hierarchy walk.
 *
 * Seam name -> live call table:
 *   parse_absolute_timestamp        agent.credential_pool._parse_absolute_timestamp(reset_at)
 *   time.time                       time.time()
 *   time.monotonic                  time.monotonic()
 *   load_config                     hermes_cli.config.load_config()
 *   normalize_route_base_url        hermes_cli.route_identity.normalize_route_base_url(base_url)
 *   logging.info                    logging.info(fmt, *args)
 *   logger.warning                  logger.warning(fmt, *args)
 *   pool.has_available              pool.has_available(model=...)
 *   buffer_diagnostic_status        agent._buffer_diagnostic_status(text)
 *   normalize_model_for_provider    hermes_cli.model_normalize.normalize_model_for_provider(model, provider)
 *   base_url_hostname               utils.base_url_hostname(url)
 *   normalize_provider              hermes_cli.providers.normalize_provider(raw)
 *   resolve_user_provider           hermes_cli.providers.resolve_user_provider(raw, providers_cfg)
 *   resolve_custom_provider         hermes_cli.providers.resolve_custom_provider(raw, custom_cfg)
 *   get_provider                    hermes_cli.providers.get_provider(canonical, allow_network=False)
 *   infer_provider_from_url         agent.model_metadata._infer_provider_from_url(str(base_url))
 *   clear_model_endpoint_credentials hermes_cli.config.clear_model_endpoint_credentials(model_cfg, clear_api_mode=False)
 *   parse_retry_after_seconds       agent.retry_utils.parse_retry_after_seconds(value)
 *   reset_delay_from_message        agent.retry_utils.reset_delay_from_message(error_msg)
 *
 * State model:
 *   - in.agent is the mutable carrier for the fallback_cooldown flows. The
 *     entry copies it, mutates the copy, and echoes the touched keys in st:
 *       _arm_rate_limit_cooldown: st = {_rate_limit_backoff_count (or 0),
 *                                       _rate_limited_until (or null)}
 *       _mark_entitlement_rejected_model: st = {_entitlement_rejected_models:
 *           sorted [provider, model] pairs} ONLY when the attribute exists.
 *     The rejected set is modeled as an array of 2-string arrays; membership
 *     is element-wise, output is sorted (Python set iteration is unordered).
 *   - in.model_cfg is the mutable carrier for the route_identity flows;
 *     st = {"model_cfg": final carrier}. drop_credentials_of_other_route's
 *     clear_model_endpoint_credentials seam records {clear_api_mode:false}
 *     (the carrier travels via st, not seam args); the port applies the
 *     documented live effect (pop api_key/api/key_env/api_key_env; the call
 *     site passes clear_api_mode=False, clear_api_key defaults True,
 *     clear_base_url defaults False) AFTER the seam returns non-raise. The
 *     oracle stub applies the identical effect.
 *   - On any raise path st stays {} (the oracle's BaseException wrapper
 *     echoes {} too); the harness enforces this.
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - str()-computed positions carry strings or integral numbers only;
 *     strip()/lower() operate on ASCII whitespace / ASCII letters.
 *   - reason carries a FailoverReason VALUE string or null.
 *   - api_error is an object double {status_code, message, str}: "str" is the
 *     explicit str(api_error) projection used when message is falsy (the live
 *     double's __str__ returns it); status_code/message are present-or-absent
 *     attributes (absent models getattr default).
 *   - pool / user_pdef / pdef doubles are OBJECTS: `is not None` sites model
 *     present-non-null; the `or` between resolve_user_provider and
 *     resolve_custom_provider is truthiness, so their scripts are restricted
 *     to {null, non-empty dict} (an empty {} double would be truthy live but
 *     falsy under json count>0).
 *   - _primary_runtime is null or a dict; parse_absolute_timestamp /
 *     normalize_route_base_url / normalize_model_for_provider scripts are
 *     number-or-null / string-or-null / string-or-null respectively.
 *   - response_headers is null, a dict, or a non-dict without .get (str/list/
 *     number): only a truthy dict reaches the header-field scan.
 *   - _json_dict: strict json.loads grammar is pre-validated before the
 *     lenient libjson parser (no leading zeros, no trailing commas, no raw
 *     control chars, strict escapes). NaN/Infinity and lone \u surrogates are
 *     OUTSIDE the fixture domain (json.loads accepts them, libjson does not);
 *     nesting depth is capped at 256 (fixtures are shallow).
 *   - float() coercion positions (min_switch_reset_seconds) carry numbers,
 *     numeric strings, or the raising forms "abc"/{}/[] (caught -> False).
 *   - backoff_count is an int; 60*2**count is computed in double with the
 *     14400 cap (pow overflow yields +inf, which the cap folds to 14400 just
 *     like live's arbitrary-precision min; integral floats normalize).
 * =========================================================================== */

#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

typedef struct fc_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} fc_io_t;

/* ---- truthiness / accessors ---- */

static int fc_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

/* `is not None` identity: present-non-null (an object double is always truthy
 * live; empty {} stays present here). */
static int fc_present(const json_t *v)
{
    return (v && v->type != JSON_NULL) ? 1 : 0;
}

static const char *fc_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *fc_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static double fc_num(const json_t *v)
{
    if (v && v->type == JSON_NUMBER) return v->num_val;
    if (v && v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return 0.0;
}

static int fc_is_num(const json_t *v)
{
    return v && (v->type == JSON_NUMBER || v->type == JSON_BOOL);
}

static json_t *fc_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

/* ---- raise specs ---- */

static int fc_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *fc_kind(const json_t *r)
{
    const char *k = fc_str(fc_get(r, "__raise__"));
    return k ? k : "";
}

static int fc_is_base(const char *kind)
{
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

static json_t *fc_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* malloc'd printf-style string builder */
static char *fc_xfmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}

/* ---- universal seam call ----
 * args OWNED by the caller (built fresh), borrowed by the dispatcher, freed
 * here. Result caller-owned (NULL == None). Fail-closed with no channel. */
static json_t *fc_call(fc_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = fc_xfmt("no seam installed for %s", name);
        r = fc_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *fc_args(void)
{
    return json_object();
}

/* logging.info / logger.warning: record fmt + raw positional args (the live
 * %-formatting happens lazily inside the logging machinery, never here). */
static json_t *fc_log(fc_io_t *io, const char *name, const char *fmt, json_t *args)
{
    json_t *a = fc_args();
    json_set(a, "fmt", json_string(fmt));
    json_set(a, "args", args ? args : json_array());
    return fc_call(io, name, a);
}

/* ---- string helpers (ASCII domain) ---- */

static int fc_is_ws(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

static char *fc_strip(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && fc_is_ws((unsigned char)s[a])) a++;
    while (b > a && fc_is_ws((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *fc_lower(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    out[n] = '\0';
    return out;
}

/* Python str() projection (domain: strings / integral numbers / bool / None).
 * Caller frees. */
static char *fc_pystr(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return fc_xfmt("None");
    if (v->type == JSON_BOOL) return fc_xfmt(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == 0.0) return fc_xfmt("0");   /* -0.0 renders as int 0 */
        if (d == (double)(long long)d) return fc_xfmt("%.0f", d);
        return fc_xfmt("%.17g", d);
    }
    if (v->type == JSON_STRING) return fc_xfmt("%s", v->str_val ? v->str_val : "");
    char *s = json_serialize(v);
    return s ? s : fc_xfmt("");
}

/* str(x or "") projection: falsy -> "", else str(x). Caller frees. */
static char *fc_pystr_or_empty(const json_t *v)
{
    if (!fc_truth(v)) return fc_xfmt("");
    return fc_pystr(v);
}

/* ---- Python equality (bool is a subclass of int: True == 1) ---- */
static int fc_pyequal(const json_t *a, const json_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    int an = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    int bn = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) return fc_num(a) == fc_num(b);
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_NULL: return 1;
    case JSON_STRING: return strcmp(a->str_val ? a->str_val : "",
                                    b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++)
            if (!fc_pyequal(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    case JSON_OBJECT:
        if (a->c.count != b->c.count) return 0;
        for (size_t i = 0; i < a->c.count; i++) {
            json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !fc_pyequal(a->c.items[i], bv)) return 0;
        }
        return 1;
    default: return 0;
    }
}

static const char *fc_typename(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL: return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY: return "list";
    case JSON_OBJECT: return "dict";
    default: return "object";
    }
}

/* ---- Python float() coercion ----
 * Returns the value; on failure *err carries a raise spec (caller owns).
 * Messages probed on the quarry CPython 3.14.9. */

static int fc_float_digits_ok(const char *s, size_t n)
{
    /* mantissa/exponent digit run with '_' only between digits; >=1 digit */
    size_t digits = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return 0;
            unsigned char p = (unsigned char)s[i - 1], q = (unsigned char)s[i + 1];
            if (!(p >= '0' && p <= '9') || !(q >= '0' && q <= '9')) return 0;
        } else if (s[i] >= '0' && s[i] <= '9') {
            digits++;
        } else {
            return 0;
        }
    }
    return digits > 0;
}

/* float() strips a NARROWER ws set than str.strip(): \x1c-\x1f are NOT
 * stripped (probed on the quarry CPython 3.14.9; same rule as int()). */
static char *fc_strip_float(const char *s)
{
    if (!s) s = "";
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n'
                     || s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n'
                     || s[b - 1] == '\r' || s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static int fc_parse_float_str(const char *raw, double *out)
{
    char *s = fc_strip_float(raw);
    if (!s) return 0;
    size_t n = strlen(s);
    size_t i = 0;
    int neg = 0;
    if (i < n && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); i++; }
    /* inf / infinity / nan (case-insensitive), full consumption */
    char *low = fc_lower(s + i);
    if (low) {
        if (strcmp(low, "inf") == 0 || strcmp(low, "infinity") == 0) {
            *out = neg ? -INFINITY : INFINITY;
            free(low); free(s);
            return 1;
        }
        if (strcmp(low, "nan") == 0) {
            *out = NAN;
            free(low); free(s);
            return 1;
        }
        free(low);
    }
    /* mantissa: digits ('.' digits?)? | '.' digits ; then optional exponent */
    size_t start = i;
    size_t int_end = i;
    while (int_end < n && ((s[int_end] >= '0' && s[int_end] <= '9') || s[int_end] == '_')) int_end++;
    size_t frac_start = 0, frac_end = 0;
    int has_dot = 0;
    if (int_end < n && s[int_end] == '.') {
        has_dot = 1;
        frac_start = int_end + 1;
        frac_end = frac_start;
        while (frac_end < n && ((s[frac_end] >= '0' && s[frac_end] <= '9') || s[frac_end] == '_')) frac_end++;
    }
    size_t mant_end = has_dot ? frac_end : int_end;
    size_t int_len = int_end - start, frac_len = frac_end - frac_start;
    int int_valid = int_len > 0 && fc_float_digits_ok(s + start, int_len);
    int frac_valid = frac_len > 0 && fc_float_digits_ok(s + frac_start, frac_len);
    /* mantissa: digits ('.' digits?)? | '.' digits ; '_' only between digits
     * on EACH side of the dot ("1._5" / "_1.5" are invalid). */
    if (int_len > 0) {
        if (!int_valid) { free(s); return 0; }
        if (has_dot && frac_len > 0 && !frac_valid) { free(s); return 0; }
    } else {
        if (!has_dot || !frac_valid) { free(s); return 0; }
    }
    size_t exp_end = mant_end;
    if (mant_end < n && (s[mant_end] == 'e' || s[mant_end] == 'E')) {
        size_t j = mant_end + 1;
        if (j < n && (s[j] == '+' || s[j] == '-')) j++;
        size_t es = j;
        while (j < n && ((s[j] >= '0' && s[j] <= '9') || s[j] == '_')) j++;
        if (j == es || !fc_float_digits_ok(s + es, j - es)) { free(s); return 0; }
        exp_end = j;
    }
    if (exp_end != n) { free(s); return 0; }
    /* clean buffer: drop underscores, strtod */
    char *clean = (char *)malloc(n + 2);
    if (!clean) { free(s); return 0; }
    size_t j = 0;
    if (neg) clean[j++] = '-';
    for (size_t k = i; k < n; k++) if (s[k] != '_') clean[j++] = s[k];
    clean[j] = '\0';
    char *endp = NULL;
    double d = strtod(clean, &endp);
    int ok = endp && *endp == '\0';
    if (ok) *out = d;
    free(clean); free(s);
    return ok;
}

static double fc_float_value(const json_t *v, json_t **err)
{
    *err = NULL;
    if (!v || v->type == JSON_NULL) {
        *err = fc_mkraise("TypeError",
            "float() argument must be a string or a real number, not 'NoneType'");
        return 0.0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) {
        double d;
        if (fc_parse_float_str(v->str_val ? v->str_val : "", &d)) return d;
        char *m = fc_xfmt("could not convert string to float: '%s'",
                          v->str_val ? v->str_val : "");
        *err = fc_mkraise("ValueError", m ? m : "could not convert string to float");
        free(m);
        return 0.0;
    }
    char *m = fc_xfmt("float() argument must be a string or a real number, not '%s'",
                      fc_typename(v));
    *err = fc_mkraise("TypeError", m ? m : "float() argument must be a string or a real number");
    free(m);
    return 0.0;
}

/* ---- strict json.loads grammar pre-validator ----
 * libjson's parser is lenient (leading zeros, bare '1.', invalid escapes,
 * trailing commas, raw control chars); json.loads is strict. Validate the
 * Python grammar BEFORE json_parse. NaN/Infinity and lone surrogates are
 * outside the fixture domain (documented in the header). */

typedef struct { const char *s; size_t n, i; int depth; } fc_jscan_t;

static void fc_jws(fc_jscan_t *p)
{
    while (p->i < p->n) {
        char c = p->s[p->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->i++;
        else break;
    }
}

static int fc_jvalue(fc_jscan_t *p);

static int fc_jstring(fc_jscan_t *p)
{
    if (p->i >= p->n || p->s[p->i] != '"') return 0;
    p->i++;
    while (p->i < p->n) {
        unsigned char c = (unsigned char)p->s[p->i];
        if (c == '"') { p->i++; return 1; }
        if (c < 0x20) return 0;
        if (c == '\\') {
            p->i++;
            if (p->i >= p->n) return 0;
            char e = p->s[p->i];
            if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f'
                || e == 'n' || e == 'r' || e == 't') {
                p->i++;
            } else if (e == 'u') {
                p->i++;
                if (p->i + 4 > p->n) return 0;
                for (int k = 0; k < 4; k++) {
                    char h = p->s[p->i + k];
                    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f')
                          || (h >= 'A' && h <= 'F'))) return 0;
                }
                p->i += 4;
            } else {
                return 0;
            }
        } else {
            p->i++;
        }
    }
    return 0;
}

static int fc_jnumber(fc_jscan_t *p)
{
    size_t start = p->i;
    if (p->i < p->n && p->s[p->i] == '-') p->i++;
    if (p->i >= p->n) return 0;
    if (p->s[p->i] == '0') {
        p->i++;
        if (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') return 0;
    } else if (p->s[p->i] >= '1' && p->s[p->i] <= '9') {
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
    } else {
        return 0;
    }
    if (p->i < p->n && p->s[p->i] == '.') {
        p->i++;
        size_t fs = p->i;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
        if (p->i == fs) return 0;
    }
    if (p->i < p->n && (p->s[p->i] == 'e' || p->s[p->i] == 'E')) {
        p->i++;
        if (p->i < p->n && (p->s[p->i] == '+' || p->s[p->i] == '-')) p->i++;
        size_t es = p->i;
        while (p->i < p->n && p->s[p->i] >= '0' && p->s[p->i] <= '9') p->i++;
        if (p->i == es) return 0;
    }
    return p->i > start;
}

static int fc_jvalue(fc_jscan_t *p)
{
    fc_jws(p);
    if (p->i >= p->n) return 0;
    if (++p->depth > 256) return 0;
    char c = p->s[p->i];
    int ok = 0;
    if (c == '{') {
        p->i++;
        fc_jws(p);
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; ok = 1; }
        else {
            for (;;) {
                fc_jws(p);
                if (!fc_jstring(p)) break;
                fc_jws(p);
                if (p->i >= p->n || p->s[p->i] != ':') break;
                p->i++;
                if (!fc_jvalue(p)) break;
                fc_jws(p);
                if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
                if (p->i < p->n && p->s[p->i] == '}') { p->i++; ok = 1; }
                break;
            }
        }
    } else if (c == '[') {
        p->i++;
        fc_jws(p);
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; ok = 1; }
        else {
            for (;;) {
                if (!fc_jvalue(p)) break;
                fc_jws(p);
                if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
                if (p->i < p->n && p->s[p->i] == ']') { p->i++; ok = 1; }
                break;
            }
        }
    } else if (c == '"') {
        ok = fc_jstring(p);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        ok = fc_jnumber(p);
    } else if (p->n - p->i >= 4 && strncmp(p->s + p->i, "true", 4) == 0) {
        p->i += 4; ok = 1;
    } else if (p->n - p->i >= 5 && strncmp(p->s + p->i, "false", 5) == 0) {
        p->i += 5; ok = 1;
    } else if (p->n - p->i >= 4 && strncmp(p->s + p->i, "null", 4) == 0) {
        p->i += 4; ok = 1;
    }
    p->depth--;
    return ok;
}

static int fc_json_strict_ok(const char *s)
{
    fc_jscan_t p = { s, strlen(s), 0, 0 };
    if (!fc_jvalue(&p)) return 0;
    fc_jws(&p);
    return p.i == p.n;
}

/* ---- shared constants ---- */

static const char *FC_MARKER =
    "model is not supported when using codex with a chatgpt account";

static const char *FC_FMT_SWITCH =
    "Rate limit resets in %.0f s (< fallback.min_switch_reset_seconds=%.0f): "
    "staying on the primary";

static const char *FC_FMT_ARM =
    "Rate-limit backoff level %d: cooldown %d s (%.1f min, backoff#%d, %s)";

static const char *FC_FMT_MARK =
    "Model entitlement rejection: this account is not entitled to %s via %s; "
    "treating it as unavailable for this session";

static const char *FC_SIGNALS[] = {
    "try again", "retry", "resets at", "reset in", "resets in", "reset after",
    "available in", "wait", "requests remaining", "periodic", "window",
    "per minute", "per second",
};

static const char *FC_RESET_FIELDS[] = {
    "resets_in_seconds", "resets_at", "reset_at", "retry_after",
};

static const char *FC_RESET_HEADERS[] = {
    "retry-after", "Retry-After", "x-ratelimit-reset", "X-RateLimit-Reset",
};

static int fc_reason_in_set(const char *reason)
{
    return reason && (strcmp(reason, "rate_limit") == 0
                      || strcmp(reason, "billing") == 0
                      || strcmp(reason, "upstream_rate_limit") == 0);
}

/* `value in (None, "")` for the reset-field scans: absent reads as None. */
static int fc_is_none_or_empty(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 1;
    if (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) return 1;
    return 0;
}

/* ---- (provider, model) pair set helpers ---- */

static int fc_pair_in(const json_t *arr, const char *p, const char *m)
{
    if (!arr || arr->type != JSON_ARRAY) return 0;
    /* A None query element (NULL) never equals a string pair member: live
     * `(provider, None) in rejected` is False for the string-pair domain.
     * Guard BEFORE strcmp — the normalized-model probe passes fc_str(NULL). */
    if (!p || !m) return 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (!e || e->type != JSON_ARRAY || e->c.count != 2) continue;
        const char *ep = fc_str(e->c.items[0]);
        const char *em = fc_str(e->c.items[1]);
        if (ep && em && strcmp(ep, p) == 0 && strcmp(em, m) == 0) return 1;
    }
    return 0;
}

typedef struct { const char *p, *m; } fc_pair_t;

static int fc_pair_cmp(const void *a, const void *b)
{
    const fc_pair_t *x = (const fc_pair_t *)a, *y = (const fc_pair_t *)b;
    int c = strcmp(x->p, y->p);
    return c ? c : strcmp(x->m, y->m);
}

/* sorted [provider, model] array projection of the rejected set */
static json_t *fc_sorted_pairs(const json_t *arr)
{
    json_t *out = json_array();
    if (!arr || arr->type != JSON_ARRAY || arr->c.count == 0) return out;
    fc_pair_t *tmp = (fc_pair_t *)malloc(arr->c.count * sizeof(fc_pair_t));
    if (!tmp) return out;
    size_t n = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *e = arr->c.items[i];
        if (!e || e->type != JSON_ARRAY || e->c.count != 2) continue;
        const char *p = fc_str(e->c.items[0]);
        const char *m = fc_str(e->c.items[1]);
        if (!p || !m) continue;
        tmp[n].p = p; tmp[n].m = m; n++;
    }
    qsort(tmp, n, sizeof(fc_pair_t), fc_pair_cmp);
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(tmp[i].p));
        json_append(pair, json_string(tmp[i].m));
        json_append(out, pair);
    }
    free(tmp);
    return out;
}

/* ===========================================================================
 * agent/fallback_cooldown.py cores
 * =========================================================================== */

/* _provider_reset_delay: parsed = _parse_absolute_timestamp(reset_at) (seam,
 * in-function import); delay = parsed - time.time() only when parsed is not
 * None; return delay when finite and > 0, else None. */
static json_t *fc_provider_reset_delay_core(fc_io_t *io, const json_t *reset_at)
{
    json_t *a = fc_args();
    json_set(a, "reset_at", fc_copy_or_null(reset_at));
    json_t *parsed = fc_call(io, "parse_absolute_timestamp", a);
    if (fc_is_raise(parsed)) return parsed;
    if (!parsed) return NULL;
    json_t *t = fc_call(io, "time.time", fc_args());
    if (fc_is_raise(t)) { json_free(parsed); return t; }
    double delay = fc_num(parsed) - fc_num(t);
    json_free(parsed);
    json_free(t);
    if (isfinite(delay) && delay > 0.0) return json_number(delay);
    return NULL;
}

/* PoP: fc_provider_reset_delay @ agent/fallback_cooldown.py:_provider_reset_delay */
json_t *fc_provider_reset_delay(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return fc_provider_reset_delay_core(io, fc_get(in, "reset_at"));
}

/* ident (nested closure of _on_primary_route): the (provider, model, base_url)
 * identity triple. normalize_route_base_url is the claimed sibling seam. */
static json_t *fc_ident_core(fc_io_t *io, const json_t *provider,
                             const json_t *model, const json_t *base_url)
{
    char *ps = fc_pystr_or_empty(provider);
    char *p1 = fc_strip(ps);
    char *p = fc_lower(p1);
    free(ps); free(p1);
    char *ms = fc_pystr_or_empty(model);
    char *m = fc_strip(ms);
    free(ms);
    char *bs = fc_pystr_or_empty(base_url);
    json_t *a = fc_args();
    json_set(a, "base_url", json_string(bs ? bs : ""));
    free(bs);
    json_t *nb = fc_call(io, "normalize_route_base_url", a);
    if (fc_is_raise(nb)) { free(p); free(m); return nb; }
    json_t *out = json_array();
    json_append(out, json_string(p ? p : ""));
    json_append(out, json_string(m ? m : ""));
    json_append(out, nb ? nb : json_null());
    free(p); free(m);
    return out;
}

/* PoP: fc_ident @ agent/fallback_cooldown.py:ident */
json_t *fc_ident(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return fc_ident_core(io, fc_get(in, "provider"), fc_get(in, "model"),
                         fc_get(in, "base_url"));
}

/* _on_primary_route: rt = getattr(agent, "_primary_runtime", None) or {};
 * empty stripped rt provider -> False; else live ident == rt ident. */
static json_t *fc_on_primary_route_core(fc_io_t *io, const json_t *agent)
{
    const json_t *rt = fc_get(agent, "_primary_runtime");
    const json_t *rt_eff = fc_truth(rt) ? rt : NULL;
    char *ps = fc_pystr_or_empty(fc_get(rt_eff, "provider"));
    char *p = fc_strip(ps);
    free(ps);
    int empty = !p || !p[0];
    free(p);
    if (empty) return json_bool(0);
    json_t *live = fc_ident_core(io, fc_get(agent, "provider"),
                                 fc_get(agent, "model"), fc_get(agent, "base_url"));
    if (fc_is_raise(live)) return live;
    json_t *rtid = fc_ident_core(io, fc_get(rt_eff, "provider"),
                                 fc_get(rt_eff, "model"), fc_get(rt_eff, "base_url"));
    if (fc_is_raise(rtid)) { json_free(live); return rtid; }
    int eq = fc_pyequal(live, rtid);
    json_free(live);
    json_free(rtid);
    return json_bool(eq);
}

/* PoP: fc_on_primary_route @ agent/fallback_cooldown.py:_on_primary_route */
json_t *fc_on_primary_route(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return fc_on_primary_route_core(io, fc_get(in, "agent"));
}

/* switch_deferred_by_reset: the only `except Exception` site in the ported
 * surface wraps load_config + the threshold coercion; BaseException kinds
 * pass through it. _provider_reset_delay and logging.info sit OUTSIDE the
 * try, so their raises propagate unconditionally. */
/* PoP: fc_switch_deferred_by_reset @ agent/fallback_cooldown.py:switch_deferred_by_reset */
json_t *fc_switch_deferred_by_reset(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const char *reason = fc_str(fc_get(in, "reason"));
    const json_t *agent = fc_get(in, "agent");
    if (!fc_reason_in_set(reason) || fc_truth(fc_get(agent, "_fallback_activated")))
        return json_bool(0);
    /* try: threshold = float((load_config() or {}).get("fallback", {})
     *                          .get("min_switch_reset_seconds") or 0) */
    json_t *cfg = fc_call(io, "load_config", fc_args());
    if (fc_is_raise(cfg)) {
        if (fc_is_base(fc_kind(cfg))) return cfg;
        json_free(cfg);
        return json_bool(0);
    }
    double threshold = 0.0;
    int failed = 0;
    const json_t *cfg_eff = fc_truth(cfg) ? cfg : NULL;
    if (cfg_eff && cfg_eff->type != JSON_OBJECT) {
        /* truthy non-dict: .get raises AttributeError, caught -> False */
        failed = 1;
    } else {
        const json_t *fb = fc_get(cfg_eff, "fallback");
        if (fb && fb->type != JSON_OBJECT) {
            /* present-non-dict fallback (incl. null): .get raises -> False */
            failed = 1;
        } else {
            const json_t *ms = fc_get(fb, "min_switch_reset_seconds");
            if (fc_truth(ms)) {
                json_t *ferr = NULL;
                threshold = fc_float_value(ms, &ferr);
                if (ferr) { json_free(ferr); failed = 1; }
            }
        }
    }
    json_free(cfg);
    if (failed) return json_bool(0);
    if (threshold <= 0.0) return json_bool(0);
    json_t *delay = fc_provider_reset_delay_core(io, fc_get(in, "reset_at"));
    if (fc_is_raise(delay)) return delay;
    if (!delay || fc_num(delay) >= threshold) {
        json_free(delay);
        return json_bool(0);
    }
    json_t *args = json_array();
    json_append(args, json_copy(delay));
    json_append(args, json_number(threshold));
    json_free(delay);
    json_t *r = fc_log(io, "logging.info", FC_FMT_SWITCH, args);
    if (fc_is_raise(r)) return r;
    json_free(r);
    return json_bool(1);
}

/* _arm_rate_limit_cooldown: arm only when leaving the primary; the backoff
 * count increments BEFORE the provider-delay probe; monotonic + duration. */
/* PoP: fc_arm_rate_limit_cooldown @ agent/fallback_cooldown.py:_arm_rate_limit_cooldown */
json_t *fc_arm_rate_limit_cooldown(fc_io_t *io, const json_t *in, json_t *st)
{
    const char *reason = fc_str(fc_get(in, "reason"));
    const json_t *agent_in = fc_get(in, "agent");
    json_t *agent = (agent_in && agent_in->type == JSON_OBJECT)
                        ? json_copy(agent_in) : json_object();
    if (!fc_reason_in_set(reason)) {
        const json_t *bc0 = fc_get(agent, "_rate_limit_backoff_count");
        json_set(st, "_rate_limit_backoff_count", bc0 ? json_copy(bc0) : json_number(0));
        const json_t *until = fc_get(agent, "_rate_limited_until");
        json_set(st, "_rate_limited_until", until ? json_copy(until) : json_null());
        json_free(agent);
        return NULL;
    }
    if (fc_truth(fc_get(agent, "_fallback_activated"))) {
        json_t *onp = fc_on_primary_route_core(io, agent);
        if (fc_is_raise(onp)) { json_free(agent); return onp; }
        int on = fc_truth(onp);
        json_free(onp);
        if (!on) {
            const json_t *bc0 = fc_get(agent, "_rate_limit_backoff_count");
            json_set(st, "_rate_limit_backoff_count", bc0 ? json_copy(bc0) : json_number(0));
            const json_t *until = fc_get(agent, "_rate_limited_until");
            json_set(st, "_rate_limited_until", until ? json_copy(until) : json_null());
            json_free(agent);
            return NULL;
        }
    }
    const json_t *bc_node = fc_get(agent, "_rate_limit_backoff_count");
    if (bc_node && !fc_is_num(bc_node)) {
        /* live backoff_count + 1 raises TypeError on a non-numeric carrier
         * (present-null included: getattr returns None, None + 1 raises).
         * str/list use the concatenation message; others the operand message
         * (probed on the quarry CPython 3.14.9). */
        const char *tn = fc_typename(bc_node);
        char *m;
        if (strcmp(tn, "str") == 0 || strcmp(tn, "list") == 0)
            m = fc_xfmt("can only concatenate %s (not \"int\") to %s", tn, tn);
        else
            m = fc_xfmt("unsupported operand type(s) for +: '%s' and 'int'", tn);
        json_t *err = fc_mkraise("TypeError", m ? m : "unsupported operand type(s) for +");
        free(m);
        json_free(agent);
        return err;
    }
    double bc = fc_num(bc_node);
    json_set(agent, "_rate_limit_backoff_count", json_number(bc + 1.0));
    json_t *pd = fc_provider_reset_delay_core(io, fc_get(in, "reset_at"));
    if (fc_is_raise(pd)) { json_free(agent); return pd; }
    double bs;
    const char *source;
    if (pd) {
        bs = ceil(fc_num(pd));
        source = "provider reset";
    } else {
        bs = 60.0 * pow(2.0, bc);
        if (bs > 14400.0) bs = 14400.0;   /* min(..., 14400): cap only when greater */
        source = "exponential fallback";
    }
    json_free(pd);
    json_t *mono = fc_call(io, "time.monotonic", fc_args());
    if (fc_is_raise(mono)) { json_free(agent); return mono; }
    double until = fc_num(mono) + bs;
    json_free(mono);
    json_set(agent, "_rate_limited_until", json_number(until));
    json_t *args = json_array();
    json_append(args, json_number(bc));
    json_append(args, json_number(bs));
    json_append(args, json_number(bs / 60.0));
    json_append(args, json_number(bc + 1.0));
    json_append(args, json_string(source));
    json_t *r = fc_log(io, "logging.info", FC_FMT_ARM, args);
    if (fc_is_raise(r)) { json_free(agent); return r; }
    json_free(r);
    json_set(st, "_rate_limit_backoff_count", json_number(bc + 1.0));
    json_set(st, "_rate_limited_until", json_number(until));
    json_free(agent);
    return json_number(bs);
}

/* _mark_entitlement_rejected_model: Codex ChatGPT-account 400 marker; pool
 * rotation owns the pair while an entry is still available; the rejected set
 * is created lazily and echoed sorted. */
/* PoP: fc_mark_entitlement_rejected_model @ agent/fallback_cooldown.py:_mark_entitlement_rejected_model */
json_t *fc_mark_entitlement_rejected_model(fc_io_t *io, const json_t *in, json_t *st)
{
    const json_t *err = fc_get(in, "api_error");
    const json_t *sc = fc_get(err, "status_code");
    if (!(fc_is_num(sc) && fc_num(sc) == 400.0)) return json_bool(0);
    const json_t *msg = fc_get(err, "message");
    char *hs;
    if (fc_truth(msg)) {
        hs = fc_pystr(msg);
    } else {
        /* str(api_error): the double's explicit str projection */
        const json_t *sp = fc_get(err, "str");
        hs = fc_pystr_or_empty(sp);
    }
    char *hay = fc_lower(hs);
    free(hs);
    int hit = hay && strstr(hay, FC_MARKER) != NULL;
    free(hay);
    if (!hit) return json_bool(0);
    const json_t *agent_in = fc_get(in, "agent");
    json_t *agent = (agent_in && agent_in->type == JSON_OBJECT)
                        ? json_copy(agent_in) : json_object();
    char *ps = fc_pystr_or_empty(fc_get(agent, "provider"));
    char *p1 = fc_strip(ps);
    char *provider = fc_lower(p1);
    free(ps); free(p1);
    char *ms = fc_pystr_or_empty(fc_get(agent, "model"));
    char *model = fc_strip(ms);
    free(ms);
    if (!provider || !provider[0] || !model || !model[0]) {
        free(provider); free(model);
        json_free(agent);
        return json_bool(0);
    }
    const json_t *pool = fc_get(agent, "_credential_pool");
    if (fc_present(pool)) {
        json_t *a = fc_args();
        json_set(a, "model", json_string(model));
        json_t *r = fc_call(io, "pool.has_available", a);
        if (fc_is_raise(r)) { free(provider); free(model); json_free(agent); return r; }
        int avail = fc_truth(r);
        json_free(r);
        if (avail) {
            free(provider); free(model);
            json_free(agent);
            return json_bool(0);
        }
    }
    json_t *rejected = fc_get(agent, "_entitlement_rejected_models");
    if (!rejected || rejected->type == JSON_NULL) {
        rejected = json_array();
        json_set(agent, "_entitlement_rejected_models", rejected);
    }
    if (fc_pair_in(rejected, provider, model)) {
        json_set(st, "_entitlement_rejected_models", fc_sorted_pairs(rejected));
        free(provider); free(model);
        json_free(agent);
        return json_bool(1);
    }
    json_t *pair = json_array();
    json_append(pair, json_string(provider));
    json_append(pair, json_string(model));
    json_append(rejected, pair);
    json_t *args = json_array();
    json_append(args, json_string(model));
    json_append(args, json_string(provider));
    json_t *r = fc_log(io, "logger.warning", FC_FMT_MARK, args);
    if (fc_is_raise(r)) { free(provider); free(model); json_free(agent); return r; }
    json_free(r);
    char *text = fc_xfmt(
        "\xf0\x9f\x9a\xab This account is not entitled to %s via %s; it will be "
        "skipped until restart. Switch to an entitled model via /model or "
        "`hermes model`.", model, provider);
    json_t *a = fc_args();
    json_set(a, "text", json_string(text ? text : ""));
    free(text);
    r = fc_call(io, "buffer_diagnostic_status", a);
    if (fc_is_raise(r)) { free(provider); free(model); json_free(agent); return r; }
    json_free(r);
    json_set(st, "_entitlement_rejected_models", fc_sorted_pairs(rejected));
    free(provider); free(model);
    json_free(agent);
    return json_bool(1);
}

/* _is_entitlement_rejected: direct pair hit, else the normalized-model pair
 * (normalize_model_for_provider is the claimed sibling seam). */
/* PoP: fc_is_entitlement_rejected @ agent/fallback_cooldown.py:_is_entitlement_rejected */
json_t *fc_is_entitlement_rejected(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *agent = fc_get(in, "agent");
    const json_t *rejected = fc_get(agent, "_entitlement_rejected_models");
    if (!fc_truth(rejected)) return json_bool(0);
    const char *provider = fc_str(fc_get(in, "provider"));
    const char *model = fc_str(fc_get(in, "model"));
    if (!provider) provider = "";
    if (!model) model = "";
    if (fc_pair_in(rejected, provider, model)) return json_bool(1);
    json_t *a = fc_args();
    json_set(a, "model", json_string(model));
    json_set(a, "provider", json_string(provider));
    json_t *nm = fc_call(io, "normalize_model_for_provider", a);
    if (fc_is_raise(nm)) return nm;
    json_t *pair = json_array();
    json_append(pair, json_string(provider));
    json_append(pair, nm ? nm : json_null());
    int hit = fc_pair_in(rejected, provider, fc_str(pair->c.items[1]));
    json_free(pair);
    return json_bool(hit);
}

/* ===========================================================================
 * hermes_cli/route_identity.py leaves
 * =========================================================================== */

/* provider_owns_route: True/False/None ownership verdict. The `or` between
 * resolve_user_provider and resolve_custom_provider is TRUTHINESS (scripts
 * restricted to null/non-empty dict); the pdef checks are `is not None`. */
static json_t *fc_provider_owns_route_core(fc_io_t *io, const json_t *provider,
                                           const json_t *base_url, const json_t *config)
{
    char *bu = fc_pystr_or_empty(base_url);
    json_t *a = fc_args();
    json_set(a, "url", json_string(bu ? bu : ""));
    free(bu);
    json_t *host_r = fc_call(io, "base_url_hostname", a);
    if (fc_is_raise(host_r)) return host_r;
    if (!fc_truth(host_r)) { json_free(host_r); return NULL; }
    char *host = fc_pystr(host_r);
    json_free(host_r);
    char *ps = fc_pystr_or_empty(provider);
    char *p1 = fc_strip(ps);
    char *raw = fc_lower(p1);
    free(ps); free(p1);
    a = fc_args();
    json_set(a, "provider", json_string(raw ? raw : ""));
    json_t *canon_r = fc_call(io, "normalize_provider", a);
    if (fc_is_raise(canon_r)) { free(host); free(raw); return canon_r; }
    char *canon = fc_pystr(canon_r);
    json_free(canon_r);
    if (canon && (strcmp(canon, "custom") == 0 || strcmp(canon, "local") == 0)) {
        free(host); free(raw); free(canon);
        return json_bool(1);
    }
    const json_t *cfg = (config && config->type == JSON_OBJECT) ? config : NULL;
    const json_t *providers_cfg = fc_get(cfg, "providers");
    a = fc_args();
    json_set(a, "provider", json_string(raw ? raw : ""));
    json_set(a, "providers", fc_truth(providers_cfg) ? json_copy(providers_cfg) : json_object());
    json_t *user_r = fc_call(io, "resolve_user_provider", a);
    if (fc_is_raise(user_r)) { free(host); free(raw); free(canon); return user_r; }
    json_t *user_pdef = user_r;
    if (!fc_truth(user_pdef)) {
        json_free(user_pdef);
        const json_t *cp = fc_get(cfg, "custom_providers");
        a = fc_args();
        json_set(a, "provider", json_string(raw ? raw : ""));
        if (fc_present(cp)) json_set(a, "custom_providers", json_copy(cp));
        user_pdef = fc_call(io, "resolve_custom_provider", a);
        if (fc_is_raise(user_pdef)) { free(host); free(raw); free(canon); return user_pdef; }
    }
    if (fc_present(user_pdef)) {
        const json_t *ub = fc_get(user_pdef, "base_url");
        if (fc_truth(ub)) {
            char *ubs = fc_pystr(ub);
            a = fc_args();
            json_set(a, "url", json_string(ubs ? ubs : ""));
            free(ubs);
            json_t *h2 = fc_call(io, "base_url_hostname", a);
            if (fc_is_raise(h2)) { free(host); free(raw); free(canon); json_free(user_pdef); return h2; }
            char *h2s = fc_pystr(h2);
            json_free(h2);
            int eq = h2s && strcmp(h2s, host) == 0;
            free(h2s);
            free(host); free(raw); free(canon);
            json_free(user_pdef);
            return json_bool(eq);
        }
    }
    a = fc_args();
    json_set(a, "provider", json_string(canon ? canon : ""));
    json_set(a, "allow_network", json_bool(0));
    json_t *pdef = fc_call(io, "get_provider", a);
    if (fc_is_raise(pdef)) { free(host); free(raw); free(canon); json_free(user_pdef); return pdef; }
    if (fc_present(pdef)) {
        const json_t *pb = fc_get(pdef, "base_url");
        if (fc_truth(pb)) {
            char *pbs = fc_pystr(pb);
            a = fc_args();
            json_set(a, "url", json_string(pbs ? pbs : ""));
            free(pbs);
            json_t *h3 = fc_call(io, "base_url_hostname", a);
            if (fc_is_raise(h3)) { free(host); free(raw); free(canon); json_free(user_pdef); json_free(pdef); return h3; }
            char *h3s = fc_pystr(h3);
            json_free(h3);
            int eq = h3s && strcmp(h3s, host) == 0;
            free(h3s);
            if (eq) {
                free(host); free(raw); free(canon);
                json_free(user_pdef); json_free(pdef);
                return json_bool(1);
            }
        }
    }
    if (!fc_present(pdef) && !fc_present(user_pdef)) {
        free(host); free(raw); free(canon);
        json_free(user_pdef); json_free(pdef);
        return json_bool(1);
    }
    char *bus = fc_pystr(base_url);
    a = fc_args();
    json_set(a, "base_url", json_string(bus ? bus : ""));
    free(bus);
    json_t *inf_r = fc_call(io, "infer_provider_from_url", a);
    if (fc_is_raise(inf_r)) { free(host); free(raw); free(canon); json_free(user_pdef); json_free(pdef); return inf_r; }
    if (!inf_r) {
        free(host); free(raw); free(canon);
        json_free(user_pdef); json_free(pdef);
        return NULL;
    }
    char *infs = fc_pystr(inf_r);
    json_free(inf_r);
    a = fc_args();
    json_set(a, "provider", json_string(infs ? infs : ""));
    free(infs);
    json_t *canon2_r = fc_call(io, "normalize_provider", a);
    if (fc_is_raise(canon2_r)) { free(host); free(raw); free(canon); json_free(user_pdef); json_free(pdef); return canon2_r; }
    char *canon2 = fc_pystr(canon2_r);
    json_free(canon2_r);
    int eq = canon && canon2 && strcmp(canon2, canon) == 0;
    free(host); free(raw); free(canon); free(canon2);
    json_free(user_pdef); json_free(pdef);
    return json_bool(eq);
}

/* PoP: fc_provider_owns_route @ hermes_cli/route_identity.py:provider_owns_route */
json_t *fc_provider_owns_route(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    return fc_provider_owns_route_core(io, fc_get(in, "provider"),
                                       fc_get(in, "base_url"), fc_get(in, "config"));
}

/* drop_stale_model_route: (popped {key: old value}, unverified) tuple; the
 * owned/None/False verdict gates popping base_url + api_mode in order. */
/* PoP: fc_drop_stale_model_route @ hermes_cli/route_identity.py:drop_stale_model_route */
json_t *fc_drop_stale_model_route(fc_io_t *io, const json_t *in, json_t *st)
{
    const json_t *mc = fc_get(in, "model_cfg");
    if (!mc || mc->type != JSON_OBJECT) {
        json_set(st, "model_cfg", mc ? json_copy(mc) : json_null());
        json_t *out = json_array();
        json_append(out, json_object());
        json_append(out, json_bool(0));
        return out;
    }
    json_t *carrier = json_copy(mc);
    char *bus = fc_pystr_or_empty(fc_get(carrier, "base_url"));
    char *bu = fc_strip(bus);
    free(bus);
    json_t *owned;
    if (bu && bu[0]) {
        json_t *bun = json_string(bu);
        owned = fc_provider_owns_route_core(io, fc_get(in, "provider"), bun,
                                            fc_get(in, "config"));
        json_free(bun);
    } else {
        owned = json_bool(0);
    }
    free(bu);
    if (fc_is_raise(owned)) { json_free(carrier); return owned; }
    json_t *out = json_array();
    if (fc_truth(owned)) {
        json_append(out, json_object());
        json_append(out, json_bool(0));
    } else if (!fc_present(owned)) {
        json_append(out, json_object());
        json_append(out, json_bool(1));
    } else {
        json_t *popped = json_object();
        const char *keys[2] = { "base_url", "api_mode" };
        for (int k = 0; k < 2; k++) {
            json_t *v = fc_get(carrier, keys[k]);
            if (v && !fc_is_none_or_empty(v)) {
                json_set(popped, keys[k], json_copy(v));
                json_obj_del(carrier, keys[k]);
            }
        }
        json_append(out, popped);
        json_append(out, json_bool(0));
    }
    json_free(owned);
    json_set(st, "model_cfg", carrier);
    return out;
}

/* drop_credentials_of_other_route: same-route test (provider strip/lower +
 * two normalize_route_base_url seam calls, prior first); a route change fires
 * clear_model_endpoint_credentials whose live effect (pop the four credential
 * keys; clear_api_mode=False at this call site) the port applies after the
 * seam returns non-raise. The oracle stub applies the identical effect. */
/* PoP: fc_drop_credentials_of_other_route @ hermes_cli/route_identity.py:drop_credentials_of_other_route */
json_t *fc_drop_credentials_of_other_route(fc_io_t *io, const json_t *in, json_t *st)
{
    const json_t *mc = fc_get(in, "model_cfg");
    json_t *carrier = (mc && mc->type == JSON_OBJECT) ? json_copy(mc) : json_object();
    const json_t *prior_in = fc_get(in, "prior_model_cfg");
    const json_t *prior = (prior_in && prior_in->type == JSON_OBJECT) ? prior_in : NULL;
    char *pps = fc_pystr_or_empty(fc_get(prior, "provider"));
    char *pp1 = fc_strip(pps);
    char *pp = fc_lower(pp1);
    free(pps); free(pp1);
    char *mps = fc_pystr_or_empty(fc_get(carrier, "provider"));
    char *mp1 = fc_strip(mps);
    char *mp = fc_lower(mp1);
    free(mps); free(mp1);
    int same = pp && mp && strcmp(pp, mp) == 0;
    free(pp); free(mp);
    if (same) {
        json_t *a = fc_args();
        const json_t *pb = fc_get(prior, "base_url");
        if (fc_present(pb)) json_set(a, "base_url", json_copy(pb));
        json_t *n1 = fc_call(io, "normalize_route_base_url", a);
        if (fc_is_raise(n1)) { json_free(carrier); return n1; }
        a = fc_args();
        const json_t *cb = fc_get(carrier, "base_url");
        if (fc_present(cb)) json_set(a, "base_url", json_copy(cb));
        json_t *n2 = fc_call(io, "normalize_route_base_url", a);
        if (fc_is_raise(n2)) { json_free(n1); json_free(carrier); return n2; }
        same = fc_pyequal(n1, n2);
        json_free(n1); json_free(n2);
    }
    if (!same) {
        json_t *a = fc_args();
        json_set(a, "clear_api_mode", json_bool(0));
        json_t *r = fc_call(io, "clear_model_endpoint_credentials", a);
        if (fc_is_raise(r)) { json_free(carrier); return r; }
        json_free(r);
        json_obj_del(carrier, "api_key");
        json_obj_del(carrier, "api");
        json_obj_del(carrier, "key_env");
        json_obj_del(carrier, "api_key_env");
    }
    json_set(st, "model_cfg", json_copy(carrier));
    return carrier;
}

/* ===========================================================================
 * agent/error_classifier.py leaves
 * =========================================================================== */

/* _error_obj: body["error"] when it is a dict, else {}. */
static json_t *fc_error_obj_core(const json_t *body)
{
    const json_t *err = fc_get(body, "error");
    if (err && err->type == JSON_OBJECT) return json_copy(err);
    return json_object();
}

/* PoP: fc_error_obj @ agent/error_classifier.py:_error_obj */
json_t *fc_error_obj(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    return fc_error_obj_core(fc_get(in, "body"));
}

/* _json_dict: strict json.loads object parse; None for non-strings, blanks,
 * invalid JSON or non-objects. */
/* PoP: fc_json_dict @ agent/error_classifier.py:_json_dict */
json_t *fc_json_dict(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *text = fc_get(in, "text");
    const char *s = fc_str(text);
    if (!s) return NULL;
    char *stripped = fc_strip(s);
    int blank = !stripped || !stripped[0];
    free(stripped);
    if (blank) return NULL;
    if (!fc_json_strict_ok(s)) return NULL;
    json_t *node = json_parse(s, NULL);
    if (node && node->type == JSON_OBJECT) return node;
    json_free(node);
    return NULL;
}

/* _usage_window_display_message: trimmed, 500-capped display_message from the
 * top level or under error; "" when absent. */
/* PoP: fc_usage_window_display_message @ agent/error_classifier.py:_usage_window_display_message */
json_t *fc_usage_window_display_message(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *body = fc_get(in, "body");
    if (!body || body->type != JSON_OBJECT) return json_string("");
    const json_t *holders[2] = { body, fc_get(body, "error") };
    for (int h = 0; h < 2; h++) {
        const json_t *holder = holders[h];
        if (!holder || holder->type != JSON_OBJECT) continue;
        const json_t *v = fc_get(holder, "display_message");
        const char *vs = fc_str(v);
        if (!vs) continue;
        char *trimmed = fc_strip(vs);
        if (trimmed && trimmed[0]) {
            size_t n = strlen(trimmed);
            if (n > 500) n = 500;
            char *cap = (char *)malloc(n + 1);
            if (cap) { memcpy(cap, trimmed, n); cap[n] = '\0'; }
            free(trimmed);
            json_t *out = json_string(cap ? cap : "");
            free(cap);
            return out;
        }
        free(trimmed);
    }
    return json_string("");
}

/* _has_usage_limit_transient_signal: message substrings, then reset fields on
 * body + _error_obj(body) (eager tuple), then headers (truthy dict only). */
/* PoP: fc_has_usage_limit_transient_signal @ agent/error_classifier.py:_has_usage_limit_transient_signal */
json_t *fc_has_usage_limit_transient_signal(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const char *msg = fc_str(fc_get(in, "error_msg"));
    if (!msg) msg = "";
    for (size_t i = 0; i < sizeof(FC_SIGNALS) / sizeof(FC_SIGNALS[0]); i++)
        if (strstr(msg, FC_SIGNALS[i])) return json_bool(1);
    const json_t *body = fc_get(in, "body");
    json_t *eobj = fc_error_obj_core(body);
    const json_t *payloads[2] = {
        (body && body->type == JSON_OBJECT) ? body : NULL, eobj
    };
    for (int p = 0; p < 2; p++) {
        if (!payloads[p]) continue;
        for (size_t f = 0; f < sizeof(FC_RESET_FIELDS) / sizeof(FC_RESET_FIELDS[0]); f++) {
            json_t *v = fc_get(payloads[p], FC_RESET_FIELDS[f]);
            if (v && !fc_is_none_or_empty(v)) { json_free(eobj); return json_bool(1); }
        }
    }
    json_free(eobj);
    const json_t *headers = fc_get(in, "response_headers");
    if (fc_truth(headers) && headers->type == JSON_OBJECT) {
        for (size_t h = 0; h < sizeof(FC_RESET_HEADERS) / sizeof(FC_RESET_HEADERS[0]); h++) {
            json_t *v = fc_get(headers, FC_RESET_HEADERS[h]);
            if (v && !fc_is_none_or_empty(v)) return json_bool(1);
        }
    }
    return json_bool(0);
}

/* _rate_limit_reset_seconds: reset fields on body + _error_obj(body) (eager),
 * "_at" numeric fields convert through time.time(); other values ride
 * parse_retry_after_seconds; then the headers mapping; then the message
 * grammar. First non-None wins. */
/* PoP: fc_rate_limit_reset_seconds @ agent/error_classifier.py:_rate_limit_reset_seconds */
json_t *fc_rate_limit_reset_seconds(fc_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    const json_t *body = fc_get(in, "body");
    json_t *eobj = fc_error_obj_core(body);
    const json_t *payloads[2] = {
        (body && body->type == JSON_OBJECT) ? body : NULL, eobj
    };
    for (int p = 0; p < 2; p++) {
        if (!payloads[p]) continue;
        for (size_t f = 0; f < sizeof(FC_RESET_FIELDS) / sizeof(FC_RESET_FIELDS[0]); f++) {
            const char *name = FC_RESET_FIELDS[f];
            json_t *v = fc_get(payloads[p], name);
            if (fc_is_none_or_empty(v)) continue;
            size_t nl = strlen(name);
            if (nl >= 3 && strcmp(name + nl - 3, "_at") == 0 && fc_is_num(v)) {
                json_t *t = fc_call(io, "time.time", fc_args());
                if (fc_is_raise(t)) { json_free(eobj); return t; }
                double d = fc_num(v) - fc_num(t);
                json_free(t);
                if (d < 0.0) d = 0.0;
                json_free(eobj);
                return json_number(d);
            }
            json_t *a = fc_args();
            json_set(a, "value", json_copy(v));
            json_t *sec = fc_call(io, "parse_retry_after_seconds", a);
            if (fc_is_raise(sec)) { json_free(eobj); return sec; }
            if (sec) { json_free(eobj); return sec; }
            json_free(sec);
        }
    }
    json_free(eobj);
    json_t *a = fc_args();
    const json_t *headers = fc_get(in, "response_headers");
    if (fc_present(headers)) json_set(a, "value", json_copy(headers));
    json_t *sec = fc_call(io, "parse_retry_after_seconds", a);
    if (fc_is_raise(sec)) return sec;
    if (sec) return sec;
    json_free(sec);
    a = fc_args();
    const json_t *em = fc_get(in, "error_msg");
    if (fc_present(em)) json_set(a, "message", json_copy(em));
    return fc_call(io, "reset_delay_from_message", a);
}
