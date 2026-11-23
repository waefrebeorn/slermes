/* ===========================================================================
 * route_identity_flow.c -- FLOW + leaf port of the UNCLAIMED route-identity
 * remainder:
 *   hermes_cli/route_identity.py  normalize_route_base_url,
 *                               should_clear_context_pin,
 *                               should_clear_context_pin_async
 *   utils/urls.py                 _parse_base_url, _hostname_of,
 *                               base_url_hostname, base_url_path,
 *                               base_url_origin, same_base_url_origin,
 *                               base_url_host_matches
 *
 * Scope note: hermes_cli/route_identity.py has exactly six top-level defs.
 * Three (provider_owns_route, drop_stale_model_route,
 * drop_credentials_of_other_route) are CLAIMED by the sibling FLOW port
 * src/agent/fallback_cooldown_flow.c (fc_* entries) and are NOT re-ported
 * here; none of the ten entries below calls them, so they need no seam.
 * The three remaining route_identity defs were claimed only by non-FLOW
 * legacy artifacts (config_py_pure.c pure port / route_identity.c
 * pre-standard printf wrapper, allowlisted as such) -- the established
 * flow-port practice re-claims those under a differential FLOW oracle.
 * utils/urls.py is the fully UNCLAIMED URL-identity leaf neighborhood of
 * provider_owns_route (its `from utils import base_url_hostname`); porting
 * it whole brings the claim count to ten unclaimed-by-FLOW defs.
 *
 * Self-contained: libjson only. Every live dependency that is NOT ported
 * here is injected through ONE universal seam:
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (the port owns it and frees it after the call);
 * the result is a NEW json_t owned by the caller (NULL == None; a
 * {"__raise__":kind,"message":msg} object == a raised exception). A scripted
 * JSON null result is delivered as a NULL pointer so truthiness reads falsy.
 * Fail-closed: no channel installed, or a missing/exhausted script, yields
 * {"__raise__":"RuntimeError","message":"no seam installed for <name>"} /
 * "no script for <name>", recorded by the dispatcher BEFORE the port
 * propagates it. should_clear_context_pin's `except Exception` SWALLOWS
 * those RuntimeErrors into True (the live fail-closed design).
 *
 * ENTRIES. json_t *rid_<fn>(rid_io_t *io, const json_t *in, json_t *st)
 *   io  -- universal seam channel (NULL => fail-closed production default)
 *   in  -- borrowed case input object (never mutated)
 *   st  -- fresh object the entry fills with the final state projection.
 *          Every entry here is stateless: st stays {} (also on raise paths,
 *          which the harness enforces). Caller owns the returned json_t.
 *          Tuples project to JSON arrays; None to NULL.
 *
 * Ported siblings compose REAL:
 *   should_clear_context_pin_async -> should_clear_context_pin
 *     (asyncio.to_thread runs its callable synchronously with no trace of
 *     its own, the gateway_inbound_flow2 convention; the `import asyncio`
 *     is silent and never fails in the fixture domain)
 *   base_url_hostname / base_url_path / base_url_origin -> _parse_base_url
 *     (+ _hostname_of for hostname/origin)
 *   same_base_url_origin -> base_url_origin (twice, short-circuit)
 *   base_url_host_matches -> base_url_hostname
 * Claimed siblings SEAMED:
 *   context_route_mismatch  agent.agent_init._context_route_mismatch(
 *       configured_base_url, active_base_url, configured_provider,
 *       active_provider)  -- the in-function import is silent; a fixture
 *       import_ok=false models sys.modules["agent.agent_init"]=None
 *       (ImportError caught by `except Exception` -> True, empty trace).
 *
 * BaseException kinds (SystemExit, KeyboardInterrupt, GeneratorExit,
 * CancelledError) PASS THROUGH the one `except Exception` site
 * (should_clear_context_pin). No exception parent table is needed: that
 * site catches exactly Exception, so the check is a direct kind test, not
 * a hierarchy walk (every other scripted kind is Exception-derived, the
 * fallback_cooldown_flow convention).
 *
 * Seam name -> live call table:
 *   context_route_mismatch  agent.agent_init._context_route_mismatch(
 *                           configured_base_url, active_base_url,
 *                           configured_provider, active_provider)
 *
 * URL machinery: urllib.parse.urlsplit/urlunsplit/urlparse are stdlib (not
 * quarry code) and are transcribed inline as static rid_* helpers -- no
 * PoP claims, no seams. The transcription covers:
 *   - _urlsplit: leading C0+space lstrip, \t\r\n removal ANYWHERE, scheme
 *     scan (first ':', ASCII-alpha start, scheme_chars [A-Za-z0-9+-.],
 *     lowercased), _splitnetloc (earliest of /?# from index 2), bracket
 *     balance + _check_bracketed_netloc/_check_bracketed_host validation,
 *     fragment-then-query split. _checknetloc is a no-op on ASCII netlocs
 *     (the fixture domain).
 *   - _hostinfo/hostname/port properties: rpartition('@'), bracket
 *     partition, '%' zone split (address lowercased, zone VERBATIM),
 *     port isdigit+isascii / 0..65535 validation with the exact ValueError
 *     messages.
 *   - _check_bracketed_host: v/V IPvFuture regex \A[vV][a-fA-F0-9]+\..+\z,
 *     else ipaddress.ip_address accept/reject (IPv4-in-brackets rejected;
 *     the generic "'<h>' does not appear to be an IPv4 or IPv6 address"
 *     message -- ip_address swallows the inner AddressValueErrors). The
 *     IPv6 accept/reject validator mirrors IPv6Address._ip_int_from_string
 *     (scope split, '/' rejection, 45-char cap, split(':', maxsplit=9),
 *     IPv4-suffix conversion, single '::' interior scan, endpoint rules,
 *     1..4-hex-digit hextets).
 *   - urlunsplit: netloc-presence branch, '/' path prepend, scheme/query/
 *     fragment appends (empty query/fragment project to None).
 *   - urlparse: urlsplit + _splitparams (';' after the last '/') for the
 *     uses_params schemes.
 *
 * State model: none -- all ten entries are pure or seam-only; st = {}.
 *
 * Domain restrictions (documented, enforced by the generator):
 *   - ASCII text inputs only. NUL bytes are OUTSIDE the domain (libjson
 *     stores strings as C strings; an embedded NUL truncates). DEL (0x7f)
 *     and the other non-NUL C0 controls are IN domain.
 *   - str()-computed positions (normalize_route_base_url.base_url,
 *     should_clear_context_pin.configured_model/active_model) carry
 *     strings, null, bools or INTEGRAL numbers only (integral floats ==
 *     ints; the generator emits ints). Composites / non-integral numbers
 *     there fail closed with RuntimeError (live str() would render a
 *     Python repr -- outside the transcription domain).
 *   - utils strip()-computed positions (base_url / domain) carry ANY JSON
 *     scalar: truthy non-strings raise the live AttributeError
 *     ('int'/'float'/'bool'/'list'/'dict' object has no attribute 'strip').
 *   - Bracketed-host contents carry no quote/backslash characters (the
 *     generic ip_address message embeds repr(hostname); the domain keeps
 *     repr == "'<h>'"). Port strings likewise carry no quotes/backslashes.
 *   - _hostname_of.parsed is null or a 6-string array projecting a
 *     ParseResult (scheme, netloc, path, params, query, fragment); the
 *     oracle materializes the real namedtuple, the C reads netloc[1].
 *   - should_clear_context_pin_async.args is an array; its model positions
 *     (0,1) obey the str() domain above, positions 2..5 forward verbatim
 *     to the seam (any JSON). import_ok defaults true.
 *   - context_route_mismatch scripts: any JSON value or raise spec; the
 *     return is forwarded UNCHANGED (live returns the callee result, not
 *     a bool coercion).
 * =========================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct rid_io {
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
    void *ctx;
} rid_io_t;

/* ---- truthiness / accessors ---- */

