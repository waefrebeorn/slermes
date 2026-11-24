/*
 * destination.c -- real C11 port of upstream Hermes
 *   plugins/config_backends/remote/destination.py.
 *
 * PoP: w22_destinatio_e2c9bb_is_loopback @ plugins/config_backends/remote/destination.py:_is_loopback
 * PoP: w22_destinatio_e2c9bb_display_url @ plugins/config_backends/remote/destination.py:display_url
 * PoP: w22_destinatio_e2c9bb_checked_url @ plugins/config_backends/remote/destination.py:checked_url
 *
 * The transport policy Remote Config applies before it sends a credential:
 * https only (plain http for loopback alone), exactly one unambiguous origin,
 * and a refusal that never repeats what it refuses. `open_credentialed` -- the
 * urllib opener whose 3xx handler raises -- is the live network seam and stays
 * upstream; the three decisions it is built around are here, with urlsplit's
 * component split (and its ValueError text) reproduced.
 */
#include "hermes_json.h"
#include "noc_w22_destinatio_e2c9bb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- a NUL-terminated buffer builder ------------------------------------ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int bad;
} dt_buf;

static void dt_buf_init(dt_buf *b) {
    b->cap = 128;
    b->len = 0;
    b->bad = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
    else b->bad = 1;
}

static void dt_buf_add(dt_buf *b, const char *s, size_t n) {
    if (b->bad) return;
    if (!s || n == 0) return;                 /* empty path: nothing appended */
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) { b->bad = 1; return; }
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';                    /* NUL-terminate after each append */
}

static void dt_buf_adds(dt_buf *b, const char *s) {
    if (s) dt_buf_add(b, s, strlen(s));
}

static char *dt_buf_take(dt_buf *b) {
    if (b->bad) { free(b->buf); b->buf = NULL; return NULL; }
    b->buf[b->len] = '\0';
    return b->buf;
}

/* Python repr(str) for the operator-supplied names these messages quote. */
static void dt_repr_into(dt_buf *b, const char *s) {
    char quote = '\'';
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    dt_buf_add(b, &quote, 1);
    while (*p) {
        unsigned char c = *p++;
        if (c == (unsigned char)quote || c == '\\') {
            char esc[2];
            esc[0] = '\\';
            esc[1] = (char)c;
            dt_buf_add(b, esc, 2);
        } else if (c == '\n') {
            dt_buf_adds(b, "\\n");
        } else if (c == '\r') {
            dt_buf_adds(b, "\\r");
        } else if (c == '\t') {
            dt_buf_adds(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)c);
            dt_buf_adds(b, esc);
        } else {
            dt_buf_add(b, (const char *)&c, 1);
        }
    }
    dt_buf_add(b, &quote, 1);
}

static void dt_lower(char *s) {
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 32 + 64);
    }
}

/* --- urlsplit ----------------------------------------------------------- */

typedef struct {
    int ok;                    /* urlsplit did not raise */
    char err[96];              /* the ValueError message when it did */
    char scheme[32];           /* urlsplit lowercases it */
    char netloc[600];
    char path[1600];
    char query[600];
    char frag[600];
    /* derived, exactly as SplitResult's properties compute them */
    int user_none;
    char user[256];
    int pass_none;
    char pass[256];
    int host_none;
    char host[256];
    int port_none;
    int port;
    char port_text[64];
} dt_url;

static int dt_scheme_char(int c, int first) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;
    if (first) return 0;
    return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

static void dt_copy(char *dst, size_t cap, const char *src, size_t n) {
    if (n >= cap) n = cap - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';                             /* NUL-terminate the slice */
}

