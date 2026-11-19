/* port_runtime_provider_flow.c — FLOW-core port of hermes_cli/runtime_provider.py.
 * See include/port_runtime_provider_flow.h for the seam table and the
 * representation contract. libjson-only; no real I/O inside the decision core.
 *
 * Internal REAL siblings (pure decision logic, exercised through the traces of
 * their callers, matching the live extracted defs): _cfg_provider,
 * _same_registered_provider, _runtime, _azure_anthropic_env_key,
 * openai_env_key_authoritative_for, _tag, _pool_entry_api_key,
 * _nous_entry_key_usable, _nous_min_key_ttl, _openrouter_fallback.
 */
#include "port_runtime_provider_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── seam dispatch ─────────────────────────────────────────────────────── */
json_t *rpf_call(rpf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

bool rpf_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

/* `except Exception` does NOT catch these BaseException-only types. */
static bool baseexc_only(const char *type) {
    return type && (strcmp(type, "SystemExit") == 0
        || strcmp(type, "KeyboardInterrupt") == 0
        || strcmp(type, "GeneratorExit") == 0);
}

static const char *raise_type(const json_t *r) {
    return json_get_str(r, "__raise__", "RuntimeError");
}
static const char *raise_msg(const json_t *r) {
    return json_get_str(r, "message", "");
}

/* ── Python truthiness / scalar helpers ────────────────────────────────── */
static bool jtruth(const json_t *v) {
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
static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

static void dtoa_repr(double x, char *buf, size_t cap) {
    if (isnan(x)) { snprintf(buf, cap, "nan"); return; }
    if (isinf(x)) { snprintf(buf, cap, x < 0 ? "-inf" : "inf"); return; }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, x);
        if (strtod(buf, NULL) == x) return;
    }
    snprintf(buf, cap, "%.17g", x);
}

/* Python str() of a JSON scalar (fixture scope: no integral-float literals,
 * so an integral double is an int and prints without a decimal point). */
static char *py_str_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
        } else {
            dtoa_repr(x, buf, sizeof buf);
        }
        return strdup(buf);
    }
    return strdup("");  /* containers excluded from fixtures */
}

/* ── ASCII whitespace strip / lower / rstrip(chars) ────────────────────── */
static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}
static char *py_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
static char *py_rstrip_chars(const char *s, const char *chars) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    while (n > 0 && strchr(chars, (unsigned char)s[n - 1]) != NULL) n--;
    char *out = malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}
static char *py_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return out;
}

/* Python repr() of an ASCII str: single quotes unless the value contains '
 * and no " (then double); escape backslash, the active quote, \n\r\t and
 * other control chars as \xNN. */
typedef struct { char *buf; size_t len, cap; } sbuf_t;
static void sb_init(sbuf_t *b) { b->cap = 128; b->len = 0; b->buf = malloc(b->cap); b->buf[0] = '\0'; }
static void sb_putc(sbuf_t *b, char c) {
    if (b->len + 2 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void sb_puts(sbuf_t *b, const char *s) {
    size_t n = strlen(s);
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    memcpy(b->buf + b->len, s, n); b->len += n; b->buf[b->len] = '\0';
}
static char *py_repr_str(const char *s) {
    if (!s) s = "";
    char q = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    sbuf_t b; sb_init(&b);
    sb_putc(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\') sb_puts(&b, "\\\\");
        else if (c == (unsigned char)q) { sb_putc(&b, '\\'); sb_putc(&b, (char)c); }
        else if (c == '\n') sb_puts(&b, "\\n");
        else if (c == '\r') sb_puts(&b, "\\r");
        else if (c == '\t') sb_puts(&b, "\\t");
        else if (c < 0x20 || c == 0x7f) {
            char esc[8]; snprintf(esc, sizeof esc, "\\x%02x", c); sb_puts(&b, esc);
        } else sb_putc(&b, (char)c);
    }
    sb_putc(&b, q);
    return b.buf;
}

/* raise descriptor helper (str(exc) parity with the oracle's projection) */
static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message);
    return o;
}

/* str(node or "") — the `x or ""` then str() idiom over a borrowed node. */
static char *str_or_empty(const json_t *v) {
    if (!jtruth(v)) return strdup("");
    return py_str_scalar(v);
}

/* ── internal REAL siblings ────────────────────────────────────────────── */

/* _cfg_provider: str(model_cfg.get("provider") or "").strip().lower() */
static json_t *cfg_provider_of(const json_t *model_cfg) {
    json_t *p = j_is_obj(model_cfg) ? json_obj_get(model_cfg, "provider") : NULL;
    char *s = str_or_empty(p);
    char *st = py_strip(s);
    char *lo = py_lower(st);
    json_t *r = json_string(lo);
    free(s); free(st); free(lo);
    return r;
}

/* PROVIDER_REGISTRY.get(key) seam; result null or a non-empty descriptor. */
static json_t *registry_get(rpf_io_t *io, const char *key) {
    json_t *args = json_object();
    json_set_hdr(args, "key", key);
    json_t *r = rpf_call(io, "PROVIDER_REGISTRY.get", args);
    json_free(args);
    return r;
}

/* _same_registered_provider(provider, configured_provider) */
static json_t *same_registered_provider_of(rpf_io_t *io, const char *provider, const char *configured_provider) {
    if (strcmp(provider, configured_provider) == 0) return json_bool(true);
    json_t *pconfig = registry_get(io, provider);
    if (rpf_raised(pconfig)) return pconfig;
    json_t *configured = registry_get(io, configured_provider);
    if (rpf_raised(configured)) { json_free(pconfig); return configured; }
    bool same = false;
    if (jtruth(pconfig) && jtruth(configured)) {
        json_t *pid = json_obj_get(pconfig, "id");
        json_t *cid = json_obj_get(configured, "id");
        if (j_is_str(pid) && j_is_str(cid)) same = strcmp(pid->str_val, cid->str_val) == 0;
        else same = false;  /* fixture scope: registry descriptors carry str id */
    }
    json_free(pconfig); json_free(configured);
    return json_bool(same);
}