static int rid_truth(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 0;
}

static const char *rid_str(const json_t *v)
{
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static json_t *rid_get(const json_t *o, const char *k)
{
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static json_t *rid_copy_or_null(const json_t *v)
{
    return v ? json_copy(v) : NULL;
}

/* json_set(obj, key, NULL) is a libjson no-op: explicit nulls need a node */
static void rid_aset_j(json_t *o, const char *k, const json_t *v)
{
    json_set(o, k, v ? json_copy(v) : json_null());
}

/* ---- raise specs ---- */

static int rid_is_raise(const json_t *r)
{
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *rid_kind(const json_t *r)
{
    const char *k = rid_str(rid_get(r, "__raise__"));
    return k ? k : "";
}

/* `except Exception` passthrough set: the BaseException-only kinds. Every
 * other scripted kind is Exception-derived (fallback_cooldown_flow rule). */
static int rid_is_base(const char *kind)
{
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

static json_t *rid_mkraise(const char *kind, const char *msg)
{
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(kind));
    json_set(o, "message", json_string(msg ? msg : ""));
    return o;
}

/* malloc'd printf-style string builder */
static char *rid_xfmt(const char *fmt, ...)
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
static json_t *rid_call(rid_io_t *io, const char *name, json_t *args)
{
    json_t *r;
    if (!io || !io->call) {
        char *m = rid_xfmt("no seam installed for %s", name);
        r = rid_mkraise("RuntimeError", m ? m : "no seam installed");
        free(m);
        json_free(args);
        return r;
    }
    r = io->call(io->ctx, name, args);
    json_free(args);
    return r;
}

/* ---- string helpers ---- */

static char *rid_dup(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static char *rid_ndup(const char *s, size_t n)
{
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char rid_lower_c(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

static char *rid_lower_ascii(const char *s)
{
    char *p = rid_dup(s);
    if (p) for (char *c = p; *c; c++) *c = rid_lower_c(*c);
    return p;
}

static char *rid_lower_ndup(const char *s, size_t n)
{
    char *p = rid_ndup(s, n);
    if (p) for (char *c = p; *c; c++) *c = rid_lower_c(*c);
    return p;
}

/* Python str.strip() default White_Space set, ASCII members only:
 * ' ' \t \n \r \v \f \x1c \x1d \x1e \x1f (NOT the other C0 controls). */
static int rid_is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v'
        || c == '\f' || (c >= 0x1c && c <= 0x1f);
}

static char *rid_strip_ws(const char *s)
{
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && rid_is_ws(s[a])) a++;
    while (b > a && rid_is_ws(s[b - 1])) b--;
    return rid_ndup(s + a, b - a);
}

/* urlsplit lstrip set: C0 controls + space (chars <= 0x20), LEADING only */
static char *rid_lstrip_c0(const char *s)
{
    while (*s && (unsigned char)*s <= 0x20) s++;
    return rid_dup(s);
}

/* _UNSAFE_URL_BYTES_TO_REMOVE: \t \r \n removed ANYWHERE */
static char *rid_remove_unsafe(const char *s)
{
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; i++)
        if (s[i] != '\t' && s[i] != '\r' && s[i] != '\n') p[k++] = s[i];
    p[k] = '\0';
    return p;
}

static void rid_rstrip(char *s, char c)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == c) s[--n] = '\0';
}

/* ---- str() / strip() input materialization ---- */

/* str(v or "") for the normalize / context-pin model positions.
 * Domain: null/bool/integral-number/string. Returns malloc'd string, or
 * NULL with *err (RuntimeError) outside the domain. */
static char *rid_str_of(const json_t *v, char **err)
{
    *err = NULL;
    if (!rid_truth(v)) return rid_dup("");
    if (v->type == JSON_STRING) return rid_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return rid_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) return rid_xfmt("%lld", (long long)d);
        *err = rid_dup("str() of non-integral number outside fixture domain");
        return NULL;
    }
    *err = rid_dup("str() of composite outside fixture domain");
    return NULL;
}

