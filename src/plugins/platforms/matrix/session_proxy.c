/*
 * session_proxy.c -- real C11 port of upstream Hermes
 *   plugins/platforms/matrix/session_proxy.py.
 *
 * PoP: w22_sessionpro_f01e6e_socks_proxy @ plugins/platforms/matrix/session_proxy.py:matrix_redaction_proxy
 * PoP: w22_sessionpro_f01e6e_redaction_proxy @ plugins/platforms/matrix/session_proxy.py:matrix_redaction_proxy
 *
 * `matrix_redaction_proxy` is three lines of decision wrapped around two reads:
 * whether `aiohttp_socks` imports, and the process proxy environment. Both are
 * seams here (an argument and the caller's own env map), and the environment
 * read -- gateway.platforms.base_log_safety.env_proxy_for_redaction with its
 * NO_PROXY bypass rule -- is ported rather than skipped: the leak-safety choice
 * ("a value whose rendering cannot show it in full wins") is the module's whole
 * point, so it is the part that must not drift.
 */
#include "hermes_json.h"
#include "noc_w22_sessionpro_f01e6e.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every <scheme>_proxy a proxy-reading transport honours. */
static const char *const SP_ENV_KEYS[] = {
    "https_proxy", "http_proxy", "all_proxy", "wss_proxy", "ws_proxy", "socks_proxy",
    "grpc_proxy", NULL
};

#define SP_MAX_VALUES 32
#define SP_MAX_NO_PROXY 8

typedef struct {
    const char *values[SP_MAX_VALUES];
    size_t nvalues;
    const char *no_proxy[SP_MAX_NO_PROXY];
    size_t n_no_proxy;
    int has_no_proxy;         /* some spelling of no_proxy carries a value */
    char *owned[SP_MAX_VALUES + SP_MAX_NO_PROXY];
    size_t n_owned;
} sp_env;

static void sp_lower(const char *in, char *out, size_t cap) {
    size_t i = 0;
    if (!in) in = "";
    while (in[i] && i + 1 < cap) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        out[i] = c;
        i++;
    }
    out[i] = '\0';                          /* NUL-terminate the shortened copy */
}

static int sp_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip() over ASCII whitespace. */
static char *sp_strip(const char *s) {
    size_t n, start = 0;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    while (start < n && sp_is_space((unsigned char)s[start])) start++;
    while (n > start && sp_is_space((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, n - start);
    out[n - start] = '\0';
    return out;
}

int w22_sessionpro_f01e6e_socks_proxy(const char *proxy_url) {
    const char *sep;
    char scheme[32];
    size_t n;
    if (!proxy_url || !proxy_url[0]) return 0;
    sep = strstr(proxy_url, "://");
    n = sep ? (size_t)(sep - proxy_url) : strlen(proxy_url);
    if (n >= sizeof(scheme)) return 0;
    memcpy(scheme, proxy_url, n);
    scheme[n] = '\0';
    sp_lower(scheme, scheme, sizeof(scheme));
    return !strncmp(scheme, "socks", 5) ? 1 : 0;
}

/* --- the shapes safe_proxy_url_for_log / _shown_in_full see -------------- */

typedef struct {
    char scheme[32];
    char netloc[600];
    char host[256];
    long port;
    int has_netloc;
    int has_port;
    int port_bad;
    int at_outside_netloc;
    int host_plausible;
    int authority_only;      /* scheme://host[:port] plus trailing slashes only */
} sp_url;

static int sp_scheme_char(int c, int first) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;
    if (first) return 0;
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

static int sp_count(const char *s, char c) {
    int n = 0;
    for (; *s; s++) if (*s == c) n++;
    return n;
}

static int sp_plausible_host(const char *host) {
    const char *p = host;
    const char *pct;
    if (!host[0]) return 0;
    pct = strchr(host, '%');
    for (; *p && p != pct; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
              || c == '_' || c == '.' || c == '-' || c == ':')) {
            return 0;
        }
    }
    if (pct) {
        for (p = pct + 1; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                  || c == '_' || c == '.' || c == '-')) {
                return 0;
            }
        }
    }
    return 1;
}

/* urlsplit-shaped parse of the value after `value if "://" in value else
 * "http://" + value`, which is how both callers normalise a proxy string. */