static void dt_urlsplit(const char *raw, dt_url *u) {
    char buf[2048];
    char *rest;
    char *colon;

    memset(u, 0, sizeof(*u));
    u->ok = 1;
    u->user_none = 1;
    u->pass_none = 1;
    u->host_none = 1;
    u->port_none = 1;

    /* urlsplit lstrips C0 controls and space and drops \t, \r, \n. */
    {
        const char *s = raw ? raw : "";
        char *d = buf;
        char *stop = buf + sizeof(buf) - 1;
        while (*s && (unsigned char)*s <= 0x20) s++;
        while (*s && d < stop) {
            if (*s != '\t' && *s != '\r' && *s != '\n') *d++ = *s;
            s++;
        }
        *d = '\0';
    }
    rest = buf;

    colon = strchr(buf, ':');
    if (colon && colon != buf) {
        size_t n = (size_t)(colon - buf);
        unsigned char first = (unsigned char)buf[0];
        int is_scheme = ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) ? 1 : 0;
        size_t k;
        for (k = 0; is_scheme && k < n; k++) {
            if (!dt_scheme_char((unsigned char)buf[k], 0)) is_scheme = 0;
        }
        if (is_scheme) {
            dt_copy(u->scheme, sizeof(u->scheme), buf, n);
            dt_lower(u->scheme);
            rest = colon + 1;
        }
    }
    if (rest[0] == '/' && rest[1] == '/') {
        char *auth = rest + 2;
        char *end = auth;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        dt_copy(u->netloc, sizeof(u->netloc), auth, (size_t)(end - auth));
        rest = end;
        if ((strchr(u->netloc, '[') && !strchr(u->netloc, ']'))
            || (strchr(u->netloc, ']') && !strchr(u->netloc, '['))) {
            u->ok = 0;
            snprintf(u->err, sizeof(u->err), "Invalid IPv6 URL");
            return;
        }
    }
    {
        char *hash = strchr(rest, '#');
        if (hash) {
            dt_copy(u->frag, sizeof(u->frag), hash + 1, strlen(hash + 1));
            *hash = '\0';
        }
        {
            char *qmark = strchr(rest, '?');
            if (qmark) {
                dt_copy(u->query, sizeof(u->query), qmark + 1, strlen(qmark + 1));
                *qmark = '\0';
            }
        }
    }
    dt_copy(u->path, sizeof(u->path), rest, strlen(rest));

    /* _userinfo / _hostinfo over the netloc */
    if (u->netloc[0]) {
        char *at = strrchr(u->netloc, '@');
        const char *hostinfo = u->netloc;
        if (at) {
            const char *ui = u->netloc;
            size_t ui_len = (size_t)(at - ui);
            const char *pc = (const char *)memchr(ui, ':', ui_len);
            u->user_none = 0;
            if (pc) {
                dt_copy(u->user, sizeof(u->user), ui, (size_t)(pc - ui));
                u->pass_none = 0;
                dt_copy(u->pass, sizeof(u->pass), pc + 1, (size_t)(at - (pc + 1)));
            } else {
                dt_copy(u->user, sizeof(u->user), ui, ui_len);
                u->pass_none = 1;
            }
            hostinfo = at + 1;
        }
        {
            const char *open = strchr(hostinfo, '[');
            char hostname[512];
            char port[64];
            if (open) {
                const char *bracketed = open + 1;
                const char *close = strchr(bracketed, ']');
                if (close) {
                    dt_copy(hostname, sizeof(hostname), bracketed, (size_t)(close - bracketed));
                    close++;
                    if (close[0] == ':') dt_copy(port, sizeof(port), close + 1, strlen(close + 1));
                    else port[0] = '\0';
                } else {
                    dt_copy(hostname, sizeof(hostname), bracketed, strlen(bracketed));
                    port[0] = '\0';
                }
            } else {
                const char *pc = strchr(hostinfo, ':');
                if (pc) {
                    dt_copy(hostname, sizeof(hostname), hostinfo, (size_t)(pc - hostinfo));
                    dt_copy(port, sizeof(port), pc + 1, strlen(pc + 1));
                } else {
                    dt_copy(hostname, sizeof(hostname), hostinfo, strlen(hostinfo));
                    port[0] = '\0';
                }
            }
            if (hostname[0]) {
                /* hostname: lowercase the host, keep a %zone's case */
                char *pct = strchr(hostname, '%');
                char zone[128];
                zone[0] = '\0';
                if (pct) {
                    snprintf(zone, sizeof(zone), "%s", pct);
                    *pct = '\0';
                }
                dt_lower(hostname);
                if (zone[0]) {
                    size_t used = strlen(hostname);
                    if (used + strlen(zone) + 1 < sizeof(hostname)) {
                        memcpy(hostname + used, zone, strlen(zone) + 1);
                    }
                }
                u->host_none = 0;
                snprintf(u->host, sizeof(u->host), "%s", hostname);
            }
            if (port[0]) {
                u->port_none = 0;
                snprintf(u->port_text, sizeof(u->port_text), "%s", port);
            }
        }
    }
}