/* (v or "").strip() for the utils positions: truthy non-strings raise the
 * live AttributeError naming the materialized type. */
static char *rid_strip_input(const json_t *v, char **err)
{
    *err = NULL;
    if (!rid_truth(v)) return rid_dup("");
    if (v->type == JSON_STRING) return rid_strip_ws(v->str_val ? v->str_val : "");
    const char *tn;
    if (v->type == JSON_BOOL) tn = "bool";
    else if (v->type == JSON_ARRAY) tn = "list";
    else if (v->type == JSON_OBJECT) tn = "dict";
    else tn = (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
    *err = rid_xfmt("'%s' object has no attribute 'strip'", tn);
    return NULL;
}

/* ---- ipaddress accept/reject validators ---- */

/* IPv4Address: exactly 4 dot-separated ASCII-digit octets, 1..3 chars,
 * no leading zeros, 0..255; '/' rejected. Accept/reject only (ip_address
 * swallows the inner messages). */
static int rid_ipv4_valid(const char *s)
{
    if (!s || !*s) return 0;
    if (strchr(s, '/')) return 0;
    const char *p = s;
    int noct = 0;
    for (;;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len == 0 || len > 3) return 0;
        if (len > 1 && p[0] == '0') return 0;
        long v = 0;
        for (size_t i = 0; i < len; i++) {
            if (p[i] < '0' || p[i] > '9') return 0;
            v = v * 10 + (p[i] - '0');
        }
        if (v > 255) return 0;
        noct++;
        if (!dot) break;
        if (noct >= 4) return 0;
        p = dot + 1;
    }
    return noct == 4;
}