/* _pool_entry_api_key: getattr(entry,"runtime_api_key",None) or getattr(entry,"access_token","") */
static json_t *pool_entry_api_key_of(const json_t *entry) {
    json_t *k = j_is_obj(entry) ? json_obj_get(entry, "runtime_api_key") : NULL;
    if (jtruth(k)) return json_copy(k);
    json_t *a = j_is_obj(entry) ? json_obj_get(entry, "access_token") : NULL;
    if (a) return json_copy(a);
    return json_string("");
}

/* env_int seam helper */
static json_t *env_int_call(rpf_io_t *io, const char *name, double def) {
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_set(args, "default", json_number(def));
    json_t *r = rpf_call(io, "env_int", args);
    json_free(args);
    return r;
}

/* _nous_min_key_ttl: max(60, env_int("HERMES_NOUS_MIN_KEY_TTL_SECONDS", 1800)) */
static json_t *nous_min_key_ttl_of(rpf_io_t *io) {
    json_t *v = env_int_call(io, "HERMES_NOUS_MIN_KEY_TTL_SECONDS", 1800);
    if (rpf_raised(v)) return v;
    double x = j_is_num(v) ? v->num_val : 0.0;
    json_free(v);
    return json_number(x > 60.0 ? x : 60.0);
}

/* _nous_entry_key_usable: _agent_key_is_usable({k: getattr(entry,k,None) ...}, min_ttl) */
static json_t *nous_entry_key_usable_of(rpf_io_t *io, const json_t *entry, double min_ttl) {
    static const char *keys[] = {"agent_key", "agent_key_expires_at", "scope"};
    json_t *state = json_object();
    for (size_t i = 0; i < 3; i++) {
        json_t *v = j_is_obj(entry) ? json_obj_get(entry, keys[i]) : NULL;
        json_set(state, keys[i], v ? json_copy(v) : json_null());
    }
    json_t *args = json_object();
    json_set(args, "state", state);
    json_set(args, "min_ttl", json_number(min_ttl));
    json_t *r = rpf_call(io, "_agent_key_is_usable", args);
    json_free(args);
    if (rpf_raised(r)) return r;
    bool usable = jtruth(r);
    json_free(r);
    return json_bool(usable);
}

/* get_secret_str(name, default) seam; returns a NEW str node (or raise). */
static json_t *get_secret_str_call(rpf_io_t *io, const char *name, const char *def) {
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_set_hdr(args, "default", def);
    json_t *r = rpf_call(io, "get_secret_str", args);
    json_free(args);
    return r;
}

/* base_url_host_matches(url, host) seam; returns NEW bool node (or raise). */
static json_t *host_matches_call(rpf_io_t *io, const json_t *url, const char *host) {
    json_t *args = json_object();
    json_set(args, "url", url ? json_copy(url) : json_null());
    json_set_hdr(args, "host", host);
    json_t *r = rpf_call(io, "base_url_host_matches", args);
    json_free(args);
    return r;
}

/* openai_env_key_authoritative_for(base_url, env_openai_base) — REAL sibling. */
static json_t *openai_env_key_authoritative_of(rpf_io_t *io, const json_t *base_url, const json_t *env_openai_base) {
    json_t *m1 = host_matches_call(io, base_url, "openai.com");
    if (rpf_raised(m1)) return m1;
    bool hit = jtruth(m1);
    json_free(m1);
    if (hit) return json_bool(true);
    json_t *m2 = host_matches_call(io, base_url, "openai.azure.com");
    if (rpf_raised(m2)) return m2;
    hit = jtruth(m2);
    json_free(m2);
    if (hit) return json_bool(true);
    char *e0 = str_or_empty(env_openai_base);
    char *e1 = py_strip(e0);
    char *env_base = py_rstrip_chars(e1, "/");
    free(e0); free(e1);
    bool result = false;
    if (env_base[0]) {
        char *b0 = str_or_empty(base_url);
        char *b1 = py_strip(b0);
        char *burl = py_rstrip_chars(b1, "/");
        result = strcmp(burl, env_base) == 0;
        free(b0); free(b1); free(burl);
    }
    free(env_base);
    return json_bool(result);
}

/* _azure_anthropic_env_key(model_cfg) — REAL sibling. */
static json_t *azure_anthropic_env_key_of(rpf_io_t *io, const json_t *model_cfg) {
    static const char *hint_keys[] = {"key_env", "api_key_env"};
    for (size_t i = 0; i < 2; i++) {
        json_t *hv = j_is_obj(model_cfg) ? json_obj_get(model_cfg, hint_keys[i]) : NULL;
        char *s0 = str_or_empty(hv);
        char *env_var = py_strip(s0);
        free(s0);
        if (env_var[0]) {
            json_t *tok = get_secret_str_call(io, env_var, "");
            if (rpf_raised(tok)) { free(env_var); return tok; }
            char *t0 = j_is_str(tok) ? strdup(tok->str_val) : strdup("");
            json_free(tok);
            char *token = py_strip(t0);
            free(t0);
            if (token[0]) { free(env_var); return json_string(token); }
            free(token);
        }
        free(env_var);
    }
    /* str(model_cfg.get("api_key") or "").strip() or AZURE_ANTHROPIC_KEY or ANTHROPIC_API_KEY */
    json_t *ak = j_is_obj(model_cfg) ? json_obj_get(model_cfg, "api_key") : NULL;
    char *a0 = str_or_empty(ak);
    char *inline_key = py_strip(a0);
    free(a0);
    if (inline_key[0]) return json_string(inline_key);
    free(inline_key);
    json_t *az = get_secret_str_call(io, "AZURE_ANTHROPIC_KEY", "");
    if (rpf_raised(az)) return az;
    char *z0 = j_is_str(az) ? strdup(az->str_val) : strdup("");
    json_free(az);
    char *azk = py_strip(z0);
    free(z0);
    if (azk[0]) return json_string(azk);
    free(azk);
    json_t *an = get_secret_str_call(io, "ANTHROPIC_API_KEY", "");
    if (rpf_raised(an)) return an;
    char *n0 = j_is_str(an) ? strdup(an->str_val) : strdup("");
    json_free(an);
    char *ank = py_strip(n0);
    free(n0);
    json_t *r = json_string(ank);
    free(ank);
    return r;
}

