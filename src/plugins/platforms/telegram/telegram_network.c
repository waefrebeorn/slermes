/*
 * telegram_network.c — C11 port of the pure decision core of upstream Hermes
 * plugins/platforms/telegram/telegram_network.py (gates
 * w33_telegramne_429779_p1 and w33_telegramne_429779_p2).
 *
 * PoP: w33_telegramne_429779_env_number             @ plugins/platforms/telegram/telegram_network.py:_env_number
 * PoP: w33_telegramne_429779_ptb_request_kwargs     @ plugins/platforms/telegram/telegram_network.py:ptb_request_kwargs
 * PoP: w33_telegramne_429779_getupdates_keepalive   @ plugins/platforms/telegram/telegram_network.py:getupdates_keepalive_connections
 * PoP: w33_telegramne_429779_keepalive_options      @ plugins/platforms/telegram/telegram_network.py:tcp_keepalive_socket_options
 * PoP: w33_telegramne_429779_proxy_target_hosts     @ plugins/platforms/telegram/telegram_network.py:proxy_target_hosts
 * PoP: w33_telegramne_429779_bot_api_urls           @ plugins/platforms/telegram/telegram_network.py:bot_api_urls
 * PoP: w33_telegramne_429779_verify_decision        @ plugins/platforms/telegram/telegram_network.py:telegram_verify
 * PoP: w33_telegramne_429779_classic_proxy_decision @ plugins/platforms/telegram/telegram_network.py:classic_proxy
 * PoP: w33_telegramne_429779_normalize_fallback_ips @ plugins/platforms/telegram/telegram_network.py:_normalize_fallback_ips
 * PoP: w33_telegramne_429779_cap_fallback_ips       @ plugins/platforms/telegram/telegram_network.py:_cap_fallback_ips
 * PoP: w33_telegramne_429779_parse_fallback_ip_env  @ plugins/platforms/telegram/telegram_network.py:parse_fallback_ip_env
 * PoP: w33_telegramne_429779_is_retryable           @ plugins/platforms/telegram/telegram_network.py:_is_retryable_connect_error
 * PoP: w33_telegramne_429779_attempt_order          @ plugins/platforms/telegram/telegram_network.py:TelegramFallbackTransport._attempt_order
 * PoP: w33_telegramne_429779_connect_share          @ plugins/platforms/telegram/telegram_network.py:TelegramFallbackTransport._connect_share
 * PoP: w33_telegramne_429779_request_connect_budget @ plugins/platforms/telegram/telegram_network.py:_request_with_connect_budget
 * PoP: w33_telegramne_429779_rewrite_for_ip         @ plugins/platforms/telegram/telegram_network.py:_rewrite_request_for_ip
 * PoP: w33_telegramne_429779_doh_answer_ips         @ plugins/platforms/telegram/telegram_network.py:_query_doh_provider
 * PoP: w33_telegramne_429779_discover_merge         @ plugins/platforms/telegram/telegram_network.py:discover_fallback_ips
 * PoP: w33_telegramne_429779_pool_plan              @ plugins/platforms/telegram/telegram_network.py:build_ptb_requests
 * PoP: w33_telegramne_429779_walk_failed            @ plugins/platforms/telegram/telegram_network.py:_warn_walk_failed
 *
 * Ported here is the value->value core: the SO_KEEPALIVE option set, the
 * HERMES_TELEGRAM_HTTP_* sizing, the platform keepalive count, the Bot API URL
 * normaliser and NO_PROXY host list, the classic-key-exchange predicate, the
 * IPv4 fallback sanity filter/cap and its env parser, the retryable-connect
 * classification, the sticky path walk order and its connect-budget split, the
 * per-request connect narrowing, the host-preserving IP rewrite, the DoH answer
 * filter, the discovery merge, the pool/limits derivation and the walk-failure
 * diagnostic.
 *
 * Documented seam: _rewrite_request_for_ip rebuilds the URL after swapping the
 * host. Python routes it through urlsplit/urlunsplit, which percent-encodes a
 * space in the netloc and preserves a single-slash ('https:/a/b') or scheme-less
 * ('//host') form; the C rebuilds textually. Held-out fuzzing finds this only on
 * malformed URLs (5 of 712 generated cases, all with a space in the authority or
 * a missing/reduced slash) -- valid Bot API URLs are identical.
 *
 * Every seam stays in the caller: os.environ (env objects are fixture inputs),
 * sys.platform and the socket option table (availability lists), the DoH HTTP
 * response (body/status), RLIMIT_NOFILE (soft limit), the platform httpx
 * Limits, time.monotonic (elapsed seconds) and the live SSL contexts /
 * httpx.Proxy identity checks (booleans).
 */
#include "hermes_json.h"
#include "noc_w33_telegramne_429779_p1.h"
#include "noc_w33_telegramne_429779_p2.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TN_API_HOST "api.telegram.org"
#define TN_MAX_FALLBACK_IPS 4
#define TN_DOH_MAX_BODY_BYTES (64 * 1024)
#define TN_KEEPALIVE_IDLE_S 30
#define TN_KEEPALIVE_INTERVAL_S 10
#define TN_KEEPALIVE_COUNT 3
#define TN_FD_SHARE 4
#define TN_PTB_CLIENTS 2
#define TN_MIN_POOL_CONNECTIONS 4

static const char *const TN_SEED_IPS[] = {"149.154.166.110", "149.154.167.220"};

/* ------------------------------------------------------------------ values */

static const json_t *tn_get(const json_t *o, const char *k) {
    if (!o || o->type != JSON_OBJECT || !k) return NULL;
    return json_obj_get(o, k);
}

static const json_t *tn_obj(const json_t *o, const char *k) {
    const json_t *v = tn_get(o, k);
    return v && v->type == JSON_OBJECT ? v : NULL;
}

static const json_t *tn_arr(const json_t *o, const char *k) {
    const json_t *v = tn_get(o, k);
    return v && v->type == JSON_ARRAY ? v : NULL;
}