static void sp_parse(const char *value, sp_url *u) {
    char buf[1024];
    char *p;
    char *sep;
    char *authority;
    char *end;
    char *at;
    size_t n;
    size_t i;

    memset(u, 0, sizeof(*u));
    u->port = -1;
    if (!value) return;
    snprintf(buf, sizeof(buf), "%s", value);
    /* urlsplit lstrips C0 controls and space and drops \t, \r, \n. */
    {
        char *s = buf;
        char *d = buf;
        while (*s && (unsigned char)*s <= 0x20) s++;
        while (*s) {
            if (*s != '\t' && *s != '\r' && *s != '\n') *d++ = *s;
            s++;
        }
        *d = '\0';
    }
    if (!strstr(buf, "://")) {
        char tmp[1100];
        snprintf(tmp, sizeof(tmp), "http://%s", buf);
        snprintf(buf, sizeof(buf), "%s", tmp);
    }
    p = buf;
    sep = strstr(p, "://");
    if (!sep) return;
    n = (size_t)(sep - p);
    if (n == 0 || n >= sizeof(u->scheme)) return;
    if (!sp_scheme_char((unsigned char)p[0], 1)) return;
    for (i = 1; i < n; i++) {
        if (!sp_scheme_char((unsigned char)p[i], 0)) return;
    }
    memcpy(u->scheme, p, n);
    u->scheme[n] = '\0';
    sp_lower(u->scheme, u->scheme, sizeof(u->scheme));
    p = sep + 3;                        /* past ": //": the authority starts here */
    authority = p;
    end = authority;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    n = (size_t)(end - authority);
    if (n >= sizeof(u->netloc)) n = sizeof(u->netloc) - 1;
    memcpy(u->netloc, authority, n);
    u->netloc[n] = '\0';
    if (!u->netloc[0]) return;
    u->has_netloc = 1;

    at = strrchr(u->netloc, '@');
    {
        const char *hp = at ? at + 1 : u->netloc;
        if (hp[0] == '[') {
            char *close = strchr(hp, ']');
            if (!close) { u->port_bad = 1; return; }
            n = (size_t)(close - hp - 1);
            if (n >= sizeof(u->host)) n = sizeof(u->host) - 1;
            memcpy(u->host, hp + 1, n);
            u->host[n] = '\0';
            hp = close + 1;
        } else {
            const char *colon = strchr(hp, ':');
            n = colon ? (size_t)(colon - hp) : strlen(hp);
            if (n >= sizeof(u->host)) n = sizeof(u->host) - 1;
            memcpy(u->host, hp, n);
            u->host[n] = '\0';
            hp = colon ? colon : hp + strlen(hp);
        }
        if (*hp == ':') {
            const char *q = hp + 1;
            char *stop = NULL;
            long v;
            if (!(*q >= '0' && *q <= '9')) {
                u->port_bad = 1;
            } else {
                v = strtol(q, &stop, 10);
                if (!stop || stop == q || v < 0 || v > 65535) u->port_bad = 1;
                else { u->has_port = 1; u->port = v; }
            }
            hp = stop ? stop : q;
        }
        /* Nothing but slashes may follow the authority. */
        while (*hp == '/') hp++;
        u->authority_only = (*hp == '\0') && !u->port_bad && u->host[0] != '\0'
                            && at == NULL;
    }
    /* hostname: lowercase the host part, keep a %zone's case. */
    {
        char *pct = strchr(u->host, '%');
        char zone[128];
        zone[0] = '\0';
        if (pct) {
            snprintf(zone, sizeof(zone), "%s", pct);
            *pct = '\0';
        }
        sp_lower(u->host, u->host, sizeof(u->host));
        if (zone[0]) {
            size_t used = strlen(u->host);
            if (used + strlen(zone) + 1 < sizeof(u->host)) {
                memcpy(u->host + used, zone, strlen(zone) + 1);
            }
        }
    }
    u->host_plausible = sp_plausible_host(u->host);
    u->at_outside_netloc = sp_count(buf, '@') != sp_count(u->netloc, '@');
}

/* _shown_in_full: rendering prints all of the value. */
static int sp_shown_in_full(const char *value) {
    sp_url u;
    sp_parse(value, &u);
    return u.authority_only;
}

/* safe_proxy_url_for_log(...) == "<unparseable URL redacted>" */
static int sp_placeholder(const char *value) {
    sp_url u;
    sp_parse(value, &u);
    if (!u.has_netloc) return 1;
    if (u.at_outside_netloc) return 1;
    if (u.port_bad) return 1;
    if (!u.host_plausible) return 1;
    return 0;
}