/* _tag: stamp requested_provider on a truthy runtime dict (in place). */
static void tag_runtime(json_t *runtime, const char *requested_provider) {
    if (jtruth(runtime) && j_is_obj(runtime)) {
        json_set(runtime, "requested_provider", json_string(requested_provider));
    }
}

/* _openrouter_fallback — REAL sibling (delegate + tag). */
static json_t *openrouter_fallback_of(rpf_io_t *io, const char *requested_provider,
                                      const json_t *explicit_api_key, const json_t *explicit_base_url) {
    json_t *args = json_object();
    json_set_hdr(args, "requested_provider", requested_provider);
    json_set(args, "explicit_api_key", explicit_api_key ? json_copy(explicit_api_key) : json_null());
    json_set(args, "explicit_base_url", explicit_base_url ? json_copy(explicit_base_url) : json_null());
    json_t *r = rpf_call(io, "_resolve_openrouter_runtime", args);
    json_free(args);
    if (rpf_raised(r)) return r;
    tag_runtime(r, requested_provider);
    return r;
}

/* _runtime(provider, api_mode, base_url, api_key, **extra) — REAL sibling.
 * extra is a borrowed object merged in insertion order (overwrite keeps
 * position, new keys append). Returns a NEW dict (or raise).
 * Call-site binding first: the caller spreads extra as **kwargs against the
 * four positional parameters; a duplicate key raises TypeError BEFORE any
 * body seam (first hit in SIGNATURE order; 3.14 names the bare qualname). */
static json_t *runtime_build(rpf_io_t *io, const char *provider, const char *api_mode,
                             const json_t *base_url, const json_t *api_key, const json_t *extra) {
    if (j_is_obj(extra)) {
        static const char *positional[] = {"provider", "api_mode", "base_url", "api_key"};
        for (int i = 0; i < 4; i++) {
            if (json_obj_get(extra, positional[i])) {
                char m[128];
                snprintf(m, sizeof(m), "_runtime() got multiple values for argument '%s'",
                         positional[i]);
                return raise_desc("TypeError", m);
            }
        }
    }
    json_t *rargs = json_object();
    json_set_hdr(rargs, "provider", provider);
    json_set(rargs, "base_url", base_url ? json_copy(base_url) : json_null());
    json_t *route = rpf_call(io, "is_actual_route", rargs);
    json_free(rargs);
    if (rpf_raised(route)) return route;
    bool actual = jtruth(route);
    json_free(route);
    const char *mode = api_mode;
    json_t *url_owned = NULL;
    const json_t *url = base_url;
    if (actual) {
        mode = "chat_completions";
        json_t *nargs = json_object();
        json_set(nargs, "base_url", base_url ? json_copy(base_url) : json_null());
        json_t *norm = rpf_call(io, "normalize_actual_base_url", nargs);
        json_free(nargs);
        if (rpf_raised(norm)) return norm;
        url_owned = norm;
        url = norm;
    }
    json_t *out = json_object();
    json_set_hdr(out, "provider", provider);
    json_set_hdr(out, "api_mode", mode);
    json_set(out, "base_url", url ? json_copy(url) : json_null());
    json_set(out, "api_key", api_key ? json_copy(api_key) : json_null());
    if (j_is_obj(extra)) {
        for (size_t i = 0; i < extra->c.count; i++) {
            json_set(out, extra->c.keys[i], json_copy(extra->c.items[i]));
        }
    }
    json_free(url_owned);
    return out;
}

/* ── PoP ports ─────────────────────────────────────────────────────────── */

/* PoP: rpf_resolves_to_custom @ hermes_cli/runtime_provider.py:_resolves_to_custom */
json_t *rpf_resolves_to_custom(rpf_io_t *io, json_t *in) {
    const char *name = json_get_str(in, "name", "");
    json_t *args = json_object();
    json_set_hdr(args, "name", name);
    json_t *r = rpf_call(io, "auth.resolve_provider", args);
    json_free(args);
    if (rpf_raised(r)) {
        if (baseexc_only(raise_type(r))) return r;
        json_free(r);
        return json_bool(false);
    }
    bool custom = j_is_str(r) && strcmp(r->str_val, "custom") == 0;
    json_free(r);
    return json_bool(custom);
}

/* PoP: rpf_cfg_provider @ hermes_cli/runtime_provider.py:_cfg_provider */
json_t *rpf_cfg_provider(rpf_io_t *io, json_t *in) {
    (void)io;
    return cfg_provider_of(json_obj_get(in, "model_cfg"));
}

/* PoP: rpf_same_registered_provider @ hermes_cli/runtime_provider.py:_same_registered_provider */
json_t *rpf_same_registered_provider(rpf_io_t *io, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *configured = json_get_str(in, "configured_provider", "");
    return same_registered_provider_of(io, provider, configured);
}

