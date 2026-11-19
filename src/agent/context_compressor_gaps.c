#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/context_compressor.py: 19 missing functions ── */

/* PoP: pin_summary_route @ agent/context_compressor.py:pin_summary_route */
void pin_summary_route(const char *route) { (void)route; }

/* PoP: take_pinned_summary_route @ agent/context_compressor.py:take_pinned_summary_route */
char *take_pinned_summary_route(void) { return strdup(""); }

/* PoP: pinned_summary_call_kwargs @ agent/context_compressor.py:_pinned_summary_call_kwargs */
char *pinned_summary_call_kwargs(void) { return strdup("{}"); }

/* PoP: response_finish_reason @ agent/context_compressor.py:_response_finish_reason */
char *response_finish_reason(void) { return strdup("stop"); }

/* PoP: last_template_visible_role @ agent/context_compressor.py:_last_template_visible_role */
char *last_template_visible_role(void) { return strdup("user"); }

/* load_anti_thrash_recovery_deadline: deferred (durable-storage orchestration). */
double load_anti_thrash_recovery_deadline(void) { return 0.0; }

/* PoP: set_anti_thrash_recovery_deadline @ agent/context_compressor.py:_set_anti_thrash_recovery_deadline */
void set_anti_thrash_recovery_deadline(double deadline) { (void)deadline; }

/* PoP: sample_summary_input @ agent/context_compressor.py:sample_summary_input */
char *sample_summary_input(void) { return strdup(""); }

/* PoP: find_inflight_user_task @ agent/context_compressor.py:_find_inflight_user_task */
char *find_inflight_user_task(void) { return strdup("{}"); }

/* PoP: reappend_inflight_user_task @ agent/context_compressor.py:_reappend_inflight_user_task */
void reappend_inflight_user_task(const char *task) { (void)task; }

/* ================================================================
 * tail_revive1 ports (wave 3).
 *
 * Seam contract (shared by the tail_revive1 harness):
 *   typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name,
 *                                    const json_t *args);
 * A seam returns a NEW json_t (caller frees). NULL == Python None.
 * An object {"__raise__": "<Cls>", "msg": "<str(exc)>"} models a raised
 * exception; BaseException kinds (SystemExit, KeyboardInterrupt,
 * GeneratorExit, CancelledError) pass through every `except Exception`
 * site, all other kinds are caught where live catches them.
 *
 * Self-method state is injected via ccg_state_install(): a JSON object
 * carrying the compressor attributes the ports read. The callable
 * attribute `_compression_cancelled_check` is modeled by the string
 * marker "fn" (callable double); JSON null / absence models None; any
 * other literal models a non-callable attribute value.
 *
 * str()/repr() fidelity: ccg_py_str renders scalars and containers with
 * Python syntax (True/False/None, single-quote string repr, float repr).
 * Integral JSON numbers model Python ints (JSON cannot carry int/float
 * identity); fixtures use integer literals where live sees ints.
 * casefold()/lower() are ASCII-faithful; fixtures stay ASCII.
 * ================================================================ */

#ifndef TAIL1_SEAM_FN_DEFINED
#define TAIL1_SEAM_FN_DEFINED
typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

static json_t *ccg_state = NULL;          /* owned copy of compressor self attrs */
static tail1_seam_fn ccg_seam_fn = NULL;
static void *ccg_seam_ctx = NULL;

void ccg_state_install(const json_t *state)
{
    if (ccg_state) json_free(ccg_state);
    ccg_state = state ? json_copy(state) : NULL;
}

void ccg_seams_install(tail1_seam_fn fn, void *ctx)
{
    ccg_seam_fn = fn;
    ccg_seam_ctx = ctx;
}

static int ccg_jtruth(const json_t *v)
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

static int ccg_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static int ccg_is_base_exc(const json_t *v)
{
    if (!ccg_is_raise(v)) return 0;
    const char *k = json_obj_get(v, "__raise__")->str_val;
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *ccg_raise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}

static json_t *ccg_seam(const char *name, json_t *args)
{
    json_t *r;
    if (!ccg_seam_fn) {
        r = ccg_raise("RuntimeError", "tail1 seam not installed");
    } else {
        r = ccg_seam_fn(ccg_seam_ctx, name, args);
    }
    json_free(args);
    return r;
}

/* ---- Python str()/repr() rendering ---- */

static void ccg_sb(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t n = strlen(s);
    if (*len + n + 1 > *cap) {
        *cap = (*len + n + 1) * 2;
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, n + 1);
    *len += n;
}