/* --- urllib's proxy_bypass_environment ---------------------------------- */

static int sp_ends_with(const char *s, const char *suffix) {
    size_t ns = strlen(s), nf = strlen(suffix);
    if (nf > ns) return 0;
    return !strcmp(s + ns - nf, suffix);
}

/* urllib.parse._splitport: "<host>:<digits>" -> "<host>", else the host. */
static void sp_hostonly(const char *host, char *out, size_t cap) {
    const char *colon = strrchr(host, ':');
    if (colon) {
        const char *p = colon + 1;
        int digits = 1;
        while (*p) {
            if (*p < '0' || *p > '9') { digits = 0; break; }
            p++;
        }
        if (digits) {
            size_t n = (size_t)(colon - host);
            if (n >= cap) n = cap - 1;
            memcpy(out, host, n);
            out[n] = '\0';
            return;
        }
    }
    snprintf(out, cap, "%s", host);
}

static int sp_bypass_env(const char *host, const char *no_proxy) {
    char lower[512];
    char hostonly[512];
    char scratch[1024];
    char *p;

    sp_lower(host, lower, sizeof(lower));
    sp_hostonly(lower, hostonly, sizeof(hostonly));
    if (!strcmp(no_proxy, "*")) return 1;
    snprintf(scratch, sizeof(scratch), "%s", no_proxy);
    p = scratch;
    while (p && *p) {
        char *comma = strchr(p, ',');
        char *name = p;
        size_t n;
        if (comma) *comma = '\0';
        while (*name == ' ' || *name == '\t') name++;
        n = strlen(name);
        while (n && (name[n - 1] == ' ' || name[n - 1] == '\t')) name[--n] = '\0';
        if (n) {
            char base[512];
            char dotted[520];
            while (*name == '.') name++;
            sp_lower(name, base, sizeof(base));
            if (!strcmp(hostonly, base) || !strcmp(lower, base)) return 1;
            snprintf(dotted, sizeof(dotted), ".%s", base);
            if (sp_ends_with(hostonly, dotted) || sp_ends_with(lower, dotted)) return 1;
        }
        p = comma ? comma + 1 : NULL;
    }
    return 0;
}

/* base_log_safety._bypasses(no_proxy, host, netloc) */
static int sp_bypasses(const char *no_proxy, const char *host, const char *netloc) {
    char scratch[1024];
    char *p;
    int exact = 0;

    if (!strcmp(no_proxy, "*")) return 1;
    if (!host || !host[0] || strchr(no_proxy, '*')) return 0;
    {
        const char *q;
        for (q = no_proxy; *q; q++) if ((unsigned char)*q > 0x7f) return 0;
        for (q = netloc ? netloc : ""; *q; q++) if ((unsigned char)*q > 0x7f) return 0;
        for (q = host; *q; q++) if ((unsigned char)*q > 0x7f) return 0;
    }
    {
        char lower[512];
        sp_lower(host, lower, sizeof(lower));
        if (strstr(lower, "xn--")) return 0;
    }
    if (!sp_bypass_env(host, no_proxy)) return 0;
    if (!sp_bypass_env(netloc ? netloc : "", no_proxy)) return 0;
    /* httplib2 compares entries unstripped and case-sensitively; httpx's
     * `.suffix` skips the apex. */
    snprintf(scratch, sizeof(scratch), "%s", no_proxy);
    p = scratch;
    while (p && *p) {
        char *comma = strchr(p, ',');
        size_t n;
        if (comma) *comma = '\0';
        n = strlen(p);
        if (n && !strcmp(p, host)) exact = 1;
        if (n > 1 && p[0] == '.' && sp_ends_with(host, p) && strcmp(host, p + 1)) exact = 1;
        if (exact) return 1;
        p = comma ? comma + 1 : NULL;
    }
    return 0;
}

/* --- env_proxy_for_redaction -------------------------------------------- */

/* urlsplit(homeserver) -> (hostname, netloc after any userinfo) */
static void sp_target(const char *url, char *host, size_t host_cap,
                      char *netloc, size_t netloc_cap) {
    sp_url u;
    char *at;
    host[0] = '\0';
    netloc[0] = '\0';
    if (!url || !url[0]) return;
    sp_parse(url, &u);
    if (!u.has_netloc) return;
    snprintf(netloc, netloc_cap, "%s", u.netloc);
    at = strrchr(netloc, '@');
    if (at) {
        char tmp[600];
        snprintf(tmp, sizeof(tmp), "%s", at + 1);
        snprintf(netloc, netloc_cap, "%s", tmp);
    }
    snprintf(host, host_cap, "%s", u.host);
}