/* PoP: rpf_config_base_url_for_provider @ hermes_cli/runtime_provider.py:_config_base_url_for_provider */
json_t *rpf_config_base_url_for_provider(rpf_io_t *io, json_t *in) {
    json_t *model_cfg = json_obj_get(in, "model_cfg");
    const char *provider = json_get_str(in, "provider", "");
    json_t *cp = cfg_provider_of(model_cfg);
    char *configured = strdup(cp->str_val);
    json_free(cp);
    if (strcmp(provider, "actual") == 0) {
        json_t *args = json_object();
        json_set_hdr(args, "provider", configured);
        json_t *norm = rpf_call(io, "_models.normalize_provider", args);
        json_free(args);
        if (rpf_raised(norm)) { free(configured); return norm; }
        free(configured);
        configured = j_is_str(norm) ? strdup(norm->str_val) : strdup("");
        json_free(norm);
    }
    json_t *same = same_registered_provider_of(io, provider, configured);
    free(configured);
    if (rpf_raised(same)) return same;
    bool match = jtruth(same);
    json_free(same);
    if (!match) return json_string("");
    json_t *bu = j_is_obj(model_cfg) ? json_obj_get(model_cfg, "base_url") : NULL;
    char *s0 = str_or_empty(bu);
    char *s1 = py_strip(s0);
    char *s2 = py_rstrip_chars(s1, "/");
    json_t *r = json_string(s2);
    free(s0); free(s1); free(s2);
    return r;
}

/* PoP: rpf_on_declared_anthropic_endpoint @ hermes_cli/runtime_provider.py:_on_declared_anthropic_endpoint */
json_t *rpf_on_declared_anthropic_endpoint(rpf_io_t *io, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    json_t *base_url = json_obj_get(in, "base_url");
    json_t *gargs = json_object();
    json_set_hdr(gargs, "provider", provider);
    json_set(gargs, "allow_network", json_bool(false));
    json_t *pdef = rpf_call(io, "get_provider", gargs);
    json_free(gargs);
    if (rpf_raised(pdef)) return pdef;
    /* default = (pdef.base_url if pdef else "").strip() */
    json_t *dbu = jtruth(pdef) ? json_obj_get(pdef, "base_url") : NULL;
    char *d0 = jtruth(pdef) ? str_or_empty(dbu) : strdup("");
    json_free(pdef);
    char *default_url = py_strip(d0);
    free(d0);
    /* default_path = base_url_path(default).removesuffix("/v1") */
    json_t *pargs = json_object();
    json_set_hdr(pargs, "url", default_url);
    json_t *dp = rpf_call(io, "base_url_path", pargs);
    json_free(pargs);
    if (rpf_raised(dp)) { free(default_url); return dp; }
    char *dp_s = j_is_str(dp) ? strdup(dp->str_val) : strdup("");
    json_free(dp);
    size_t dpn = strlen(dp_s);
    if (dpn >= 3 && strcmp(dp_s + dpn - 3, "/v1") == 0) dp_s[dpn - 3] = '\0';
    /* if not default_path or not (base_url or "").strip(): return True */
    char *b0 = str_or_empty(base_url);
    char *bstrip = py_strip(b0);
    free(b0);
    if (!dp_s[0] || !bstrip[0]) {
        free(default_url); free(dp_s); free(bstrip);
        return json_bool(true);
    }
    free(bstrip);
    /* base_url_hostname(default) then base_url_host_matches(base_url, hostname) */
    json_t *hargs = json_object();
    json_set_hdr(hargs, "url", default_url);
    json_t *hn = rpf_call(io, "base_url_hostname", hargs);
    json_free(hargs);
    if (rpf_raised(hn)) { free(default_url); free(dp_s); return hn; }
    char *hostname = j_is_str(hn) ? strdup(hn->str_val) : strdup("");
    json_free(hn);
    json_t *hm = host_matches_call(io, base_url, hostname);
    free(hostname);
    if (rpf_raised(hm)) { free(default_url); free(dp_s); return hm; }
    bool matches = jtruth(hm);
    json_free(hm);
    if (!matches) { free(default_url); free(dp_s); return json_bool(false); }
    /* path = base_url_path(base_url) */
    json_t *bargs = json_object();
    json_set(bargs, "url", base_url ? json_copy(base_url) : json_null());
    json_t *bp = rpf_call(io, "base_url_path", bargs);
    json_free(bargs);
    if (rpf_raised(bp)) { free(default_url); free(dp_s); return bp; }
    char *path = j_is_str(bp) ? strdup(bp->str_val) : strdup("");
    json_free(bp);
    size_t pn = strlen(path), dn = strlen(dp_s);
    bool ok = (pn == 0) || (strcmp(path, dp_s) == 0)
        || (pn > dn && strncmp(path, dp_s, dn) == 0 && path[dn] == '/');
    free(path); free(default_url); free(dp_s);
    return json_bool(ok);
}

/* PoP: rpf_azure_inferred_api_mode @ hermes_cli/runtime_provider.py:_azure_inferred_api_mode */
json_t *rpf_azure_inferred_api_mode(rpf_io_t *io, json_t *in) {
    const char *effective_model = json_get_str(in, "effective_model", "");
    const char *api_mode = json_get_str(in, "api_mode", "");
    if (!effective_model[0] || strcmp(api_mode, "anthropic_messages") == 0) {
        return json_string(api_mode);
    }
    json_t *args = json_object();
    json_set_hdr(args, "model", effective_model);
    json_t *r = rpf_call(io, "_models.azure_foundry_model_api_mode", args);
    json_free(args);
    if (rpf_raised(r)) {
        if (baseexc_only(raise_type(r))) return r;
        json_free(r);
        return json_string(api_mode);
    }
    if (jtruth(r)) return r;
    json_free(r);
    return json_string(api_mode);
}

/* PoP: rpf_runtime @ hermes_cli/runtime_provider.py:_runtime */
json_t *rpf_runtime(rpf_io_t *io, json_t *in) {
    const char *provider = json_get_str(in, "provider", "");
    const char *api_mode = json_get_str(in, "api_mode", "");
    json_t *base_url = json_obj_get(in, "base_url");
    json_t *api_key = json_obj_get(in, "api_key");
    json_t *extra = json_obj_get(in, "extra");
    return runtime_build(io, provider, api_mode, base_url, api_key, extra);
}