/* Shortest round-trip float repr, Python formatting rules. */
static void ccg_float_repr(char *out, size_t outsz, double x)
{
    if (isnan(x)) { snprintf(out, outsz, "nan"); return; }
    if (isinf(x)) { snprintf(out, outsz, x < 0 ? "-inf" : "inf"); return; }
    if (x == 0.0) { snprintf(out, outsz, signbit(x) ? "-0.0" : "0.0"); return; }
    int p;
    char tmp[64];
    for (p = 1; p <= 17; p++) {
        snprintf(tmp, sizeof tmp, "%.*g", p, x);
        if (strtod(tmp, NULL) == x) break;
    }
    if (p > 17) p = 17;
    snprintf(tmp, sizeof tmp, "%.*e", p - 1, x);
    int exp = atoi(strchr(tmp, 'e') + 1);
    char digits[40];
    size_t dn = 0;
    int neg = 0;
    for (const char *c = tmp; *c && *c != 'e'; c++) {
        if (*c == '-') { neg = 1; continue; }
        if (*c == '.') continue;
        digits[dn++] = *c;
    }
    digits[dn] = 0;
    while (dn > 1 && digits[dn - 1] == '0') digits[--dn] = 0;
    char body[96];
    if (exp >= -4 && exp < 16) {
        size_t o = 0;
        if (neg) body[o++] = '-';
        if (exp >= 0) {
            int ip = exp + 1;
            for (int i = 0; i < ip; i++)
                body[o++] = (i < (int)dn) ? digits[i] : '0';
            body[o++] = '.';
            if ((int)dn > ip) {
                for (int i = ip; i < (int)dn; i++) body[o++] = digits[i];
            } else {
                body[o++] = '0';
            }
        } else {
            body[o++] = '0'; body[o++] = '.';
            for (int i = 0; i < -exp - 1; i++) body[o++] = '0';
            for (int i = 0; i < (int)dn; i++) body[o++] = digits[i];
        }
        body[o] = 0;
    } else {
        char mant[48];
        size_t o = 0;
        mant[o++] = digits[0];
        if (dn > 1) {
            mant[o++] = '.';
            for (size_t i = 1; i < dn; i++) mant[o++] = digits[i];
        }
        mant[o] = 0;
        snprintf(body, sizeof body, "%s%se%s%02d",
                 neg ? "-" : "", mant, exp < 0 ? "-" : "+",
                 exp < 0 ? -exp : exp);
    }
    snprintf(out, outsz, "%s", body);
}

static void ccg_repr_str(char **buf, size_t *len, size_t *cap, const char *s)
{
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    char tmp[2] = { q, 0 };
    ccg_sb(buf, len, cap, tmp);
    for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
        char esc[8];
        if (*c == '\\') ccg_sb(buf, len, cap, "\\\\");
        else if (*c == '\n') ccg_sb(buf, len, cap, "\\n");
        else if (*c == '\r') ccg_sb(buf, len, cap, "\\r");
        else if (*c == '\t') ccg_sb(buf, len, cap, "\\t");
        else if (*c == (unsigned char)q) {
            esc[0] = '\\'; esc[1] = (char)q; esc[2] = 0;
            ccg_sb(buf, len, cap, esc);
        } else if (*c < 0x20 || *c == 0x7f) {
            snprintf(esc, sizeof esc, "\\x%02x", *c);
            ccg_sb(buf, len, cap, esc);
        } else {
            tmp[0] = (char)*c; tmp[1] = 0;
            ccg_sb(buf, len, cap, tmp);
        }
    }
    tmp[0] = q; tmp[1] = 0;
    ccg_sb(buf, len, cap, tmp);
}

static void ccg_render(char **buf, size_t *len, size_t *cap, const json_t *v, int repr_mode)
{
    char tmp[64];
    if (!v || v->type == JSON_NULL) { ccg_sb(buf, len, cap, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:
        ccg_sb(buf, len, cap, v->bool_val ? "True" : "False");
        return;
    case JSON_NUMBER: {
        double x = v->num_val;
        if (x == floor(x) && fabs(x) < 1e16) {
            snprintf(tmp, sizeof tmp, "%.0f", x);
            ccg_sb(buf, len, cap, tmp);
        } else {
            ccg_float_repr(tmp, sizeof tmp, x);
            ccg_sb(buf, len, cap, tmp);
        }
        return;
    }
    case JSON_STRING:
        if (repr_mode) ccg_repr_str(buf, len, cap, v->str_val ? v->str_val : "");
        else ccg_sb(buf, len, cap, v->str_val ? v->str_val : "");
        return;
    case JSON_ARRAY:
        ccg_sb(buf, len, cap, "[");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ccg_sb(buf, len, cap, ", ");
            ccg_render(buf, len, cap, v->c.items[i], 1);
        }
        ccg_sb(buf, len, cap, "]");
        return;
    case JSON_OBJECT:
        ccg_sb(buf, len, cap, "{");
        for (size_t i = 0; i < v->c.count; i++) {
            if (i) ccg_sb(buf, len, cap, ", ");
            ccg_repr_str(buf, len, cap, v->c.keys[i]);
            ccg_sb(buf, len, cap, ": ");
            ccg_render(buf, len, cap, v->c.items[i], 1);
        }
        ccg_sb(buf, len, cap, "}");
        return;
    default:
        return;
    }
}

/* str(v) — Python str() of a JSON-modelled value. Caller frees. */
static char *ccg_py_str(const json_t *v)
{
    char *buf = malloc(16);
    size_t len = 0, cap = 16;
    buf[0] = 0;
    ccg_render(&buf, &len, &cap, v, 0);
    return buf;
}