static void sp_env_free(sp_env *e) {
    size_t i;
    for (i = 0; i < e->n_owned; i++) free(e->owned[i]);
    e->n_owned = 0;
    e->nvalues = 0;
    e->n_no_proxy = 0;
    e->has_no_proxy = 0;
}

static void sp_env_read(const json_t *env, sp_env *e) {
    size_t i;
    memset(e, 0, sizeof(*e));
    if (!env || env->type != JSON_OBJECT) return;
    for (i = 0; i < env->c.count; i++) {
        const char *key = env->c.keys[i];
        json_t *v = env->c.items[i];
        char lkey[64];
        char *stripped;
        size_t k;
        int is_proxy = 0;
        if (!key || !v || v->type != JSON_STRING) continue;
        stripped = sp_strip(v->str_val);
        if (!stripped || !stripped[0]) { free(stripped); continue; }
        sp_lower(key, lkey, sizeof(lkey));
        for (k = 0; SP_ENV_KEYS[k]; k++) {
            if (!strcmp(lkey, SP_ENV_KEYS[k])) { is_proxy = 1; break; }
        }
        if (!strcmp(lkey, "no_proxy")) {
            /* os.environ is the caller's: on this host every spelling addresses
             * the same variable, so any case counts as NO_PROXY being set. */
            if (e->n_no_proxy < SP_MAX_NO_PROXY) {
                e->no_proxy[e->n_no_proxy++] = stripped;
                e->owned[e->n_owned++] = stripped;
                e->has_no_proxy = 1;
            } else {
                free(stripped);
            }
            continue;
        }
        if (is_proxy && e->nvalues < SP_MAX_VALUES) {
            e->values[e->nvalues++] = stripped;
            e->owned[e->n_owned++] = stripped;
            continue;
        }
        free(stripped);
    }
}

/* _no_proxy_bypasses_all(values, targets): keeping the error text in full is
 * only for readability, so it is taken only when no library could have used a
 * proxy. */
static int sp_no_proxy_bypasses_all(const sp_env *e, const char *homeserver) {
    size_t i;
    char host[256];
    char netloc[600];

    if (!e->has_no_proxy || e->n_no_proxy == 0) return 0;
    for (i = 0; i < e->nvalues; i++) {
        if (sp_placeholder(e->values[i])) return 0;
    }
    for (i = 0; i < e->nvalues; i++) {
        if (!sp_shown_in_full(e->values[i])) {
            sp_url u;
            sp_parse(e->values[i], &u);
            if (strcmp(u.scheme, "http") && strcmp(u.scheme, "https")) return 0;
        }
    }
    if (!homeserver || !homeserver[0]) return 0;
    sp_target(homeserver, host, sizeof(host), netloc, sizeof(netloc));
    if (!host[0]) return 0;
    for (i = 0; i < e->n_no_proxy; i++) {
        if (!sp_bypasses(e->no_proxy[i], host, netloc)) return 0;
    }
    return 1;
}

json_t *w22_sessionpro_f01e6e_redaction_proxy(const char *proxy_url, const char *homeserver,
                                              int socks_connector_available, int trust_env,
                                              const json_t *env) {
    sp_env e;
    json_t *out = NULL;
    size_t i;

    if (proxy_url && proxy_url[0]) {
        int socks = w22_sessionpro_f01e6e_socks_proxy(proxy_url);
        /* A SOCKS proxy the session could not install: the session fell back
         * to the environment, so that is what redaction must look at. */
        if (!(socks && !socks_connector_available)) return json_string(proxy_url);
    }
    if (!trust_env) return json_null();
    sp_env_read(env, &e);
    if (!e.nvalues) { sp_env_free(&e); return json_null(); }
    if (sp_no_proxy_bypasses_all(&e, homeserver)) {
        sp_env_free(&e);
        return json_null();
    }
    for (i = 0; i < e.nvalues; i++) {
        if (!sp_shown_in_full(e.values[i])) {
            out = json_string(e.values[i]);
            break;
        }
    }
    if (!out) out = json_string(e.values[0]);
    sp_env_free(&e);
    return out;
}