/* PoP: rpf_creds_fallback @ hermes_cli/runtime_provider.py:_creds_fallback */
json_t *rpf_creds_fallback(rpf_io_t *io, json_t *in) {
    json_t *api_key = json_obj_get(in, "api_key");
    json_t *explicit_base_url = json_obj_get(in, "explicit_base_url");
    json_t *base_url = json_obj_get(in, "base_url");
    json_t *expiry = json_obj_get(in, "expiry");
    const char *expiry_key = json_get_str(in, "expiry_key", "");
    const char *resolve_name = json_get_str(in, "resolve_name", "resolve");
    if (jtruth(api_key)) {
        json_t *out = json_object();
        json_set(out, "api_key", json_copy(api_key));
        json_set(out, "base_url", base_url ? json_copy(base_url) : json_null());
        json_set(out, "expiry", expiry ? json_copy(expiry) : json_null());
        return out;
    }
    json_t *creds = rpf_call(io, resolve_name, NULL);
    if (rpf_raised(creds)) return creds;
    /* creds.get("api_key","") */
    json_t *ck = j_is_obj(creds) ? json_obj_get(creds, "api_key") : NULL;
    /* explicit_base_url or creds.get("base_url","").rstrip("/") or base_url */
    json_t *chosen_url = NULL;
    bool url_owned = false;
    if (jtruth(explicit_base_url)) {
        chosen_url = explicit_base_url;
    } else {
        json_t *cb = j_is_obj(creds) ? json_obj_get(creds, "base_url") : NULL;
        char *cbs = j_is_str(cb) ? cb->str_val : "";
        char *stripped = py_rstrip_chars(cbs, "/");
        if (stripped[0]) {
            chosen_url = json_string(stripped);
            url_owned = true;
        }
        free(stripped);
        if (!chosen_url) chosen_url = base_url;
    }
    json_t *ce = j_is_obj(creds) ? json_obj_get(creds, expiry_key) : NULL;
    json_t *out = json_object();
    json_set(out, "api_key", ck ? json_copy(ck) : json_string(""));
    json_set(out, "base_url", chosen_url ? (url_owned ? chosen_url : json_copy(chosen_url)) : json_null());
    json_set(out, "expiry", ce ? json_copy(ce) : json_null());
    json_free(creds);
    return out;
}

/* PoP: rpf_azure_anthropic_env_key @ hermes_cli/runtime_provider.py:_azure_anthropic_env_key */
json_t *rpf_azure_anthropic_env_key(rpf_io_t *io, json_t *in) {
    return azure_anthropic_env_key_of(io, json_obj_get(in, "model_cfg"));
}

/* PoP: rpf_openai_env_key_authoritative_for @ hermes_cli/runtime_provider.py:openai_env_key_authoritative_for */
json_t *rpf_openai_env_key_authoritative_for(rpf_io_t *io, json_t *in) {
    return openai_env_key_authoritative_of(io, json_obj_get(in, "base_url"), json_obj_get(in, "env_openai_base"));
}

/* PoP: rpf_host_gated_env_key_candidates @ hermes_cli/runtime_provider.py:_host_gated_env_key_candidates */
json_t *rpf_host_gated_env_key_candidates(rpf_io_t *io, json_t *in) {
    json_t *base_url = json_obj_get(in, "base_url");
    bool ollama = json_get_bool(in, "ollama", false);
    /* is_openai = openai_env_key_authoritative_for(base_url, get_secret_str("OPENAI_BASE_URL","")) */
    json_t *ob = get_secret_str_call(io, "OPENAI_BASE_URL", "");
    if (rpf_raised(ob)) return ob;
    json_t *is_openai = openai_env_key_authoritative_of(io, base_url, ob);
    json_free(ob);
    if (rpf_raised(is_openai)) return is_openai;
    bool openai_ok = jtruth(is_openai);
    json_free(is_openai);
    json_t *out = json_array();
    /* candidates = [OLLAMA_API_KEY.strip() if host_matches(ollama.com) else ""] if ollama else [] */
    if (ollama) {
        json_t *hm = host_matches_call(io, base_url, "ollama.com");
        if (rpf_raised(hm)) { json_free(out); return hm; }
        bool ollama_host = jtruth(hm);
        json_free(hm);
        if (ollama_host) {
            json_t *ok = get_secret_str_call(io, "OLLAMA_API_KEY", "");
            if (rpf_raised(ok)) { json_free(out); return ok; }
            char *s0 = j_is_str(ok) ? strdup(ok->str_val) : strdup("");
            json_free(ok);
            char *s1 = py_strip(s0);
            json_append(out, json_string(s1));
            free(s0); free(s1);
        } else {
            json_append(out, json_string(""));
        }
    }
    /* OPENAI_API_KEY.strip() if is_openai else "" */
    if (openai_ok) {
        json_t *ok = get_secret_str_call(io, "OPENAI_API_KEY", "");
        if (rpf_raised(ok)) { json_free(out); return ok; }
        char *s0 = j_is_str(ok) ? strdup(ok->str_val) : strdup("");
        json_free(ok);
        char *s1 = py_strip(s0);
        json_append(out, json_string(s1));
        free(s0); free(s1);
    } else {
        json_append(out, json_string(""));
    }
    /* OPENROUTER_API_KEY.strip() if host_matches(openrouter.ai) else "" */
    json_t *hm = host_matches_call(io, base_url, "openrouter.ai");
    if (rpf_raised(hm)) { json_free(out); return hm; }
    bool or_host = jtruth(hm);
    json_free(hm);
    if (or_host) {
        json_t *ok = get_secret_str_call(io, "OPENROUTER_API_KEY", "");
        if (rpf_raised(ok)) { json_free(out); return ok; }
        char *s0 = j_is_str(ok) ? strdup(ok->str_val) : strdup("");
        json_free(ok);
        char *s1 = py_strip(s0);
        json_append(out, json_string(s1));
        free(s0); free(s1);
    } else {
        json_append(out, json_string(""));
    }
    /* _host_derived_api_key(base_url) */
    json_t *dargs = json_object();
    json_set(dargs, "base_url", base_url ? json_copy(base_url) : json_null());
    json_t *derived = rpf_call(io, "_host_derived_api_key", dargs);
    json_free(dargs);
    if (rpf_raised(derived)) { json_free(out); return derived; }
    json_append(out, derived);
    return out;
}