static int rid_hextet_valid(const char *h)
{
    size_t n = strlen(h);
    if (n == 0 || n > 4) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = h[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

/* IPv6Address._ip_int_from_string accept/reject (addr part, scope split
 * already done by the caller). */
static int rid_ipv6_addr_valid(const char *a)
{
    if (!*a) return 0;
    if (strlen(a) > 45) return 0;
    /* parts = a.split(':', maxsplit=9) -- at most 10 parts, the last holds
     * the remainder when more than 9 colons are present. */
    char *parts[10];
    int np = 0;
    const char *p = a;
    for (int k = 0; k < 10; k++) {
        const char *col = strchr(p, ':');
        if (!col || k == 9) { parts[np++] = rid_dup(p); break; }
        parts[np++] = rid_ndup(p, (size_t)(col - p));
        p = col + 1;
    }
    int ok = 0;
    if (np < 3) goto done;
    /* IPv4-style suffix: convert the last part to two hextets. np==10
     * means >9 colons, so the remainder carries a ':' and can never be a
     * valid IPv4 -- guard the parts[np] append explicitly anyway. */
    if (strchr(parts[np - 1], '.')) {
        if (np >= 10) goto done;
        if (!rid_ipv4_valid(parts[np - 1])) goto done;
        free(parts[np - 1]);
        parts[np - 1] = rid_dup("0");
        parts[np] = rid_dup("0");
        np++;
    }
    if (np > 9) goto done;
    {
        int skip = -1;
        for (int i = 1; i < np - 1; i++) {
            if (!parts[i][0]) {
                if (skip >= 0) goto done;   /* at most one '::' */
                skip = i;
            }
        }
        int hi, lo, skipped;
        if (skip >= 0) {
            hi = skip;
            lo = np - skip - 1;
            if (!parts[0][0]) { hi--; if (hi) goto done; }
            if (!parts[np - 1][0]) { lo--; if (lo) goto done; }
            skipped = 8 - (hi + lo);
            if (skipped < 1) goto done;
        } else {
            if (np != 8) goto done;
            if (!parts[0][0] || !parts[np - 1][0]) goto done;
            hi = np; lo = 0; skipped = 0;
        }
        for (int i = 0; i < hi; i++)
            if (!rid_hextet_valid(parts[i])) goto done;
        for (int i = np - lo; i < np; i++)
            if (!rid_hextet_valid(parts[i])) goto done;
    }
    ok = 1;
done:
    for (int i = 0; i < np; i++) free(parts[i]);
    return ok;
}

/* IPv6Address: '/' rejected, scope split at the FIRST '%' (zone non-empty,
 * no second '%'), then the addr validator. */
static int rid_ipv6_valid(const char *s)
{
    if (!s) return 0;
    if (strchr(s, '/')) return 0;
    const char *pct = strchr(s, '%');
    if (pct) {
        if (!pct[1] || strchr(pct + 1, '%')) return 0;
        char *addr = rid_ndup(s, (size_t)(pct - s));
        int ok = rid_ipv6_addr_valid(addr);
        free(addr);
        return ok;
    }
    return rid_ipv6_addr_valid(s);
}

/* _check_bracketed_host: returns a malloc'd ValueError message or NULL.
 * Domain: hostname carries no quotes/backslashes (repr == "'<h>'"). */
static char *rid_check_bracketed_host(const char *h)
{
    if (h[0] == 'v' || h[0] == 'V') {
        /* re.match(r"\A[vV][a-fA-F0-9]+\..+\z", h) */
        const char *p = h + 1;
        const char *start = p;
        while (*p && ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')
                      || (*p >= '0' && *p <= '9'))) p++;
        if (p == start || *p != '.' || !p[1])
            return rid_dup("IPvFuture address is invalid");
        return NULL;
    }
    if (rid_ipv4_valid(h)) return rid_dup("An IPv4 address cannot be in brackets");
    if (rid_ipv6_valid(h)) return NULL;
    return rid_xfmt("'%s' does not appear to be an IPv4 or IPv6 address", h);
}

/* _check_bracketed_netloc: mirror the _hostinfo splitting; no data before
 * the '[' or after the ']' (before the port ':'); then validate the host
 * EVEN WHEN the post-userinfo hostinfo has no brackets ('[::1]@h'). */
static char *rid_check_bracketed_netloc(const char *netloc)
{
    const char *at = strrchr(netloc, '@');
    const char *hi = at ? at + 1 : netloc;
    const char *ob = strchr(hi, '[');
    char *hostname;
    if (ob) {
        if (ob != hi) return rid_dup("Invalid IPv6 URL");
        const char *cb = strchr(ob + 1, ']');
        size_t hlen = cb ? (size_t)(cb - ob - 1) : strlen(ob + 1);
        hostname = rid_ndup(ob + 1, hlen);
        const char *port = cb ? cb + 1 : "";
        if (*port && port[0] != ':') {
            free(hostname);
            return rid_dup("Invalid IPv6 URL");
        }
    } else {
        const char *col = strchr(hi, ':');
        hostname = col ? rid_ndup(hi, (size_t)(col - hi)) : rid_dup(hi);
    }
    char *err = rid_check_bracketed_host(hostname);
    free(hostname);
    return err;
}

/* ---- urlsplit / urlunsplit / urlparse transcription ---- */

typedef struct rid_split {
    char *scheme, *netloc, *path, *query, *fragment, *params;
} rid_split_t;

static void rid_split_free(rid_split_t *s)
{
    free(s->scheme); free(s->netloc); free(s->path);
    free(s->query); free(s->fragment); free(s->params);
    memset(s, 0, sizeof(*s));
}

static int rid_scheme_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

/* _urlsplit. Returns NULL on success (out filled, all fields malloc'd),
 * else a malloc'd ValueError message. _checknetloc is a no-op: the fixture
 * domain is ASCII (netloc.isascii() short-circuits it live). */
static char *rid_urlsplit(const char *raw, rid_split_t *out)
{
    memset(out, 0, sizeof(*out));
    char *ls = rid_lstrip_c0(raw);
    char *url = rid_remove_unsafe(ls);
    free(ls);
    out->scheme = rid_dup("");
    out->netloc = rid_dup("");
    out->query = rid_dup("");
    out->fragment = rid_dup("");
    out->params = NULL;
    out->path = NULL;

    char *rest = url;
    const char *col = strchr(url, ':');
    if (col && col != url
        && ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
        size_t i = (size_t)(col - url);
        int all = 1;
        for (size_t k = 0; k < i; k++)
            if (!rid_scheme_char(url[k])) { all = 0; break; }
        if (all) {
            free(out->scheme);
            out->scheme = rid_lower_ndup(url, i);
            rest = url + i + 1;
        }
    }
    char *url2 = rid_dup(rest);
    free(url);
    if (url2[0] == '/' && url2[1] == '/') {
        /* _splitnetloc(url, 2): earliest of /?# at index >= 2 */
        size_t delim = strlen(url2);
        for (const char *d = "/?#"; *d; d++) {
            const char *f = strchr(url2 + 2, *d);
            if (f && (size_t)(f - url2) < delim) delim = (size_t)(f - url2);
        }
        free(out->netloc);
        out->netloc = rid_ndup(url2 + 2, delim - 2);
        char *rest2 = rid_dup(url2 + delim);
        int have_ob = strchr(out->netloc, '[') != NULL;
        int have_cb = strchr(out->netloc, ']') != NULL;
        if (have_ob != have_cb) {
            free(url2); free(rest2);
            rid_split_free(out);
            return rid_dup("Invalid IPv6 URL");
        }
        if (have_ob && have_cb) {
            char *err = rid_check_bracketed_netloc(out->netloc);
            if (err) {
                free(url2); free(rest2);
                rid_split_free(out);
                return err;
            }
        }
        free(url2);
        url2 = rest2;
    }
    /* fragment first, then query (allow_fragments=True) */
    char *hash = strchr(url2, '#');
    if (hash) {
        free(out->fragment);
        out->fragment = rid_dup(hash + 1);
        *hash = '\0';
    }
    char *ques = strchr(url2, '?');
    if (ques) {
        free(out->query);
        out->query = rid_dup(ques + 1);
        *ques = '\0';
    }
    out->path = rid_dup(url2);
    free(url2);
    return NULL;
}

/* _hostinfo: malloc'd hostname (possibly empty) + port string (NULL when
 * absent/empty). Bracketed hosts lose their brackets here. */
static void rid_hostinfo(const char *netloc, char **hostname, char **portstr)
{
    const char *at = strrchr(netloc, '@');
    const char *hi = at ? at + 1 : netloc;
    const char *ob = strchr(hi, '[');
    const char *hstart, *hend, *pstart;
    if (ob) {
        hstart = ob + 1;
        const char *cb = strchr(hstart, ']');
        if (cb) {
            hend = cb;
            const char *col = strchr(cb + 1, ':');
            pstart = col ? col + 1 : cb + 1 + strlen(cb + 1);
        } else {
            hend = hstart + strlen(hstart);
            pstart = hend;
        }
    } else {
        hstart = hi;
        const char *col = strchr(hi, ':');
        hend = col ? col : hi + strlen(hi);
        pstart = col ? col + 1 : hend;
    }
    *hostname = rid_ndup(hstart, (size_t)(hend - hstart));
    *portstr = *pstart ? rid_dup(pstart) : NULL;
}

/* SplitResult.hostname: None for an empty host; address lowercased, '%'
 * zone kept VERBATIM (RFC 4007 scoped IPv6). */
static char *rid_hostname(const char *netloc)
{
    char *h, *p;
    rid_hostinfo(netloc, &h, &p);
    free(p);
    if (!h[0]) { free(h); return NULL; }
    char *pct = strchr(h, '%');
    char *res;
    if (pct) {
        char *low = rid_lower_ndup(h, (size_t)(pct - h));
        res = rid_xfmt("%s%%%s", low, pct + 1);
        free(low);
    } else {
        res = rid_lower_ascii(h);
    }
    free(h);
    return res;
}

/* SplitResult.port: 0 OK (*port_out = -1 for None), 1 ValueError (*err
 * malloc'd). isdigit+isascii then 0..65535, exact live messages. */
static int rid_port(const char *netloc, long *port_out, char **err)
{
    char *h, *p;
    rid_hostinfo(netloc, &h, &p);
    free(h);
    *port_out = -1;
    *err = NULL;
    if (!p) return 0;
    int ok = 1;
    for (const char *c = p; *c; c++)
        if (*c < '0' || *c > '9') { ok = 0; break; }
    if (!ok) {
        *err = rid_xfmt("Port could not be cast to integer value as '%s'", p);
        free(p);
        return 1;
    }
    /* Python int() is arbitrary precision: skip leading zeros, then a
     * >5-digit remainder is always out of range (strtol would saturate
     * wrongly on long zero-padded strings like "000...00080"). */
    const char *d = p;
    while (*d == '0') d++;
    size_t dl = strlen(d);
    long v = 0;
    if (dl > 5) {
        *err = rid_dup("Port out of range 0-65535");
        free(p);
        return 1;
    }
    for (const char *c = d; *c; c++) v = v * 10 + (*c - '0');
    if (v > 65535) {
        *err = rid_dup("Port out of range 0-65535");
        free(p);
        return 1;
    }
    *port_out = v;
    free(p);
    return 0;
}

static const char *RID_USES_NETLOC[] = {
    "", "ftp", "http", "gopher", "nntp", "telnet", "imap", "wais", "file",
    "mms", "https", "shttp", "snews", "prospero", "rtsp", "rtsps", "rtspu",
    "rsync", "svn", "svn+ssh", "sftp", "nfs", "git", "git+ssh", "ws", "wss",
    "itms-services", NULL
};

static const char *RID_USES_PARAMS[] = {
    "", "ftp", "hdl", "prospero", "http", "imap", "https", "shttp", "rtsp",
    "rtsps", "rtspu", "sip", "sips", "mms", "sftp", "tel", NULL
};

static int rid_in_list(const char *const *list, const char *s)
{
    for (size_t i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0) return 1;
    return 0;
}

/* urlunsplit((scheme, netloc, path, query, fragment)); empty query /
 * fragment project to None (no delimiter appended). */
static char *rid_urlunsplit(const char *scheme, const char *netloc,
                            const char *path, const char *query,
                            const char *fragment)
{
    char *url = rid_dup(path);
    char *nl = NULL;
    if (!netloc || !netloc[0]) {
        if (scheme && scheme[0] && rid_in_list(RID_USES_NETLOC, scheme)
            && (!url[0] || url[0] == '/'))
            nl = rid_dup("");
    } else {
        nl = rid_dup(netloc);
    }
    if (nl) {
        if (url[0] && url[0] != '/') {
            char *t = rid_xfmt("/%s", url);
            free(url); url = t;
        }
        char *t = rid_xfmt("//%s%s", nl, url);
        free(url); url = t;
    } else if (url[0] == '/' && url[1] == '/') {
        char *t = rid_xfmt("//%s", url);
        free(url); url = t;
    }
    if (scheme && scheme[0]) {
        char *t = rid_xfmt("%s:%s", scheme, url);
        free(url); url = t;
    }
    if (query && query[0]) {
        char *t = rid_xfmt("%s?%s", url, query);
        free(url); url = t;
    }
    if (fragment && fragment[0]) {
        char *t = rid_xfmt("%s#%s", url, fragment);
        free(url); url = t;
    }
    free(nl);
    return url;
}

/* urlparse = urlsplit + _splitparams for uses_params schemes: the first
 * ';' at/after the last '/' (or the first ';' when no '/'). */
static char *rid_urlparse(const char *raw, rid_split_t *out)
{
    char *err = rid_urlsplit(raw, out);
    if (err) return err;
    out->params = rid_dup("");
    if (rid_in_list(RID_USES_PARAMS, out->scheme) && strchr(out->path, ';')) {
        const char *slash = strrchr(out->path, '/');
        const char *semi = slash ? strchr(slash, ';') : strchr(out->path, ';');
        if (semi) {
            free(out->params);
            out->params = rid_dup(semi + 1);
            char *np = rid_ndup(out->path, (size_t)(semi - out->path));
            free(out->path);
            out->path = np;
        }
    }
    return NULL;
}

/* ---- Oxford-comma argument list (CPython binding TypeError format) ---- */

static char *rid_oxford(const char *const *names, size_t k)
{
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    for (size_t i = 0; i < k; i++) {
        const char *sep = "";
        if (i > 0) sep = (i == k - 1) ? (k == 2 ? " and " : ", and ") : ", ";
        char *piece = rid_xfmt("%s'%s'", sep, names[i]);
        size_t pl = strlen(piece);
        if (len + pl + 1 > cap) {
            while (len + pl + 1 > cap) cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) { free(piece); free(buf); return NULL; }
            buf = nb;
        }
        memcpy(buf + len, piece, pl + 1);
        len += pl;
        free(piece);
    }
    return buf;
}