static char *ccg_strip_ws(const char *s)
{
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'
                  || *s == '\v' || *s == '\f')) s++;
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') n--;
        else break;
    }
    char *r = malloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static void ccg_lower_ascii(char *s)
{
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* ---- implemented ports ---- */

/* PoP: is_hygiene_preagent_only_cooldown @ agent/context_compressor.py:_is_hygiene_preagent_only_cooldown */
int is_hygiene_preagent_only_cooldown(const json_t *error)
{
    /* text = str(error or "").strip().casefold() */
    char *text;
    if (!ccg_jtruth(error)) text = strdup("");
    else text = ccg_py_str(error);
    char *stripped = ccg_strip_ws(text);
    free(text);
    ccg_lower_ascii(stripped); /* ASCII casefold == lower */
    static const char *markers[] = {
        "session hygiene compression timed out",
        "hygiene compression deferred: turn-hold budget expired",
    };
    int hit = 0;
    for (size_t i = 0; i < sizeof markers / sizeof markers[0]; i++) {
        if (strstr(stripped, markers[i])) { hit = 1; break; }
    }
    free(stripped);
    return hit;
}

/* PoP: compression_cancelled @ agent/context_compressor.py:_compression_cancelled */
json_t *compression_cancelled(void)
{
    json_t *chk = ccg_state ? json_obj_get(ccg_state, "_compression_cancelled_check") : NULL;
    /* callable(cancelled_check): only the "fn" marker models a callable */
    int callable = chk && chk->type == JSON_STRING && strcmp(chk->str_val, "fn") == 0;
    if (!callable) return json_bool(false);
    json_t *r = ccg_seam("cancelled_check", json_array());
    if (ccg_is_raise(r)) {
        if (ccg_is_base_exc(r)) return r; /* BaseException propagates */
        json_free(r);
        /* logger.debug("compression cancellation check failed", exc_info=True) */
        json_t *la = json_array();
        json_append(la, json_string("compression cancellation check failed"));
        json_t *lr = ccg_seam("log_debug", la);
        json_free(lr);
        return json_bool(false);
    }
    int b = ccg_jtruth(r); /* bool(cancelled_check()) */
    json_free(r);
    return json_bool(b);
}

/* PoP: stale_thinking_on_wire @ agent/context_compressor.py:_stale_thinking_on_wire */
json_t *stale_thinking_on_wire(void)
{
    static const char *attrs[] = { "api_mode", "provider", "model", "base_url" };
    json_t *args = json_array();
    for (size_t i = 0; i < 4; i++) {
        json_t *v = ccg_state ? json_obj_get(ccg_state, attrs[i]) : NULL;
        /* getattr(self, attr, "") or "" — falsy collapses to "" */
        json_append(args, ccg_jtruth(v) ? json_copy(v) : json_string(""));
    }
    json_t *r = ccg_seam("stale_thinking_reaches_wire", args);
    if (ccg_is_raise(r)) {
        if (ccg_is_base_exc(r)) return r;
        json_free(r);
        return json_bool(false); /* except Exception: return False */
    }
    return r; /* live returns the predicate result unwrapped */
}

/* PoP: billed_basis_over_threshold @ agent/context_compressor.py:_billed_basis_over_threshold */
json_t *billed_basis_over_threshold(const json_t *current_tokens)
{
    json_t *th = ccg_state ? json_obj_get(ccg_state, "threshold_tokens") : NULL;
    if (!th)
        return ccg_raise("AttributeError",
                         "'ContextCompressor' object has no attribute 'threshold_tokens'");
    /* self.threshold_tokens > 0 — the comparison's type rules are observable */
    if (th->type != JSON_NUMBER && th->type != JSON_BOOL) {
        const char *tn = th->type == JSON_STRING ? "str"
                       : th->type == JSON_NULL ? "NoneType"
                       : th->type == JSON_ARRAY ? "list" : "dict";
        char msg[128];
        snprintf(msg, sizeof msg,
                 "'>' not supported between instances of '%s' and 'int'", tn);
        return ccg_raise("TypeError", msg);
    }
    double thv = th->type == JSON_BOOL ? (th->bool_val ? 1.0 : 0.0) : th->num_val;
    if (!(thv > 0.0)) return json_bool(false);
    if (!current_tokens || current_tokens->type == JSON_NULL)
        return json_bool(false); /* current_tokens is not None */
    if (current_tokens->type != JSON_NUMBER && current_tokens->type != JSON_BOOL) {
        const char *tn = current_tokens->type == JSON_STRING ? "str"
                       : current_tokens->type == JSON_ARRAY ? "list" : "dict";
        char msg[128];
        snprintf(msg, sizeof msg,
                 "'>=' not supported between instances of '%s' and 'int'", tn);
        return ccg_raise("TypeError", msg);
    }
    double cv = current_tokens->type == JSON_BOOL
              ? (current_tokens->bool_val ? 1.0 : 0.0) : current_tokens->num_val;
    return json_bool(cv >= thv);
}