/* PoP: rpf_openrouter_should_use_pool @ hermes_cli/runtime_provider.py:_openrouter_should_use_pool */
json_t *rpf_openrouter_should_use_pool(rpf_io_t *io, json_t *in) {
    const char *requested_provider = json_get_str(in, "requested_provider", "");
    json_t *model_cfg = json_obj_get(in, "model_cfg");
    json_t *explicit_api_key = json_obj_get(in, "explicit_api_key");
    json_t *explicit_base_url = json_obj_get(in, "explicit_base_url");
    /* cfg_base_url = str(model_cfg.get("base_url") or "").strip() */
    json_t *cbu = j_is_obj(model_cfg) ? json_obj_get(model_cfg, "base_url") : NULL;
    char *c0 = str_or_empty(cbu);
    char *cfg_base_url = py_strip(c0);
    free(c0);
    /* env_base_urls = get_secret_str("OPENAI_BASE_URL","").strip() or get_secret_str("OPENROUTER_BASE_URL","").strip() */
    json_t *e1 = get_secret_str_call(io, "OPENAI_BASE_URL", "");
    if (rpf_raised(e1)) { free(cfg_base_url); return e1; }
    char *e1s = j_is_str(e1) ? strdup(e1->str_val) : strdup("");
    json_free(e1);
    char *env_base_urls = py_strip(e1s);
    free(e1s);
    if (!env_base_urls[0]) {
        json_t *e2 = get_secret_str_call(io, "OPENROUTER_BASE_URL", "");
        if (rpf_raised(e2)) { free(cfg_base_url); free(env_base_urls); return e2; }
        char *e2s = j_is_str(e2) ? strdup(e2->str_val) : strdup("");
        json_free(e2);
        free(env_base_urls);
        env_base_urls = py_strip(e2s);
        free(e2s);
    }
    /* cfg_is_mirror = bool(cfg_base_url) and (cp in {auto,custom} or (cp==openrouter and not host_matches)) */
    bool cfg_is_mirror = false;
    if (cfg_base_url[0]) {
        json_t *cp = cfg_provider_of(model_cfg);
        bool auto_custom = strcmp(cp->str_val, "auto") == 0 || strcmp(cp->str_val, "custom") == 0;
        bool is_or = strcmp(cp->str_val, "openrouter") == 0;
        json_free(cp);
        if (auto_custom) {
            cfg_is_mirror = true;
        } else if (is_or) {
            json_t *urlnode = json_string(cfg_base_url);
            json_t *hm = host_matches_call(io, urlnode, "openrouter.ai");
            json_free(urlnode);
            if (rpf_raised(hm)) { free(cfg_base_url); free(env_base_urls); return hm; }
            cfg_is_mirror = !jtruth(hm);
            json_free(hm);
        }
    }
    bool has_custom_endpoint = jtruth(explicit_base_url) || env_base_urls[0] || cfg_is_mirror;
    bool in_set = strcmp(requested_provider, "openrouter") == 0 || strcmp(requested_provider, "auto") == 0;
    bool result = in_set && !has_custom_endpoint && !(jtruth(explicit_api_key) || jtruth(explicit_base_url));
    free(cfg_base_url); free(env_base_urls);
    return json_bool(result);
}

/* PoP: rpf_raise_if_provider_disabled @ hermes_cli/runtime_provider.py:_raise_if_provider_disabled */
json_t *rpf_raise_if_provider_disabled(rpf_io_t *io, json_t *in) {
    const char *requested_provider = json_get_str(in, "requested_provider", "");
    json_t *full_cfg = rpf_call(io, "load_config", NULL);
    if (rpf_raised(full_cfg)) return full_cfg;
    json_t *provs_cfg = j_is_obj(full_cfg) ? json_obj_get(full_cfg, "providers") : NULL;
    json_t *block = j_is_obj(provs_cfg) ? json_obj_get(provs_cfg, requested_provider) : NULL;
    if (j_is_obj(block)) {
        json_t *args = json_object();
        json_set(args, "block", json_copy(block));
        json_t *en = rpf_call(io, "_config_mod.is_provider_enabled", args);
        json_free(args);
        if (rpf_raised(en)) { json_free(full_cfg); return en; }
        bool enabled = jtruth(en);
        json_free(en);
        if (!enabled) {
            json_free(full_cfg);
            char *rep = py_repr_str(requested_provider);
            sbuf_t b; sb_init(&b);
            sb_puts(&b, "provider ");
            sb_puts(&b, rep);
            sb_puts(&b, " is disabled in config (providers.");
            sb_puts(&b, requested_provider);
            sb_puts(&b, ".enabled: false)");
            json_t *err = raise_desc("ValueError", b.buf);
            free(rep); free(b.buf);
            return err;
        }
    }
    json_free(full_cfg);
    return json_null();
}