/* ===========================================================================
 * ENTRIES
 * =========================================================================== */

/* PoP: rid_normalize_route_base_url @ hermes_cli/route_identity.py:normalize_route_base_url */
json_t *rid_normalize_route_base_url(rid_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char *err = NULL;
    char *raw = rid_str_of(rid_get(in, "base_url"), &err);
    if (!raw) {
        json_t *r = rid_mkraise("RuntimeError", err);
        free(err);
        return r;
    }
    if (!raw[0]) { free(raw); return json_string(""); }
    for (const char *c = raw; *c; c++) {
        if ((unsigned char)*c <= 0x20) {
            json_t *r = json_string(raw);
            free(raw);
            return r;
        }
    }
    /* had_query_delimiter = "?" in raw.split("#", 1)[0] */
    int had_q = 0;
    {
        const char *h = strchr(raw, '#');
        size_t n = h ? (size_t)(h - raw) : strlen(raw);
        for (size_t i = 0; i < n; i++)
            if (raw[i] == '?') { had_q = 1; break; }
    }
    rid_split_t sp;
    char *uerr = rid_urlsplit(raw, &sp);
    if (uerr) {
        /* except (TypeError, ValueError): return raw */
        free(uerr);
        rid_split_free(&sp);
        json_t *r = json_string(raw);
        free(raw);
        return r;
    }
    char *hostname = rid_hostname(sp.netloc);
    if (!sp.scheme[0] || !hostname) {
        free(hostname);
        rid_split_free(&sp);
        json_t *r = json_string(raw);
        free(raw);
        return r;
    }
    char *scheme = rid_lower_ascii(sp.scheme);
    char *host;
    char *pct = strchr(hostname, '%');
    if (pct) {
        char *low = rid_lower_ndup(hostname, (size_t)(pct - hostname));
        host = rid_xfmt("%s%%%s", low, pct + 1);
        free(low);
    } else {
        host = rid_lower_ascii(hostname);
    }
    long port;
    char *perr = NULL;
    if (rid_port(sp.netloc, &port, &perr)) {
        free(perr);
        free(host); free(scheme); free(hostname);
        rid_split_free(&sp);
        json_t *r = json_string(raw);
        free(raw);
        return r;
    }
    /* route_host = parsed.netloc.rsplit("@", 1)[-1] */
    const char *at = strrchr(sp.netloc, '@');
    const char *route_host = at ? at + 1 : sp.netloc;
    if (route_host[0] == '[' || strchr(host, ':')) {
        char *t = rid_xfmt("[%s]", host);
        free(host); host = t;
    }
    if (port >= 0
        && !((strcmp(scheme, "http") == 0 && port == 80)
             || (strcmp(scheme, "https") == 0 && port == 443))) {
        char *t = rid_xfmt("%s:%ld", host, port);
        free(host); host = t;
    }
    if (strchr(sp.netloc, '@')) {
        const char *a2 = strrchr(sp.netloc, '@');
        char *ui = rid_ndup(sp.netloc, (size_t)(a2 - sp.netloc));
        char *t = rid_xfmt("%s@%s", ui, host);
        free(ui); free(host); host = t;
    }
    char *path = rid_dup(sp.path);
    size_t plen = strlen(path);
    if (plen && path[plen - 1] == '/' && !had_q) path[plen - 1] = '\0';
    char *normalized = rid_urlunsplit(scheme, host, path, sp.query, "");
    if (had_q && !sp.query[0]) {
        char *t = rid_xfmt("%s?", normalized);
        free(normalized); normalized = t;
    }
    json_t *ret = json_string(normalized);
    free(normalized); free(path); free(host); free(scheme); free(hostname);
    rid_split_free(&sp);
    free(raw);
    return ret;
}