/* SplitResult.port: raises ValueError on a malformed or out-of-range port. */
static int dt_port_error(const dt_url *u, char *msg, size_t cap) {
    const char *p;
    long v;
    if (u->port_none) return 0;
    for (p = u->port_text; *p; p++) {
        if (*p < '0' || *p > '9') {
            dt_buf b;
            dt_buf_init(&b);
            dt_buf_adds(&b, "Port could not be cast to integer value as ");
            dt_repr_into(&b, u->port_text);
            {
                char *text = dt_buf_take(&b);
                snprintf(msg, cap, "%s", text ? text : "");
                free(text);
            }
            return 1;
        }
    }
    v = strtol(u->port_text, NULL, 10);
    if (v < 0 || v > 65535) {
        snprintf(msg, cap, "Port out of range 0-65535");
        return 1;
    }
    return 0;
}

/* --- ipaddress.is_loopback ---------------------------------------------- */

static int dt_ipv4_loopback(const char *host) {
    int part = 0, value = 0, digits = 0, first = -1;
    const char *p = host;
    for (;;) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            if (digits == 0 && c == '0' && p[1] >= '0' && p[1] <= '9') return 0;  /* leading zero */
            value = value * 10 + (c - '0');
            if (value > 255) return 0;
            digits++;
            if (digits > 3) return 0;
        } else if (c == '.' || c == '\0') {
            if (digits == 0) return 0;
            if (first < 0) first = value;
            part++;
            value = 0;
            digits = 0;
            if (c == '\0') break;
        } else {
            return 0;
        }
        p++;
    }
    if (part != 4) return 0;
    return first == 127;
}

/* Parse an IPv6 literal into 16 bytes; 0 when it is not one. */
static int dt_ipv6_bytes(const char *host, unsigned char out[16]) {
    unsigned int groups[8];
    int ngroups = 0;
    int gap = -1;
    const char *p = host;
    int i;

    memset(out, 0, 16);
    if (!*p) return 0;
    if (p[0] == ':' && p[1] == ':') {
        gap = 0;
        p += 2;
    } else if (p[0] == ':') {
        return 0;
    }
    while (*p) {
        const char *tok_end = p;
        while (*tok_end && *tok_end != ':') tok_end++;
        if (memchr(p, '.', (size_t)(tok_end - p))) {
            /* an embedded IPv4 tail: ::ffff:127.0.0.1 */
            char tmp[64];
            size_t len = (size_t)(tok_end - p);
            int a = 0, b = 0, c2 = 0, d2 = 0;
            if (len == 0 || len >= sizeof(tmp) || ngroups > 6) return 0;
            memcpy(tmp, p, len);
            tmp[len] = '\0';
            if (sscanf(tmp, "%d.%d.%d.%d", &a, &b, &c2, &d2) != 4) return 0;
            if (a < 0 || a > 255 || b < 0 || b > 255 || c2 < 0 || c2 > 255
                || d2 < 0 || d2 > 255) {
                return 0;
            }
            groups[ngroups++] = (unsigned int)((a << 8) | b);
            groups[ngroups++] = (unsigned int)((c2 << 8) | d2);
            p = tok_end;
            break;
        }
        {
            unsigned int v = 0;
            int digits = 0;
            while (p < tok_end) {
                char c = *p;
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else return 0;
                v = v * 16 + (unsigned int)d;
                if (v > 0xFFFF) return 0;
                digits++;
                p++;
            }
            if (digits == 0 || ngroups >= 8) return 0;
            groups[ngroups++] = v;
        }
        if (*p == ':') {
            p++;
            if (*p == ':') {
                if (gap >= 0) return 0;
                gap = ngroups;
                p++;
            }
        } else if (*p) {
            return 0;
        }
    }
    if (gap < 0 && ngroups != 8) return 0;
    if (gap >= 0 && ngroups >= 8) return 0;
    if (gap < 0) {
        for (i = 0; i < 8; i++) {
            out[2 * i] = (unsigned char)(groups[i] >> 8);
            out[2 * i + 1] = (unsigned char)(groups[i] & 0xFF);
        }
    } else {
        int tail = ngroups - gap;
        for (i = 0; i < gap; i++) {
            out[2 * i] = (unsigned char)(groups[i] >> 8);
            out[2 * i + 1] = (unsigned char)(groups[i] & 0xFF);
        }
        for (i = 0; i < tail; i++) {
            int idx = 8 - tail + i;
            out[2 * idx] = (unsigned char)(groups[gap + i] >> 8);
            out[2 * idx + 1] = (unsigned char)(groups[gap + i] & 0xFF);
        }
    }
    return 1;
}