/* PoP: rpf_raise_for_credentialless_bare_custom @ hermes_cli/runtime_provider.py:_raise_for_credentialless_bare_custom */
json_t *rpf_raise_for_credentialless_bare_custom(rpf_io_t *io, json_t *in) {
    (void)io;
    const char *requested_provider = json_get_str(in, "requested_provider", "");
    json_t *runtime = json_obj_get(in, "runtime");
    if (strcmp(requested_provider, "custom") != 0) return json_null();
    json_t *prov = j_is_obj(runtime) ? json_obj_get(runtime, "provider") : NULL;
    if (!(j_is_str(prov) && strcmp(prov->str_val, "custom") == 0)) return json_null();
    json_t *key = j_is_obj(runtime) ? json_obj_get(runtime, "api_key") : NULL;
    if (jtruth(key)) return json_null();
    sbuf_t b; sb_init(&b);
    sb_puts(&b, "provider '");
    sb_puts(&b, requested_provider);
    sb_puts(&b, "' resolved without credentials (no endpoint or API key configured). "
                "If this is a named custom provider, use its real name (see providers: in config.yaml).");
    json_t *err = raise_desc("AuthError", b.buf);
    free(b.buf);
    return err;
}

/* PoP: rpf_tag @ hermes_cli/runtime_provider.py:_tag */
json_t *rpf_tag(rpf_io_t *io, json_t *in) {
    (void)io;
    json_t *runtime = json_obj_get(in, "runtime");
    const char *requested_provider = json_get_str(in, "requested_provider", "");
    tag_runtime(runtime, requested_provider);
    return runtime ? json_copy(runtime) : json_null();
}

/* PoP: rpf_local_endpoint_bypass @ hermes_cli/runtime_provider.py:_local_endpoint_bypass */
json_t *rpf_local_endpoint_bypass(rpf_io_t *io, json_t *in) {
    const char *requested_provider = json_get_str(in, "requested_provider", "");
    json_t *explicit_api_key = json_obj_get(in, "explicit_api_key");
    json_t *explicit_base_url = json_obj_get(in, "explicit_base_url");
    json_t *model_cfg = rpf_call(io, "_get_model_config", NULL);
    if (rpf_raised(model_cfg)) return model_cfg;
    json_t *cbu = j_is_obj(model_cfg) ? json_obj_get(model_cfg, "base_url") : NULL;
    char *c0 = str_or_empty(cbu);
    char *cfg_base_url = py_strip(c0);
    free(c0);
    if (!cfg_base_url[0]) { free(cfg_base_url); json_free(model_cfg); return json_null(); }
    json_t *cp = cfg_provider_of(model_cfg);
    bool auto_empty = strcmp(cp->str_val, "auto") == 0 || cp->str_val[0] == '\0';
    json_free(cp);
    if (!auto_empty) { free(cfg_base_url); json_free(model_cfg); return json_null(); }
    json_free(model_cfg);
    static const char *cloud_hosts[] = {"openrouter.ai", "anthropic.com", "openai.com"};
    json_t *urlnode = json_string(cfg_base_url);
    for (size_t i = 0; i < 3; i++) {
        json_t *hm = host_matches_call(io, urlnode, cloud_hosts[i]);
        if (rpf_raised(hm)) { json_free(urlnode); free(cfg_base_url); return hm; }
        bool cloud = jtruth(hm);
        json_free(hm);
        if (cloud) { json_free(urlnode); free(cfg_base_url); return json_null(); }
    }
    json_free(urlnode);
    free(cfg_base_url);
    return openrouter_fallback_of(io, requested_provider, explicit_api_key, explicit_base_url);
}

/* PoP: rpf_resolve_requested_shortcuts @ hermes_cli/runtime_provider.py:_resolve_requested_shortcuts */
json_t *rpf_resolve_requested_shortcuts(rpf_io_t *io, json_t *in) {
    const char *rq = json_get_str(in, "requested_provider", "");
    json_t *explicit_api_key = json_obj_get(in, "explicit_api_key");
    json_t *explicit_base_url = json_obj_get(in, "explicit_base_url");
    json_t *target_model = json_obj_get(in, "target_model");
    if (strcmp(rq, "moa") == 0) {
        json_t *extra = json_object();
        json_set_hdr(extra, "source", "moa-virtual-provider");
        json_set_hdr(extra, "requested_provider", rq);
        json_t *bu = json_string("moa://local");
        json_t *ak = json_string("moa-virtual-provider");
        json_t *r = runtime_build(io, "moa", "chat_completions", bu, ak, extra);
        json_free(extra); json_free(bu); json_free(ak);
        return r;
    }
    char *e0 = str_or_empty(explicit_base_url);
    char *eff_base = py_strip(e0);
    free(e0);
    if (strcmp(rq, "anthropic") == 0) {
        json_t *ebnode = json_string(eff_base);
        json_t *hm = host_matches_call(io, ebnode, "azure.com");
        json_free(ebnode);
        if (rpf_raised(hm)) { free(eff_base); return hm; }
        bool azure = jtruth(hm);
        json_free(hm);
        if (azure) {
            char *k0 = str_or_empty(explicit_api_key);
            char *key = py_strip(k0);
            free(k0);
            if (!key[0]) {
                free(key);
                json_t *empty_cfg = json_object();
                json_t *ek = azure_anthropic_env_key_of(io, empty_cfg);
                json_free(empty_cfg);
                if (rpf_raised(ek)) { free(eff_base); return ek; }
                key = j_is_str(ek) ? strdup(ek->str_val) : strdup("");
                json_free(ek);
            }
            if (!key[0]) {
                free(key); free(eff_base);
                return raise_desc("AuthError",
                    "No Azure Anthropic API key found. Set AZURE_ANTHROPIC_KEY or ANTHROPIC_API_KEY, or point "
                    "key_env/api_key_env in your config.yaml model section at a custom env var.");
            }
            char *url = py_rstrip_chars(eff_base, "/");
            json_t *bunode = json_string(url);
            json_t *aknode = json_string(key);
            json_t *extra = json_object();
            json_set_hdr(extra, "source", "azure-explicit");
            json_set_hdr(extra, "requested_provider", rq);
            json_t *r = runtime_build(io, "anthropic", "anthropic_messages", bunode, aknode, extra);
            free(url); json_free(bunode); json_free(aknode); json_free(extra);
            free(key); free(eff_base);
            return r;
        }
    }
    free(eff_base);
    if (strcmp(rq, "azure-foundry") == 0) {
        /* kwarg left-to-right: model_cfg=_get_model_config() evaluates before the call */
        json_t *mc = rpf_call(io, "_get_model_config", NULL);
        if (rpf_raised(mc)) return mc;
        json_t *args = json_object();
        json_set_hdr(args, "requested_provider", rq);
        json_set(args, "model_cfg", mc);
        json_set(args, "explicit_api_key", explicit_api_key ? json_copy(explicit_api_key) : json_null());
        json_set(args, "explicit_base_url", explicit_base_url ? json_copy(explicit_base_url) : json_null());
        json_set(args, "target_model", target_model ? json_copy(target_model) : json_null());
        json_t *r = rpf_call(io, "_resolve_azure_foundry_runtime", args);
        json_free(args);
        return r;
    }
    static const char *vertex_names[] = {"vertex", "google-vertex", "vertex-ai", "gcp-vertex", "vertexai"};
    for (size_t i = 0; i < 5; i++) {
        if (strcmp(rq, vertex_names[i]) == 0) {
            json_t *args = json_object();
            json_set_hdr(args, "requested_provider", rq);
            json_t *r = rpf_call(io, "_resolve_vertex_runtime", args);
            json_free(args);
            return r;
        }
    }
    return json_null();
}