/* Product adapter: hermes_cli.route_identity.normalize_route_base_url as a
 * malloc'd C string. Invalid input fails closed to "".
 */
char *rid_normalize_route_base_url_cstr(const char *base_url)
{
    json_t *in = json_object();
    json_set(in, "base_url", base_url ? json_string(base_url) : json_null());
    json_t *r = rid_normalize_route_base_url(NULL, in, NULL);
    json_free(in);
    if (!r || r->type != JSON_STRING) { json_free(r); return strdup(""); }
    char *out = strdup(r->str_val ? r->str_val : "");
    json_free(r);
    return out;
}

/* PoP: rid_should_clear_context_pin @ hermes_cli/route_identity.py:should_clear_context_pin */
json_t *rid_should_clear_context_pin(rid_io_t *io, const json_t *in, json_t *st)
{
    (void)st;
    char *err = NULL;
    char *cm = rid_str_of(rid_get(in, "configured_model"), &err);
    if (!cm) {
        json_t *r = rid_mkraise("RuntimeError", err);
        free(err);
        return r;
    }
    char *cm_s = rid_strip_ws(cm);
    free(cm);
    if (cm_s[0]) {
        char *am = rid_str_of(rid_get(in, "active_model"), &err);
        if (!am) {
            free(cm_s);
            json_t *r = rid_mkraise("RuntimeError", err);
            free(err);
            return r;
        }
        char *am_s = rid_strip_ws(am);
        free(am);
        int neq = strcmp(cm_s, am_s) != 0;
        free(am_s);
        if (neq) { free(cm_s); return json_bool(1); }
    }
    free(cm_s);
    /* try: from agent.agent_init import _context_route_mismatch; return ...
     * except Exception: return True. import_ok=false models the halted
     * import (ImportError swallowed, no seam event). */
    if (!json_get_bool(in, "import_ok", true)) return json_bool(1);
    json_t *a = json_object();
    rid_aset_j(a, "configured_base_url", rid_get(in, "configured_base_url"));
    rid_aset_j(a, "active_base_url", rid_get(in, "active_base_url"));
    rid_aset_j(a, "configured_provider", rid_get(in, "configured_provider"));
    rid_aset_j(a, "active_provider", rid_get(in, "active_provider"));
    json_t *r = rid_call(io, "context_route_mismatch", a);
    if (rid_is_raise(r)) {
        if (rid_is_base(rid_kind(r))) return r;   /* BaseException passthrough */
        json_free(r);
        return json_bool(1);                      /* except Exception -> True */
    }
    return r;   /* the callee result UNCHANGED (NULL == None) */
}

/* Product adapter for the CLI wrapper: arg is "configured_model\tactive_model\troutes_match".
 * routes_match=1 models _context_route_mismatch returning False; 0 models True.
 */
static json_t *rid_pin_seam(void *ctx, const char *name, const json_t *args)
{
    (void)name; (void)args;
    return json_bool(*(int *)ctx ? 0 : 1);
}