static int dt_ipv6_loopback(const char *host) {
    char literal[128];
    unsigned char bytes[16];
    char *zone;
    int i;
    snprintf(literal, sizeof(literal), "%s", host);
    zone = strchr(literal, '%');
    if (zone) *zone = '\0';
    if (!dt_ipv6_bytes(literal, bytes)) return 0;
    for (i = 0; i < 15; i++) {
        if (bytes[i] != 0) break;
    }
    if (i == 15 && bytes[15] == 1) return 1;                    /* ::1 */
    for (i = 0; i < 10; i++) if (bytes[i] != 0) return 0;
    if (bytes[10] != 0xFF || bytes[11] != 0xFF) return 0;       /* ::ffff:a.b.c.d */
    return bytes[12] == 127;
}

int w22_destinatio_e2c9bb_is_loopback(const char *host) {
    if (!host || !host[0]) return 0;
    if (!strcmp(host, "localhost")) return 1;
    if (strchr(host, ':')) return dt_ipv6_loopback(host);
    return dt_ipv4_loopback(host);
}

/* --- urlunsplit --------------------------------------------------------- */

static int dt_uses_netloc(const char *scheme) {
    static const char *const SCHEMES[] = {
        "ftp", "http", "gopher", "nntp", "telnet", "imap", "wais", "file", "mms", "https",
        "shttp", "snews", "prospero", "rtsp", "rtspu", "rsync", "svn", "svn+ssh", "sftp",
        "nfs", "git", "git+ssh", "ws", "wss", "itms-services", NULL
    };
    int i;
    if (!scheme[0]) return 0;
    for (i = 0; SCHEMES[i]; i++) {
        if (!strcmp(scheme, SCHEMES[i])) return 1;
    }
    return 0;
}

static char *dt_urlunsplit(const char *scheme, const char *netloc, const char *path,
                           const char *query, const char *frag) {
    dt_buf b;
    int needs = 0;
    dt_buf_init(&b);
    if ((netloc && netloc[0])
        || (scheme[0] && dt_uses_netloc(scheme) && !(path[0] == '/' && path[1] == '/'))) {
        needs = 1;
    }
    if (needs) {
        if (path[0] && path[0] != '/') dt_buf_adds(&b, "/");
        dt_buf_adds(&b, "//");
        dt_buf_adds(&b, netloc ? netloc : "");
        dt_buf_adds(&b, path);
    } else {
        dt_buf_adds(&b, path);
    }
    if (scheme[0]) {
        /* prefix the scheme after the netloc part is built */
        char *tail = dt_buf_take(&b);
        dt_buf c;
        dt_buf_init(&c);
        dt_buf_adds(&c, scheme);
        dt_buf_adds(&c, ":");
        dt_buf_adds(&c, tail ? tail : "");
        free(tail);
        b = c;
    }
    if (query[0]) {
        dt_buf_adds(&b, "?");
        dt_buf_adds(&b, query);
    }
    if (frag[0]) {
        dt_buf_adds(&b, "#");
        dt_buf_adds(&b, frag);
    }
    return dt_buf_take(&b);
}

/* --- display_url -------------------------------------------------------- */

/* The `except ValueError` path: cut at the last '@'. */
static char *dt_cut_at(dt_buf *b, const char *text) {
    const char *at = strrchr(text, '@');
    dt_buf_adds(b, at ? at + 1 : text);
    return dt_buf_take(b);
}