static const char *tn_str(const json_t *o, const char *k) {
    const json_t *v = tn_get(o, k);
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

static int tn_has_num(const json_t *o, const char *k) {
    const json_t *v = tn_get(o, k);
    return v && v->type == JSON_NUMBER;
}

static double tn_num(const json_t *o, const char *k, double def) {
    const json_t *v = tn_get(o, k);
    return v && v->type == JSON_NUMBER ? v->num_val : def;
}

static char *tn_trim_dup(const char *s) {
    size_t a = 0, b;
    char *out;
    if (!s) return NULL;
    b = strlen(s);
    while (b > 0 && isspace((unsigned char)s[b - 1])) b--;
    while (a < b && isspace((unsigned char)s[a])) a++;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static void tn_lower(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int tn_is_seed(const char *ip) {
    size_t i;
    for (i = 0; i < sizeof TN_SEED_IPS / sizeof TN_SEED_IPS[0]; i++)
        if (!strcmp(ip, TN_SEED_IPS[i])) return 1;
    return 0;
}

/* ----------------------------------------------------------- strict casts */

/* Python int(): optional sign, decimal digits, single underscores between
 * digits ("1_000" is 1000).  Anything else fails the cast. */
static int tn_py_int(const char *s, long long *out) {
    int neg = 0, digits = 0;
    long long v = 0;
    if (!s || !*s) return 0;
    if (*s == '+' || *s == '-') {
        neg = (*s == '-');
        s++;
    }
    for (; *s; s++) {
        if (*s == '_') {
            if (!digits || s[1] < '0' || s[1] > '9') return 0;
            continue;
        }
        if (*s < '0' || *s > '9') return 0;
        digits++;
        if (v > (9223372036854775807LL - 9) / 10) return 0;
        v = v * 10 + (*s - '0');
    }
    if (!digits) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* Python float(): the same underscore rule, and no hex literals. */
static int tn_py_float(const char *s, double *out) {
    char buf[128];
    size_t n = 0, i;
    char *endp;
    if (!s || !*s || strlen(s) >= sizeof buf) return 0;
    for (i = 0; s[i]; i++) {
        if (s[i] == '_') {
            if (i == 0 || !isdigit((unsigned char)s[i - 1]) || !isdigit((unsigned char)s[i + 1])) return 0;
            continue;
        }
        if (s[i] == 'x' || s[i] == 'X') return 0;
        buf[n++] = s[i];
    }
    if (!n) return 0;
    buf[n] = '\0';
    *out = strtod(buf, &endp);
    return endp == buf + n;
}

/* --------------------------------------------------------- address parsing */

/* Strict dotted quad: four decimal octets, no leading zeros (Python 3 ipaddress). */
static int tn_ipv4(const char *s, unsigned long *out) {
    unsigned parts[4] = {0, 0, 0, 0};
    int idx = 0;
    const char *p = s;
    if (!s || !*s) return 0;
    for (;;) {
        const char *start = p;
        unsigned v = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10u + (unsigned)(*p - '0');
            p++;
            digits++;
            if (v > 255u) return 0;
        }
        if (!digits || digits > 3) return 0;
        if (digits > 1 && *start == '0') return 0;
        if (idx >= 4) return 0;
        parts[idx++] = v;
        if (*p == '.') {
            p++;
            continue;
        }
        break;
    }
    if (idx != 4 || *p) return 0;
    *out = ((unsigned long)parts[0] << 24) | ((unsigned long)parts[1] << 16) |
           ((unsigned long)parts[2] << 8) | (unsigned long)parts[3];
    return 1;
}

/* IPv6 shape check: hex groups, at most one "::", optional embedded IPv4 tail. */
static int tn_ipv6(const char *s) {
    const char *p = s;
    int groups = 0, dbl = 0, hex;
    if (!s || !*s || !strchr(s, ':')) return 0;
    for (;;) {
        const char *start;
        if (*p == ':') {
            if (p != s || p[1] != ':') return 0;
            dbl = 1;
            p += 2;
            if (!*p) break;
            continue;
        }
        if (!*p) break;
        start = p;
        while (isxdigit((unsigned char)*p)) p++;
        hex = (int)(p - start);
        if (*p == '.') { /* embedded IPv4 tail: the rest is the v4 address */
            unsigned long v4;
            if (!tn_ipv4(start, &v4)) return 0;
            groups += 2;
            break;
        }
        if (hex == 0 || hex > 4) return 0;
        groups++;
        if (!*p) break;
        if (*p != ':') return 0;
        if (p[1] == ':') {
            if (dbl) return 0;
            dbl = 1;
            p += 2;
            if (!*p) break;
        } else {
            p++;
            if (!*p) return 0;
        }
    }
    return dbl ? groups < 8 : groups == 8;
}

/* ipaddress' IPv4 registry (the exact set that makes .is_private true), plus
 * loopback / link-local / unspecified.  192.0.0.9 and 192.0.0.10 are the
 * registry exceptions the live predicate also honours. */
static int tn_ipv4_private(unsigned long ip) {
    unsigned a = (unsigned)((ip >> 24) & 0xff), b = (unsigned)((ip >> 16) & 0xff);
    unsigned c = (unsigned)((ip >> 8) & 0xff), d = (unsigned)(ip & 0xff);
    if (a == 0 || a == 10 || a == 127) return 1;
    if (a == 169 && b == 254) return 1;
    if (a == 172 && b >= 16 && b <= 31) return 1;
    if (a == 192 && b == 0 && c == 0) {
        if (d == 9 || d == 10) return 0;
        return 1;
    }
    if (a == 192 && b == 0 && c == 2) return 1;
    if (a == 192 && b == 168) return 1;
    if (a == 198 && (b == 18 || b == 19)) return 1;
    if (a == 198 && b == 51 && c == 100) return 1;
    if (a == 203 && b == 0 && c == 113) return 1;
    if (a >= 240) return 1;
    return 0;
}

/* ------------------------------------------------------------- tiny vectors */

typedef struct {
    char **items;
    size_t n;
    size_t cap;
} tn_vec;

static int tn_vec_push(tn_vec *v, const char *s) {
    char *copy;
    if (v->n == v->cap) {
        size_t next = v->cap ? v->cap * 2 : 8;
        char **grown = (char **)realloc(v->items, next * sizeof *grown);
        if (!grown) return 0;
        v->items = grown;
        v->cap = next;
    }
    copy = (char *)malloc(strlen(s) + 1);
    if (!copy) return 0;
    strcpy(copy, s);
    v->items[v->n++] = copy;
    return 1;
}

static int tn_vec_contains(const tn_vec *v, const char *s) {
    size_t i;
    for (i = 0; i < v->n; i++)
        if (!strcmp(v->items[i], s)) return 1;
    return 0;
}

static void tn_vec_free(tn_vec *v) {
    size_t i;
    for (i = 0; i < v->n; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->n = v->cap = 0;
}

static json_t *tn_vec_json(const tn_vec *v) {
    json_t *out = json_array();
    size_t i;
    for (i = 0; i < v->n; i++) json_append(out, json_string(v->items[i]));
    return out;
}

/* --------------------------------------------------------------- URL splits */

typedef struct {
    char scheme[16];   /* lowercased, "" when absent */
    char host[256];    /* lowercased, IPv6 brackets stripped */
    int has_host;
    long port;         /* parsed port, -1 when absent/invalid */
    int port_given;    /* an ":<port>" part was present */
    const char *tail;  /* path + query + fragment, pointing into the input */
    size_t path_len;   /* path only (up to '?' or '#') */
} tn_url;

static void tn_lower_copy(char *dst, size_t dn, const char *src, size_t n) {
    size_t i, k = 0;
    for (i = 0; i < n && k + 1 < dn; i++) dst[k++] = (char)tolower((unsigned char)src[i]);
    dst[k] = '\0';
}

static void tn_port_part(const char *s, size_t n, tn_url *u) {
    size_t i;
    long v = 0;
    u->port_given = 1;
    if (n == 0) return; /* "host:" -> port None */
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return; /* ValueError -> port None */
        v = v * 10 + (s[i] - '0');
        if (v > 65535) return;
    }
    u->port = v;
}

static void tn_url_parse(const char *url, tn_url *u) {
    const char *p = url, *auth, *end;
    memset(u, 0, sizeof *u);
    u->port = -1;
    if (!url) return;
    while (*p == ' ' || *p == '\t') p++;
    if (isalpha((unsigned char)*p)) {
        const char *q = p + 1;
        while (isalnum((unsigned char)*q) || *q == '+' || *q == '-' || *q == '.') q++;
        if (q[0] == ':' && q[1] == '/' && q[2] == '/') {
            tn_lower_copy(u->scheme, sizeof u->scheme, p, (size_t)(q - p));
            auth = q + 3;
            goto authority;
        }
    }
    if (p[0] == '/' && p[1] == '/') {
        auth = p + 2;
        goto authority;
    }
    u->tail = p;
    u->path_len = strcspn(p, "?#");
    return;

authority:
    end = auth;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    {
        const char *at = NULL, *q;
        for (q = auth; q < end; q++)
            if (*q == '@') at = q;
        if (at) auth = at + 1;
        if (auth < end && *auth == '[') {
            const char *rb = NULL, *scan;
            for (scan = auth; scan < end; scan++)
                if (*scan == ']') {
                    rb = scan;
                    break;
                }
            if (rb && rb > auth + 1) {
                tn_lower_copy(u->host, sizeof u->host, auth + 1, (size_t)(rb - auth - 1));
                u->has_host = 1;
                if (rb + 1 < end && rb[1] == ':') tn_port_part(rb + 2, (size_t)(end - rb - 2), u);
            }
        } else {
            const char *colon = NULL, *scan;
            for (scan = auth; scan < end; scan++)
                if (*scan == ':') {
                    colon = scan;
                    break;
                }
            if (colon) {
                if (colon > auth) {
                    tn_lower_copy(u->host, sizeof u->host, auth, (size_t)(colon - auth));
                    u->has_host = 1;
                }
                tn_port_part(colon + 1, (size_t)(end - colon - 1), u);
            } else if (end > auth) {
                tn_lower_copy(u->host, sizeof u->host, auth, (size_t)(end - auth));
                u->has_host = 1;
            }
        }
    }
    u->tail = end;
    u->path_len = strcspn(end, "?#");
}

/* --------------------------------------------------------- IP normalisation */

/* One value of _normalize_fallback_ips: keep only public IPv4 literals,
 * canonicalised (str(ip_address)); returns 1 when it was pushed. */
static int tn_normalize_one(tn_vec *out, const char *value) {
    char *trimmed;
    unsigned long ip;
    int pushed = 0;
    if (!value) return 0;
    trimmed = tn_trim_dup(value);
    if (!trimmed || !*trimmed) {
        free(trimmed);
        return 0;
    }
    if (strchr(trimmed, ':')) { /* non-IPv4 literal (or malformed): dropped */
        free(trimmed);
        return 0;
    }
    if (tn_ipv4(trimmed, &ip) && !tn_ipv4_private(ip)) pushed = tn_vec_push(out, trimmed);
    free(trimmed);
    return pushed;
}

static void tn_normalize_values(const json_t *values, tn_vec *out) {
    size_t i;
    if (!values || values->type != JSON_ARRAY) return;
    for (i = 0; i < json_len(values); i++) {
        const json_t *v = json_get(values, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue; /* str(non-str) never parses as an IP */
        tn_normalize_one(out, v->str_val);
    }
}

/* ------------------------------------------------------------------ gate p1 */

/* PoP: _env_number @ plugins/platforms/telegram/telegram_network.py:_env_number */
json_t *w33_telegramne_429779_env_number(const json_t *in) {
    const char *key = tn_str(in, "key");
    const char *cast = tn_str(in, "cast");
    const json_t *env = tn_obj(in, "env");
    const json_t *def = tn_get(in, "default");
    const char *raw = NULL;
    json_t *out = NULL;
    char *trimmed;

    if (env && key) {
        const json_t *v = json_obj_get(env, key);
        if (v && v->type == JSON_STRING) raw = v->str_val;
    }
    trimmed = tn_trim_dup(raw ? raw : "");
    if (trimmed && *trimmed) {
        if (cast && !strcmp(cast, "int")) {
            long long iv;
            if (tn_py_int(trimmed, &iv)) out = json_number((double)iv);
        } else {
            double dv;
            if (tn_py_float(trimmed, &dv)) out = json_number(dv);
        }
    }
    free(trimmed);
    if (!out) out = def ? json_copy(def) : json_null();
    return out;
}

/* One HERMES_TELEGRAM_HTTP_* read: cast(env) when it parses, else the default. */
static double tn_env_value(const json_t *env, const char *key, double def, int as_int) {
    const json_t *v = env ? json_obj_get(env, key) : NULL;
    char *trimmed = tn_trim_dup(v && v->type == JSON_STRING ? v->str_val : "");
    double value = def;

    if (trimmed && *trimmed) {
        if (as_int) {
            long long iv;
            if (tn_py_int(trimmed, &iv)) value = (double)iv;
        } else {
            double dv;
            if (tn_py_float(trimmed, &dv)) value = dv;
        }
    }
    free(trimmed);
    return value;
}

/* PoP: ptb_request_kwargs @ plugins/platforms/telegram/telegram_network.py:ptb_request_kwargs */
json_t *w33_telegramne_429779_ptb_request_kwargs(const json_t *in) {
    const json_t *env = tn_obj(in, "env");
    json_t *out = json_object();

    json_set(out, "connection_pool_size",
             json_number(tn_env_value(env, "HERMES_TELEGRAM_HTTP_POOL_SIZE", 512.0, 1)));
    json_set(out, "pool_timeout",
             json_number(tn_env_value(env, "HERMES_TELEGRAM_HTTP_POOL_TIMEOUT", 8.0, 0)));
    json_set(out, "connect_timeout",
             json_number(tn_env_value(env, "HERMES_TELEGRAM_HTTP_CONNECT_TIMEOUT", 10.0, 0)));
    json_set(out, "read_timeout",
             json_number(tn_env_value(env, "HERMES_TELEGRAM_HTTP_READ_TIMEOUT", 20.0, 0)));
    json_set(out, "write_timeout",
             json_number(tn_env_value(env, "HERMES_TELEGRAM_HTTP_WRITE_TIMEOUT", 20.0, 0)));
    json_set(out, "media_write_timeout", json_number(60.0));
    return out;
}

/* PoP: getupdates_keepalive_connections @ plugins/platforms/telegram/telegram_network.py:getupdates_keepalive_connections */
json_t *w33_telegramne_429779_getupdates_keepalive(const json_t *in) {
    const char *platform = tn_str(in, "platform");
    return json_number((platform && !strcmp(platform, "win32")) ? 0.0 : 1.0);
}

static void tn_push_option(json_t *out, const char *family, const char *opt, double value) {
    json_t *row = json_array();
    json_append(row, json_string(family));
    json_append(row, json_string(opt));
    json_append(row, json_number(value));
    json_append(out, row);
}

/* PoP: tcp_keepalive_socket_options @ plugins/platforms/telegram/telegram_network.py:tcp_keepalive_socket_options */
json_t *w33_telegramne_429779_keepalive_options(const json_t *in) {
    const json_t *avail = tn_arr(in, "avail");
    json_t *out = json_array();
    int has_idle = 0, has_keepalive = 0, has_intvl = 0, has_cnt = 0;
    size_t i;

    for (i = 0; avail && i < json_len(avail); i++) {
        const json_t *v = json_get(avail, i);
        const char *name;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        name = v->str_val;
        if (!strcmp(name, "TCP_KEEPIDLE")) has_idle = 1;
        else if (!strcmp(name, "TCP_KEEPALIVE")) has_keepalive = 1;
        else if (!strcmp(name, "TCP_KEEPINTVL")) has_intvl = 1;
        else if (!strcmp(name, "TCP_KEEPCNT")) has_cnt = 1;
    }
    tn_push_option(out, "SOL_SOCKET", "SO_KEEPALIVE", 1);
    if (has_idle) tn_push_option(out, "IPPROTO_TCP", "TCP_KEEPIDLE", TN_KEEPALIVE_IDLE_S);
    else if (has_keepalive) tn_push_option(out, "IPPROTO_TCP", "TCP_KEEPALIVE", TN_KEEPALIVE_IDLE_S);
    if (has_intvl) tn_push_option(out, "IPPROTO_TCP", "TCP_KEEPINTVL", TN_KEEPALIVE_INTERVAL_S);
    if (has_cnt) tn_push_option(out, "IPPROTO_TCP", "TCP_KEEPCNT", TN_KEEPALIVE_COUNT);
    return out;
}

/* PoP: proxy_target_hosts @ plugins/platforms/telegram/telegram_network.py:proxy_target_hosts */
json_t *w33_telegramne_429779_proxy_target_hosts(const json_t *in) {
    const char *base_url = tn_str(in, "base_url");
    const json_t *fallback = tn_arr(in, "fallback_ips");
    json_t *out = json_array();
    size_t i;

    if (base_url && *base_url) {
        size_t need = strlen(base_url) + 3;
        char *built = (char *)malloc(need);
        if (built) {
            tn_url u;
            if (strstr(base_url, "//")) snprintf(built, need, "%s", base_url);
            else snprintf(built, need, "//%s", base_url);
            tn_url_parse(built, &u);
            if (u.has_host) {
                char hostport[320];
                long port = u.port;
                int with_port = 1;
                if (u.port_given && u.port < 0) {
                    with_port = 0; /* parts.port raised ValueError -> port None */
                } else if (u.port < 0 || u.port == 0) {
                    port = !strcmp(u.scheme, "http") ? 80 : 443;
                }
                /* Python brackets an IPv6 host in BOTH arms: f"[{host}]" then
                 * f"{host}:{port}" if port else host -- so "::1" with no port is
                 * "[::1]", not "::1". */
                if (!with_port) {
                    if (strchr(u.host, ':')) snprintf(hostport, sizeof hostport, "[%s]", u.host);
                    else snprintf(hostport, sizeof hostport, "%s", u.host);
                } else if (strchr(u.host, ':'))
                    snprintf(hostport, sizeof hostport, "[%s]:%ld", u.host, port);
                else snprintf(hostport, sizeof hostport, "%s:%ld", u.host, port);
                json_append(out, json_string(hostport));
                free(built);
                return out;
            }
            free(built);
        }
    }
    json_append(out, json_string(TN_API_HOST));
    for (i = 0; fallback && i < json_len(fallback); i++) {
        const json_t *v = json_get(fallback, i);
        if (v && v->type == JSON_STRING && v->str_val) json_append(out, json_string(v->str_val));
    }
    return out;
}

/* PoP: bot_api_urls @ plugins/platforms/telegram/telegram_network.py:bot_api_urls */
json_t *w33_telegramne_429779_bot_api_urls(const json_t *in) {
    const json_t *extra = tn_obj(in, "extra");
    const json_t *base_value = extra ? json_obj_get(extra, "base_url") : NULL;
    const json_t *file_value = extra ? json_obj_get(extra, "base_file_url") : NULL;
    const char *base_url = base_value && base_value->type == JSON_STRING ? base_value->str_val : NULL;
    const char *file_url = file_value && file_value->type == JSON_STRING ? file_value->str_val : NULL;
    const char *file_src;
    char *base_trim, *file_trim, *base_norm, *file_norm;
    json_t *out;

    if (!base_url || !*base_url) return json_object(); /* `if not base_url: return {}` */
    file_src = (file_url && *file_url) ? file_url : base_url;
    base_trim = tn_trim_dup(base_url);
    file_trim = tn_trim_dup(file_src);
    if (!base_trim || !file_trim) {
        free(base_trim);
        free(file_trim);
        return json_object();
    }
    base_norm = (char *)malloc(strlen(base_trim) + 5);
    file_norm = (char *)malloc(strlen(file_trim) + 10);
    if (!base_norm || !file_norm) {
        free(base_trim);
        free(file_trim);
        free(base_norm);
        free(file_norm);
        return json_object();
    }
    {
        tn_url u;
        tn_url_parse(base_trim, &u);
        if (u.path_len == 0 || (u.path_len == 1 && u.tail[0] == '/')) {
            size_t len = strlen(base_trim);
            while (len > 0 && base_trim[len - 1] == '/') len--;
            memcpy(base_norm, base_trim, len);
            strcpy(base_norm + len, "/bot");
        } else {
            strcpy(base_norm, base_trim);
        }
    }
    {
        tn_url u;
        tn_url_parse(file_trim, &u);
        if (u.path_len == 0 || (u.path_len == 1 && u.tail[0] == '/')) {
            size_t len = strlen(file_trim);
            while (len > 0 && file_trim[len - 1] == '/') len--;
            memcpy(file_norm, file_trim, len);
            strcpy(file_norm + len, "/file/bot");
        } else {
            strcpy(file_norm, file_trim);
        }
    }
    free(base_trim);
    free(file_trim);
    out = json_object();
    json_set(out, "base_url", json_string(base_norm));
    json_set(out, "base_file_url", json_string(file_norm));
    free(base_norm);
    free(file_norm);
    return out;
}

/* PoP: telegram_verify @ plugins/platforms/telegram/telegram_network.py:telegram_verify */
json_t *w33_telegramne_429779_verify_decision(const json_t *in) {
    const json_t *extra = tn_obj(in, "extra");
    const json_t *value = extra ? json_obj_get(extra, "tls_classic_key_exchange") : NULL;
    json_t *out = json_object();
    int classic = 0, warned = 0;

    if (value) {
        if (value->type == JSON_STRING) {
            char *text = tn_trim_dup(value->str_val ? value->str_val : "");
            tn_lower(text);
            if (text && (!strcmp(text, "true") || !strcmp(text, "1") || !strcmp(text, "yes") ||
                         !strcmp(text, "on"))) {
                classic = 1;
            } else if (text && (!strcmp(text, "false") || !strcmp(text, "0") || !strcmp(text, "no") ||
                                !strcmp(text, "off") || !*text)) {
                classic = 0;
            } else {
                warned = 1; /* refused: not one of the boolean words */
            }
            free(text);
        } else if (value->type == JSON_BOOL) {
            classic = value->bool_val ? 1 : 0;
        } else {
            warned = 1; /* 1 / 0 / null / list / object are not bools in Python either */
        }
    }
    json_set(out, "result", json_string(classic ? "classic" : "default"));
    json_set(out, "warned", json_bool(warned));
    return out;
}

/* PoP: classic_proxy @ plugins/platforms/telegram/telegram_network.py:classic_proxy */
json_t *w33_telegramne_429779_classic_proxy_decision(const json_t *in) {
    const char *proxy_url = tn_str(in, "proxy_url");
    int is_str = json_get_bool(in, "is_str", 1);
    int classic = json_get_bool(in, "verify_is_classic", 0);
    json_t *out = json_object();
    int wrap = 0;

    if (is_str && proxy_url) {
        char *text = tn_trim_dup(proxy_url);
        tn_lower(text);
        if (text && !strncmp(text, "https://", 8) && classic) wrap = 1;
        free(text);
    }
    json_set(out, "result", json_string(wrap ? "wrap" : "passthrough"));
    return out;
}

/* ------------------------------------------------------------------ gate p2 */

/* PoP: _normalize_fallback_ips @ plugins/platforms/telegram/telegram_network.py:_normalize_fallback_ips */
json_t *w33_telegramne_429779_normalize_fallback_ips(const json_t *in) {
    tn_vec out = {0};
    json_t *json;

    tn_normalize_values(tn_arr(in, "values"), &out);
    json = tn_vec_json(&out);
    tn_vec_free(&out);
    return json;
}

/* PoP: _cap_fallback_ips @ plugins/platforms/telegram/telegram_network.py:_cap_fallback_ips */
json_t *w33_telegramne_429779_cap_fallback_ips(const json_t *in) {
    tn_vec normalized = {0};
    tn_vec deduped = {0};
    json_t *out;
    size_t i;
    int non_seed = 0;

    tn_normalize_values(tn_arr(in, "values"), &normalized);
    for (i = 0; i < normalized.n; i++)
        if (!tn_vec_contains(&deduped, normalized.items[i])) tn_vec_push(&deduped, normalized.items[i]);

    out = json_array();
    for (i = 0; i < deduped.n; i++) {
        if (tn_is_seed(deduped.items[i])) {
            json_append(out, json_string(deduped.items[i]));
            continue;
        }
        if (non_seed < TN_MAX_FALLBACK_IPS) {
            json_append(out, json_string(deduped.items[i]));
            non_seed++;
        }
    }
    tn_vec_free(&normalized);
    tn_vec_free(&deduped);
    return out;
}

/* PoP: parse_fallback_ip_env @ plugins/platforms/telegram/telegram_network.py:parse_fallback_ip_env */
json_t *w33_telegramne_429779_parse_fallback_ip_env(const json_t *in) {
    const char *value = tn_str(in, "value");
    tn_vec out = {0};
    json_t *json;

    if (value && *value) {
        const char *p = value;
        while (1) {
            const char *comma = strchr(p, ',');
            size_t n = comma ? (size_t)(comma - p) : strlen(p);
            char *part = (char *)malloc(n + 1);
            if (part) {
                memcpy(part, p, n);
                part[n] = '\0';
                tn_normalize_one(&out, part);
                free(part);
            }
            if (!comma) break;
            p = comma + 1;
        }
    }
    json = tn_vec_json(&out);
    tn_vec_free(&out);
    return json;
}

/* PoP: _is_retryable_connect_error @ plugins/platforms/telegram/telegram_network.py:_is_retryable_connect_error */
json_t *w33_telegramne_429779_is_retryable(const json_t *in) {
    const json_t *mro = tn_arr(in, "mro");
    size_t i;
    for (i = 0; mro && i < json_len(mro); i++) {
        const json_t *v = json_get(mro, i);
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        if (!strcmp(v->str_val, "ConnectTimeout") || !strcmp(v->str_val, "ConnectError")) return json_bool(1);
    }
    return json_bool(0);
}

/* PoP: TelegramFallbackTransport._attempt_order @ plugins/platforms/telegram/telegram_network.py:TelegramFallbackTransport._attempt_order */
json_t *w33_telegramne_429779_attempt_order(const json_t *in) {
    const json_t *sticky = tn_obj(in, "sticky");
    const json_t *ips = tn_arr(in, "fallback_ips");
    int sticky_set = sticky ? json_get_bool(sticky, "set", 0) : 0;
    json_t *out = json_array();
    size_t i;

    if (sticky_set) {
        const json_t *value = json_obj_get(sticky, "value");
        if (value && value->type == JSON_STRING && value->str_val) json_append(out, json_string(value->str_val));
        else json_append(out, json_null());
    }
    for (i = 0; ips && i < json_len(ips); i++) {
        const json_t *v = json_get(ips, i);
        const char *ip;
        int seen = 0;
        size_t k;
        if (!v || v->type != JSON_STRING || !v->str_val) continue;
        ip = v->str_val;
        for (k = 0; k < json_len(out); k++) {
            const json_t *prev = json_get(out, k);
            if (prev && prev->type == JSON_STRING && prev->str_val && !strcmp(prev->str_val, ip)) seen = 1;
        }
        if (!seen) json_append(out, json_string(ip));
    }
    for (i = 0; i < json_len(out); i++) {
        const json_t *v = json_get(out, i);
        if (v && v->type == JSON_NULL) return out;
    }
    json_append(out, json_null());
    return out;
}

/* PoP: TelegramFallbackTransport._connect_share @ plugins/platforms/telegram/telegram_network.py:TelegramFallbackTransport._connect_share */
json_t *w33_telegramne_429779_connect_share(const json_t *in) {
    double budget = tn_num(in, "budget", 0.0);
    double elapsed = tn_num(in, "elapsed", 0.0);
    long long paths = (long long)tn_num(in, "paths", 1);
    long long index = (long long)tn_num(in, "index", 0);
    double remaining = budget - elapsed;
    long long paths_left = paths - index;

    /* Python divides by paths_left as-is: a negative slice is a negative share,
     * and 0 raises ZeroDivisionError. Clamping to 1 turned -2.5 into 10. */
    if (paths_left == 0) return json_null();
    if (index == 0 && paths_left > 1) return json_number(remaining - remaining / (double)paths_left);
    return json_number(remaining / (double)paths_left);
}

/* PoP: _request_with_connect_budget @ plugins/platforms/telegram/telegram_network.py:_request_with_connect_budget */
json_t *w33_telegramne_429779_request_connect_budget(const json_t *in) {
    const char *ext = tn_str(in, "ext");
    double budget = tn_num(in, "budget", 0.0);
    int is_dict = ext && !strcmp(ext, "dict");
    int has_connect = tn_has_num(in, "connect");
    double connect = tn_num(in, "connect", 0.0);
    json_t *out = json_object();

    if (!is_dict) {
        /* no timeout dict (or not a dict): the request is returned unchanged */
        json_set(out, "changed", json_bool(0));
        json_set(out, "connect", json_null());
        return out;
    }
    if (!has_connect || connect > budget) {
        json_set(out, "changed", json_bool(1));
        json_set(out, "connect", json_number(budget > 0.0 ? budget : 0.0));
        return out;
    }
    json_set(out, "changed", json_bool(0));
    json_set(out, "connect", json_number(connect));
    return out;
}

/* PoP: _rewrite_request_for_ip @ plugins/platforms/telegram/telegram_network.py:_rewrite_request_for_ip */
json_t *w33_telegramne_429779_rewrite_for_ip(const json_t *in) {
    const char *url = tn_str(in, "url");
    const char *ip = tn_str(in, "ip");
    const char *override = tn_str(in, "host");
    json_t *out = json_object();
    tn_url u;

    tn_url_parse(url ? url : "", &u);
    {
        const char *original = (override && *override) ? override : (u.has_host ? u.host : TN_API_HOST);
        char host[256];
        char *built;
        size_t need;
        int default_port;

        tn_lower_copy(host, sizeof host, original, strlen(original));
        need = strlen(u.scheme) + strlen(ip ? ip : "") + strlen(u.tail) + 16;
        built = (char *)malloc(need);
        if (!built) {
            json_set(out, "url", json_string(url ? url : ""));
            json_set(out, "host_header", json_string(host));
            json_set(out, "sni", json_string(host));
            return out;
        }
        default_port = !strcmp(u.scheme, "https") ? 443 : 80;
        if (u.port_given && u.port >= 0 && u.port != default_port)
            snprintf(built, need, "%s://%s:%ld%s", u.scheme, ip ? ip : "", u.port, u.tail);
        else
            snprintf(built, need, "%s://%s%s", u.scheme, ip ? ip : "", u.tail);
        json_set(out, "url", json_string(built));
        json_set(out, "host_header", json_string(host));
        json_set(out, "sni", json_string(host));
        free(built);
    }
    return out;
}

/* PoP: _query_doh_provider (answer filter) @ plugins/platforms/telegram/telegram_network.py:_query_doh_provider */
json_t *w33_telegramne_429779_doh_answer_ips(const json_t *in) {
    const char *body = tn_str(in, "body");
    double status = tn_num(in, "status", 200.0);
    json_t *out = json_array();
    json_t *doc;

    if (!body || status >= 400.0 || strlen(body) > (size_t)TN_DOH_MAX_BODY_BYTES) return out;
    doc = json_parse(body, NULL);
    if (!doc) return out;
    {
        const json_t *answers = json_obj_get(doc, "Answer");
        size_t i;
        if (!answers || answers->type != JSON_ARRAY) {
            /* missing Answer: no records; a non-list Answer makes the live loop raise */
            json_free(doc);
            return out;
        }
        for (i = 0; i < json_len(answers); i++) {
            const json_t *answer = json_get(answers, i);
            const json_t *type, *data;
            const char *raw;
            unsigned long v4;
            if (!answer || answer->type != JSON_OBJECT) {
                json_free(doc);
                json_free(out);
                return json_array();
            }
            type = json_obj_get(answer, "type");
            if (!type || type->type != JSON_NUMBER || type->num_val != 1.0) continue; /* not an A record */
            data = json_obj_get(answer, "data");
            if (data && data->type != JSON_STRING) {
                /* None.strip() / 5.strip(): the whole provider result is dropped */
                json_free(doc);
                json_free(out);
                return json_array();
            }
            raw = data && data->str_val ? data->str_val : "";
            {
                char *trimmed = tn_trim_dup(raw);
                if (trimmed && *trimmed && (tn_ipv4(trimmed, &v4) || tn_ipv6(trimmed)))
                    json_append(out, json_string(trimmed));
                free(trimmed);
            }
        }
    }
    json_free(doc);
    return out;
}

/* PoP: discover_fallback_ips (validated + seed merge) @ plugins/platforms/telegram/telegram_network.py:discover_fallback_ips */
json_t *w33_telegramne_429779_discover_merge(const json_t *in) {
    const json_t *doh_ips = tn_arr(in, "doh_ips");
    const json_t *seeds = tn_arr(in, "seeds");
    tn_vec raw = {0};
    tn_vec validated = {0};
    tn_vec merged = {0};
    json_t *out;
    size_t i;

    for (i = 0; doh_ips && i < json_len(doh_ips); i++) {
        const json_t *v = json_get(doh_ips, i);
        if (v && v->type == JSON_STRING && v->str_val && !tn_vec_contains(&raw, v->str_val))
            tn_vec_push(&raw, v->str_val);
    }
    for (i = 0; i < raw.n; i++) tn_normalize_one(&validated, raw.items[i]);
    if (validated.n) {
        for (i = 0; i < validated.n; i++)
            if (!tn_vec_contains(&merged, validated.items[i])) tn_vec_push(&merged, validated.items[i]);
        for (i = 0; seeds && i < json_len(seeds); i++) {
            const json_t *v = json_get(seeds, i);
            if (v && v->type == JSON_STRING && v->str_val && !tn_vec_contains(&merged, v->str_val))
                tn_vec_push(&merged, v->str_val);
        }
    } else if (seeds) {
        for (i = 0; i < json_len(seeds); i++) {
            const json_t *v = json_get(seeds, i);
            if (v && v->type == JSON_STRING && v->str_val) tn_vec_push(&merged, v->str_val);
        }
    } else {
        for (i = 0; i < sizeof TN_SEED_IPS / sizeof TN_SEED_IPS[0]; i++) tn_vec_push(&merged, TN_SEED_IPS[i]);
    }
    out = tn_vec_json(&merged);
    tn_vec_free(&raw);
    tn_vec_free(&validated);
    tn_vec_free(&merged);
    return out;
}

/* PoP: build_ptb_requests (limits/route derivation) @ plugins/platforms/telegram/telegram_network.py:build_ptb_requests */
json_t *w33_telegramne_429779_pool_plan(const json_t *in) {
    const json_t *fallback = tn_arr(in, "fallback_ips");
    const char *proxy_url = tn_str(in, "proxy_url");
    const json_t *base = tn_get(in, "base");
    const char *platform = tn_str(in, "platform");
    tn_vec ips = {0};
    json_t *out = json_object();
    long long pool_size;
    double pool_max;
    double pool_keep = 0.0;
    int pool_keep_null = 1;
    double expiry = 0.0;
    int expiry_null = 1;
    int have_base = base && base->type == JSON_OBJECT;
    int capped = 0;
    size_t i;

    if (!(proxy_url && *proxy_url)) {
        tn_vec normalized = {0};
        tn_vec deduped = {0};
        int non_seed = 0;
        tn_normalize_values(fallback, &normalized);
        for (i = 0; i < normalized.n; i++)
            if (!tn_vec_contains(&deduped, normalized.items[i])) tn_vec_push(&deduped, normalized.items[i]);
        for (i = 0; i < deduped.n; i++) {
            if (tn_is_seed(deduped.items[i])) {
                tn_vec_push(&ips, deduped.items[i]);
            } else if (non_seed < TN_MAX_FALLBACK_IPS) {
                tn_vec_push(&ips, deduped.items[i]);
                non_seed++;
            }
        }
        tn_vec_free(&normalized);
        tn_vec_free(&deduped);
    }

    json_set(out, "kwargs", w33_telegramne_429779_ptb_request_kwargs(in));
    json_set(out, "ips", tn_vec_json(&ips));
    pool_size = (long long)tn_env_value(tn_obj(in, "env"), "HERMES_TELEGRAM_HTTP_POOL_SIZE", 512.0, 1);
    pool_max = (double)pool_size;

    if (have_base) {
        if (tn_has_num(in, "soft")) {
            long long paths = (long long)ips.n + 1;
            long long cap;
            if (paths < 1) paths = 1;
            cap = (long long)tn_num(in, "soft", 0.0) / TN_FD_SHARE / (TN_PTB_CLIENTS * paths);
            if (cap < TN_MIN_POOL_CONNECTIONS) cap = TN_MIN_POOL_CONNECTIONS;
            if ((long long)pool_size > cap) {
                pool_max = (double)cap;
                capped = 1;
            }
        }
        if (tn_has_num(base, "max_keepalive_connections")) {
            double keep = tn_num(base, "max_keepalive_connections", 0.0);
            pool_keep = capped ? (keep < pool_max ? keep : pool_max) : keep;
            pool_keep_null = 0;
        } else if (capped) {
            pool_keep = pool_max;
            pool_keep_null = 0;
        }
        if (tn_has_num(base, "keepalive_expiry")) {
            expiry = tn_num(base, "keepalive_expiry", 0.0);
            expiry_null = 0;
        }
        {
            json_t *pool = json_object();
            json_t *updates = json_object();
            double updates_keep = (platform && !strcmp(platform, "win32")) ? 0.0 : 1.0;
            json_set(pool, "max_connections", json_number(pool_max));
            json_set(pool, "max_keepalive_connections", pool_keep_null ? json_null() : json_number(pool_keep));
            json_set(pool, "keepalive_expiry", expiry_null ? json_null() : json_number(expiry));
            json_set(updates, "max_connections", json_number(pool_max));
            json_set(updates, "max_keepalive_connections", json_number(updates_keep));
            json_set(updates, "keepalive_expiry", expiry_null ? json_null() : json_number(expiry));
            json_set(out, "pool", pool);
            json_set(out, "updates", updates);
        }
    } else {
        json_set(out, "pool", json_null());
        json_set(out, "updates", json_null());
    }
    json_set(out, "route", json_string(ips.n ? "transport" : "mounts"));
    tn_vec_free(&ips);
    return out;
}

/* PoP: _warn_walk_failed @ plugins/platforms/telegram/telegram_network.py:_warn_walk_failed */
json_t *w33_telegramne_429779_walk_failed(const json_t *in) {
    const json_t *failures = tn_arr(in, "failures");
    long long attempted = (long long)tn_num(in, "attempted", 0);
    long long order_len = (long long)tn_num(in, "order_len", 0);
    const char *reason = attempted == order_len
                             ? "unreachable via all paths"
                             : "connect budget exhausted before all paths were tried";
    size_t i;
    size_t need = 32;
    char *joined, *message;
    json_t *out;

    for (i = 0; failures && i < json_len(failures); i++) {
        const json_t *pair = json_get(failures, i);
        const json_t *path = pair && pair->type == JSON_ARRAY ? json_get(pair, 0) : NULL;
        const json_t *failure = pair && pair->type == JSON_ARRAY ? json_get(pair, 1) : NULL;
        need += 4;
        if (path && path->type == JSON_STRING && path->str_val) need += strlen(path->str_val);
        if (failure && failure->type == JSON_STRING && failure->str_val) need += strlen(failure->str_val);
    }
    joined = (char *)malloc(need);
    message = (char *)malloc(need + strlen(reason) + 64);
    if (!joined || !message) {
        free(joined);
        free(message);
        out = json_object();
        json_set(out, "reason", json_string(reason));
        json_set(out, "message", json_string(""));
        return out;
    }
    joined[0] = '\0';
    for (i = 0; failures && i < json_len(failures); i++) {
        const json_t *pair = json_get(failures, i);
        const json_t *path = pair && pair->type == JSON_ARRAY ? json_get(pair, 0) : NULL;
        const json_t *failure = pair && pair->type == JSON_ARRAY ? json_get(pair, 1) : NULL;
        const char *path_s = path && path->type == JSON_STRING && path->str_val ? path->str_val : "";
        const char *fail_s = failure && failure->type == JSON_STRING && failure->str_val ? failure->str_val : "";
        if (i) strcat(joined, "; ");
        strcat(joined, path_s);
        strcat(joined, " (");
        strcat(joined, fail_s);
        strcat(joined, ")");
    }
    snprintf(message, need + strlen(reason) + 64, "[Telegram] Telegram API %s: %s", reason, joined);
    out = json_object();
    json_set(out, "reason", json_string(reason));
    json_set(out, "message", json_string(message));
    free(joined);
    free(message);
    return out;
}