int rid_should_clear_context_pin_cstr(const char *configured_model,
                                      const char *active_model, int routes_match)
{
    rid_io_t io = { rid_pin_seam, &routes_match };
    json_t *in = json_object();
    json_set(in, "configured_model", configured_model ? json_string(configured_model) : json_null());
    json_set(in, "active_model", active_model ? json_string(active_model) : json_null());
    json_set(in, "configured_base_url", json_string(""));
    json_set(in, "active_base_url", json_string(""));
    json_t *r = rid_should_clear_context_pin(&io, in, NULL);
    json_free(in);
    int out = rid_truth(r) ? 1 : 0;
    json_free(r);
    return out;
}

/* PoP: rid_should_clear_context_pin_async @ hermes_cli/route_identity.py:should_clear_context_pin_async */
json_t *rid_should_clear_context_pin_async(rid_io_t *io, const json_t *in, json_t *st)
{
    static const char *const names[6] = {
        "configured_model", "active_model", "configured_base_url",
        "active_base_url", "configured_provider", "active_provider"
    };
    const json_t *args = rid_get(in, "args");
    if (args && args->type != JSON_ARRAY)
        return rid_mkraise("RuntimeError", "args outside fixture domain (not an array)");
    size_t n = args ? json_len(args) : 0;
    if (n != 6) {
        char *m;
        if (n < 6) {
            size_t k = 6 - n;
            char *list = rid_oxford(names + n, k);
            m = rid_xfmt("should_clear_context_pin() missing %zu required positional argument%s: %s",
                         k, k == 1 ? "" : "s", list ? list : "");
            free(list);
        } else {
            m = rid_xfmt("should_clear_context_pin() takes 6 positional arguments but %zu were given", n);
        }
        json_t *r = rid_mkraise("TypeError", m);
        free(m);
        return r;
    }
    /* asyncio.to_thread runs the ported sibling synchronously (silent). */
    json_t *in2 = json_object();
    for (int i = 0; i < 6; i++) rid_aset_j(in2, names[i], json_get(args, (size_t)i));
    rid_aset_j(in2, "import_ok", rid_get(in, "import_ok"));
    json_t *r = rid_should_clear_context_pin(io, in2, st);
    json_free(in2);
    return r;
}

/* PoP: rid_parse_base_url @ utils/urls.py:_parse_base_url */
json_t *rid_parse_base_url(rid_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    char *err = NULL;
    char *raw = rid_strip_input(rid_get(in, "base_url"), &err);
    if (!raw) {
        json_t *r = rid_mkraise("AttributeError", err);
        free(err);
        return r;
    }
    if (!raw[0]) { free(raw); return NULL; }   /* None */
    char *u = strstr(raw, "://") ? rid_dup(raw) : rid_xfmt("//%s", raw);
    free(raw);
    rid_split_t sp;
    char *uerr = rid_urlparse(u, &sp);
    free(u);
    if (uerr) {
        json_t *r = rid_mkraise("ValueError", uerr);
        free(uerr);
        rid_split_free(&sp);
        return r;
    }
    /* ParseResult namedtuple projects to a 6-string array */
    json_t *arr = json_array();
    json_append(arr, json_string(sp.scheme));
    json_append(arr, json_string(sp.netloc));
    json_append(arr, json_string(sp.path));
    json_append(arr, json_string(sp.params));
    json_append(arr, json_string(sp.query));
    json_append(arr, json_string(sp.fragment));
    rid_split_free(&sp);
    return arr;
}

/* PoP: rid_hostname_of @ utils/urls.py:_hostname_of */
json_t *rid_hostname_of(rid_io_t *io, const json_t *in, json_t *st)
{
    (void)io; (void)st;
    const json_t *parsed = rid_get(in, "parsed");
    if (!rid_truth(parsed)) return json_string("");
    /* ParseResult double: netloc is element 1; a ParseResult is always
     * truthy live (6-tuple), so any present array computes the hostname. */
    const char *nl = "";
    if (parsed->type == JSON_ARRAY && json_len(parsed) >= 2) {
        const char *s = rid_str(json_get(parsed, 1));
        if (s) nl = s;
    }
    char *h = rid_hostname(nl);
    char *low = h ? rid_lower_ascii(h) : rid_dup("");
    free(h);
    rid_rstrip(low, '.');
    json_t *r = json_string(low);
    free(low);
    return r;
}

/* PoP: rid_base_url_hostname @ utils/urls.py:base_url_hostname */
json_t *rid_base_url_hostname(rid_io_t *io, const json_t *in, json_t *st)
{
    json_t *p = rid_parse_base_url(io, in, st);
    if (rid_is_raise(p)) return p;
    json_t *in2 = json_object();
    json_set(in2, "parsed", p ? p : json_null());
    json_t *r = rid_hostname_of(io, in2, st);
    json_free(in2);
    return r;
}

/* Product adapter: utils.base_url_hostname(base_url) as a malloc'd C string.
 * The FLOW entry keeps its JSON envelope for the differential oracle; this
 * adapter lets product callers consume the same ported implementation without
 * constructing envelopes themselves. Invalid input (Python exception) yields NULL.
 */
char *rid_base_url_hostname_cstr(const char *base_url)
{
    json_t *in = json_object();
    json_set(in, "base_url", base_url ? json_string(base_url) : json_null());
    json_t *r = rid_base_url_hostname(NULL, in, NULL);
    json_free(in);
    if (!r || r->type != JSON_STRING) { json_free(r); return NULL; }
    char *out = strdup(r->str_val ? r->str_val : "");
    json_free(r);
    return out;
}

/* PoP: rid_base_url_path @ utils/urls.py:base_url_path */
json_t *rid_base_url_path(rid_io_t *io, const json_t *in, json_t *st)
{
    json_t *p = rid_parse_base_url(io, in, st);
    if (rid_is_raise(p)) return p;
    const char *path = "";
    if (p && p->type == JSON_ARRAY && json_len(p) >= 3) {
        const char *s = rid_str(json_get(p, 2));
        if (s) path = s;
    }
    char *low = rid_lower_ascii(path);
    rid_rstrip(low, '/');
    json_t *r = json_string(low);
    free(low);
    json_free(p);
    return r;
}