/* PoP: rpf_refresh_nous_pool_entry @ hermes_cli/runtime_provider.py:_refresh_nous_pool_entry */
json_t *rpf_refresh_nous_pool_entry(rpf_io_t *io, json_t *in) {
    json_t *entry_in = json_obj_get(in, "entry");
    json_t *entry = entry_in ? json_copy(entry_in) : json_null();
    json_t *pak_in = json_obj_get(in, "pool_api_key");
    json_t *pool_api_key = pak_in ? json_copy(pak_in) : json_string("");
    json_t *ttl = nous_min_key_ttl_of(io);
    if (rpf_raised(ttl)) { json_free(entry); json_free(pool_api_key); return ttl; }
    double min_ttl = j_is_num(ttl) ? ttl->num_val : 60.0;
    json_free(ttl);
    json_t *usable = nous_entry_key_usable_of(io, entry, min_ttl);
    if (rpf_raised(usable)) { json_free(entry); json_free(pool_api_key); return usable; }
    bool ok = jtruth(usable);
    json_free(usable);
    if (ok) {
        json_t *out = json_object();
        json_set(out, "entry", json_copy(entry));
        json_set(out, "pool_api_key", json_copy(pool_api_key));
        json_free(entry); json_free(pool_api_key);
        return out;
    }
    /* logger.debug("Nous pool entry agent_key expired/missing, refreshing selected pool entry") */
    json_t *largs = json_object();
    json_set_hdr(largs, "fmt", "Nous pool entry agent_key expired/missing, refreshing selected pool entry");
    json_set(largs, "args", json_array());
    json_t *lg = rpf_call(io, "logger.debug", largs);
    json_free(largs);
    if (rpf_raised(lg)) { json_free(entry); json_free(pool_api_key); return lg; }
    json_free(lg);
    /* try: refreshed = pool.try_refresh_current() except Exception: log + None */
    json_t *refreshed = rpf_call(io, "pool.try_refresh_current", NULL);
    if (rpf_raised(refreshed)) {
        if (baseexc_only(raise_type(refreshed))) { json_free(entry); json_free(pool_api_key); return refreshed; }
        char *msg = strdup(raise_msg(refreshed));
        json_free(refreshed);
        json_t *fargs = json_object();
        json_set_hdr(fargs, "fmt", "Nous pool entry refresh failed: %s");
        json_t *vals = json_array();
        json_append(vals, json_string(msg));
        free(msg);
        json_set(fargs, "args", vals);
        json_t *fl = rpf_call(io, "logger.debug", fargs);
        json_free(fargs);
        if (rpf_raised(fl)) { json_free(entry); json_free(pool_api_key); return fl; }
        json_free(fl);
        refreshed = NULL;
    }
    if (refreshed && refreshed->type != JSON_NULL) {
        json_free(entry);
        entry = refreshed;
        json_free(pool_api_key);
        pool_api_key = pool_entry_api_key_of(entry);
    } else {
        json_free(refreshed);
    }
    /* if not pool_api_key or not _nous_entry_key_usable(entry, min_ttl): log + "" */
    bool still_bad = false;
    if (!jtruth(pool_api_key)) {
        still_bad = true;
    } else {
        json_t *u2 = nous_entry_key_usable_of(io, entry, min_ttl);
        if (rpf_raised(u2)) { json_free(entry); json_free(pool_api_key); return u2; }
        still_bad = !jtruth(u2);
        json_free(u2);
    }
    if (still_bad) {
        json_t *sargs = json_object();
        json_set_hdr(sargs, "fmt", "Nous pool entry agent_key still unavailable, falling through to runtime resolution");
        json_set(sargs, "args", json_array());
        json_t *sl = rpf_call(io, "logger.debug", sargs);
        json_free(sargs);
        if (rpf_raised(sl)) { json_free(entry); json_free(pool_api_key); return sl; }
        json_free(sl);
        json_free(pool_api_key);
        pool_api_key = json_string("");
    }
    json_t *out = json_object();
    json_set(out, "entry", json_copy(entry));
    json_set(out, "pool_api_key", json_copy(pool_api_key));
    json_free(entry); json_free(pool_api_key);
    return out;
}