json_t *w22_destinatio_e2c9bb_display_url(const char *url) {
    dt_url u;
    char msg[128];
    json_t *out;
    const char *src = url ? url : "";

    dt_urlsplit(src, &u);
    if (u.ok && !dt_port_error(&u, msg, sizeof(msg))) {
        char netloc[600];
        if (u.user_none && u.pass_none) return json_string(src);
        if (!u.host_none) {
            char host[300];
            if (strchr(u.host, ':')) snprintf(host, sizeof(host), "[%s]", u.host);
            else snprintf(host, sizeof(host), "%s", u.host);
            if (!u.port_none) {
                long port = strtol(u.port_text, NULL, 10);
                snprintf(netloc, sizeof(netloc), "%s:%ld", host, port);
            } else {
                snprintf(netloc, sizeof(netloc), "%s", host);
            }
        } else {
            netloc[0] = '\0';
        }
        {
            char *text = dt_urlunsplit(u.scheme, netloc, u.path, u.query, u.frag);
            out = json_string(text ? text : "");
            free(text);
        }
        return out;
    }
    {
        /* head, sep, tail = url.partition("//") */
        const char *sep = strstr(src, "//");
        dt_buf b;
        dt_buf_init(&b);
        if (sep) {
            dt_buf_add(&b, src, (size_t)(sep - src));
            dt_buf_adds(&b, "//");
            dt_cut_at(&b, sep + 2);
        } else {
            dt_cut_at(&b, src);
        }
        {
            char *text = dt_buf_take(&b);
            out = json_string(text ? text : "");
            free(text);
        }
    }
    return out;
}

/* --- checked_url -------------------------------------------------------- */

static json_t *dt_refuse(const char *message) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(message ? message : ""));
    return out;
}

json_t *w22_destinatio_e2c9bb_checked_url(const char *url, const char *env_name) {
    dt_url u;
    char msg[128];
    const char *name = env_name ? env_name : "";
    const char *src = url ? url : "";
    dt_buf b;
    char *message;
    json_t *out;

    dt_urlsplit(src, &u);
    if (!u.ok || dt_port_error(&u, msg, sizeof(msg))) {
        json_t *shown = w22_destinatio_e2c9bb_display_url(src);
        dt_buf_init(&b);
        dt_buf_adds(&b, name);
        dt_buf_adds(&b, "=");
        dt_repr_into(&b, shown ? shown->str_val : "");
        dt_buf_adds(&b, " is not a valid URL: ");
        dt_buf_adds(&b, u.ok ? msg : u.err);
        message = dt_buf_take(&b);
        json_free(shown);
        out = dt_refuse(message ? message : "");
        free(message);
        return out;
    }
    {
        /* host = (parts.hostname or "").lower() */
        char host[256];
        snprintf(host, sizeof(host), "%s", u.host_none ? "" : u.host);
        dt_lower(host);
        if (!host[0]) {
            json_t *shown = w22_destinatio_e2c9bb_display_url(src);
            dt_buf_init(&b);
            dt_buf_adds(&b, name);
            dt_buf_adds(&b, "=");
            dt_repr_into(&b, shown ? shown->str_val : "");
            dt_buf_adds(&b, " has no host.");
            message = dt_buf_take(&b);
            json_free(shown);
            out = dt_refuse(message ? message : "");
            free(message);
            return out;
        }
        if (!u.user_none || !u.pass_none || u.query[0] || u.frag[0]) {
            dt_buf_init(&b);
            dt_buf_adds(&b, name);
            dt_buf_adds(&b, " must be a plain origin and path (no user info, query or "
                            "fragment); Remote Config does not send credentials to an "
                            "ambiguous destination.");
            message = dt_buf_take(&b);
            out = dt_refuse(message ? message : "");
            free(message);
            return out;
        }
        if (!strcmp(u.scheme, "https")
            || (!strcmp(u.scheme, "http") && w22_destinatio_e2c9bb_is_loopback(host))) {
            out = json_object();
            json_set(out, "ok", json_bool(1));
            json_set(out, "url", json_string(src));
            return out;
        }
        dt_buf_init(&b);
        dt_buf_adds(&b, name);
        dt_buf_adds(&b, " must be an https:// URL (plain http is accepted only for localhost, "
                        "127.0.0.1 or ::1); Remote Config does not send credentials to ");
        dt_buf_adds(&b, u.scheme[0] ? u.scheme : "no");
        dt_buf_adds(&b, "://");
        dt_buf_adds(&b, host);
        dt_buf_adds(&b, ".");
        message = dt_buf_take(&b);
        out = dt_refuse(message ? message : "");
        free(message);
        return out;
    }
}