/* PoP: rid_base_url_origin @ utils/urls.py:base_url_origin */
json_t *rid_base_url_origin(rid_io_t *io, const json_t *in, json_t *st)
{
    json_t *p = rid_parse_base_url(io, in, st);
    if (rid_is_raise(p)) return p;
    json_t *in2 = json_object();
    /* COPY into in2: json_free(in2) below would otherwise free p, which is
     * still read (scheme/netloc) and freed again at every exit path — a
     * UAF read + double free (0xC0000374 on the origin cases). */
    json_set(in2, "parsed", p ? json_copy(p) : json_null());
    json_t *hj = rid_hostname_of(io, in2, st);
    json_free(in2);
    const char *hostname = rid_str(hj);
    if (!hostname || !hostname[0]) {
        json_free(hj); json_free(p);
        json_t *arr = json_array();
        json_append(arr, json_string(""));
        json_append(arr, json_string(""));
        json_append(arr, json_number(0));
        return arr;
    }
    const char *scheme_raw = "";
    const char *netloc = "";
    if (p && p->type == JSON_ARRAY && json_len(p) >= 2) {
        const char *s0 = rid_str(json_get(p, 0));
        const char *s1 = rid_str(json_get(p, 1));
        if (s0) scheme_raw = s0;
        if (s1) netloc = s1;
    }
    char *scheme = rid_lower_ascii(scheme_raw);
    long port;
    char *perr = NULL;
    long eff;
    if (rid_port(netloc, &port, &perr)) {
        /* except ValueError: not a usable origin */
        free(perr);
        free(scheme);
        json_free(hj); json_free(p);
        json_t *arr = json_array();
        json_append(arr, json_string(""));
        json_append(arr, json_string(""));
        json_append(arr, json_number(0));
        return arr;
    }
    if (port >= 0) eff = port;
    else if (strcmp(scheme, "https") == 0) eff = 443;
    else if (strcmp(scheme, "http") == 0) eff = 80;
    else eff = 0;
    json_t *arr = json_array();
    json_append(arr, json_string(scheme));
    json_append(arr, json_string(hostname));
    json_append(arr, json_number((double)eff));
    free(scheme);
    json_free(hj); json_free(p);
    return arr;
}

/* PoP: rid_same_base_url_origin @ utils/urls.py:same_base_url_origin */
json_t *rid_same_base_url_origin(rid_io_t *io, const json_t *in, json_t *st)
{
    json_t *ina = json_object();
    rid_aset_j(ina, "base_url", rid_get(in, "a"));
    json_t *oa = rid_base_url_origin(io, ina, st);
    json_free(ina);
    if (rid_is_raise(oa)) return oa;
    const char *ha = "";
    if (oa && oa->type == JSON_ARRAY && json_len(oa) >= 2) {
        const char *s = rid_str(json_get(oa, 1));
        if (s) ha = s;
    }
    if (!ha[0]) { json_free(oa); return json_bool(0); }  /* b never evaluated */
    json_t *inb = json_object();
    rid_aset_j(inb, "base_url", rid_get(in, "b"));
    json_t *ob = rid_base_url_origin(io, inb, st);
    json_free(inb);
    if (rid_is_raise(ob)) { json_free(oa); return ob; }
    int eq = 0;
    if (oa && ob && oa->type == JSON_ARRAY && ob->type == JSON_ARRAY
        && json_len(oa) == 3 && json_len(ob) == 3) {
        eq = 1;
        for (size_t i = 0; i < 3; i++) {
            json_t *x = json_get(oa, i), *y = json_get(ob, i);
            if (i == 2) {
                double xv = (x && x->type == JSON_NUMBER) ? x->num_val : 0;
                double yv = (y && y->type == JSON_NUMBER) ? y->num_val : 0;
                if (xv != yv) { eq = 0; break; }
            } else {
                const char *xs = rid_str(x), *ys = rid_str(y);
                if (strcmp(xs ? xs : "", ys ? ys : "") != 0) { eq = 0; break; }
            }
        }
    }
    json_free(oa); json_free(ob);
    return json_bool(eq ? 1 : 0);
}

/* PoP: rid_base_url_host_matches @ utils/urls.py:base_url_host_matches */
json_t *rid_base_url_host_matches(rid_io_t *io, const json_t *in, json_t *st)
{
    json_t *hj = rid_base_url_hostname(io, in, st);
    if (rid_is_raise(hj)) return hj;
    const char *hostname = rid_str(hj);
    if (!hostname) hostname = "";
    char *err = NULL;
    char *dom = rid_strip_input(rid_get(in, "domain"), &err);
    if (!dom) {
        json_free(hj);
        json_t *r = rid_mkraise("AttributeError", err);
        free(err);
        return r;
    }
    char *dl = rid_lower_ascii(dom);
    free(dom);
    rid_rstrip(dl, '.');
    int match = 0;
    if (hostname[0] && dl[0]) {
        if (strcmp(hostname, dl) == 0) {
            match = 1;
        } else {
            char *dotd = rid_xfmt(".%s", dl);
            size_t hl = strlen(hostname), dl2 = strlen(dotd);
            if (hl >= dl2 && strcmp(hostname + hl - dl2, dotd) == 0) match = 1;
            free(dotd);
        }
    }
    free(dl);
    json_free(hj);
    return json_bool(match ? 1 : 0);
}

/* Product adapter: utils.base_url_host_matches(base_url, domain) as C bool.
 * Mirrors the JSON-envelope entry; invalid input (Python exception) fails closed.
 */
bool rid_base_url_host_matches_cstr(const char *base_url, const char *domain)
{
    json_t *in = json_object();
    json_set(in, "base_url", base_url ? json_string(base_url) : json_null());
    json_set(in, "domain", domain ? json_string(domain) : json_null());
    json_t *r = rid_base_url_host_matches(NULL, in, NULL);
    json_free(in);
    if (!r || r->type != JSON_BOOL) { json_free(r); return false; }
    bool ok = r->bool_val;
    json_free(r);
    return ok;
}

