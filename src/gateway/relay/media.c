/*
 * media.c -- real C11 port of upstream Hermes gateway/relay/media.py (pure half).
 *
 * ``RelayMediaClient.upload*`` / ``download*`` are live network wrappers (urllib in a
 * thread executor) and are not ported.  Everything that decides, formats or parses --
 * the dial-URL -> base-URL mapping, the same-origin re-host check, the credentials
 * predicate, the foreign-rehost warning decision, RFC 6266 Content-Disposition
 * parsing and the attachment-filename cleaner they lean on -- is ported exactly.
 *
 * PoP: w19_media_dc1e86_media_base_url @ gateway/relay/media.py:media_base_url
 * PoP: w19_media_dc1e86_is_relay_media_url @ gateway/relay/media.py:RelayMediaClient.is_relay_media_url
 * PoP: w19_media_dc1e86_enabled @ gateway/relay/media.py:RelayMediaClient.enabled
 * PoP: w19_media_dc1e86_clean_attachment_filename @ gateway/platforms/event.py:clean_attachment_filename
 * PoP: w19_media_dc1e86_content_disposition_filename @ gateway/relay/media.py:_content_disposition_filename
 * PoP: w19_media_dc1e86_warn_foreign_rehost @ gateway/relay/media.py:RelayMediaClient._warn_foreign_rehost
 */
#include "hermes_json.h"
#include "noc_w19_media_dc1e86.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_FILENAME_CHARS 255

/* -- small string helpers (C11 has no strdup) --------------------------------- */

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *dup_range(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int is_ascii_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static void lower_in_place(char *s) {
    for (; s && *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* -- UTF-8 helpers ----------------------------------------------------------- */

/* Decode one UTF-8 code point; invalid bytes come back as their raw value, length 1. */
static size_t u8_next(const char *s, unsigned *cp) {
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80) { *cp = u[0]; return 1; }
    if ((u[0] & 0xE0) == 0xC0 && (u[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x1F) << 6) | (unsigned)(u[1] & 0x3F);
        return 2;
    }
    if ((u[0] & 0xF0) == 0xE0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x0F) << 12) | ((unsigned)(u[1] & 0x3F) << 6) | (unsigned)(u[2] & 0x3F);
        return 3;
    }
    if ((u[0] & 0xF8) == 0xF0 && (u[1] & 0xC0) == 0x80 && (u[2] & 0xC0) == 0x80 && (u[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(u[0] & 0x07) << 18) | ((unsigned)(u[1] & 0x3F) << 12) |
              ((unsigned)(u[2] & 0x3F) << 6) | (unsigned)(u[3] & 0x3F);
        return 4;
    }
    *cp = u[0];
    return 1;
}

static size_t u8_count(const char *s) {
    size_t n = 0;
    unsigned cp;
    while (s && *s) { s += u8_next(s, &cp); n++; }
    return n;
}

/* First `nchars` code points of `s`, as a new string. */
static char *u8_prefix(const char *s, size_t nchars) {
    size_t bytes = 0, n = 0;
    unsigned cp;
    while (s[bytes] && n < nchars) { bytes += u8_next(s + bytes, &cp); n++; }
    return dup_range(s, bytes);
}

static size_t u8_put(unsigned cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* unicodedata.category(ch) in {"Cc", "Cf"} */
static int is_cc_cf(unsigned cp) {
    if (cp <= 0x1F) return 1;
    if (cp >= 0x7F && cp <= 0x9F) return 1;
    if (cp == 0xAD) return 1;
    if (cp >= 0x600 && cp <= 0x605) return 1;
    if (cp == 0x61C || cp == 0x6DD || cp == 0x70F || cp == 0x180E) return 1;
    if (cp >= 0x200B && cp <= 0x200F) return 1;
    if (cp >= 0x202A && cp <= 0x202E) return 1;
    if (cp >= 0x2060 && cp <= 0x2064) return 1;
    if (cp >= 0x206A && cp <= 0x206F) return 1;
    if (cp == 0xFEFF) return 1;
    if (cp >= 0xFFF9 && cp <= 0xFFFB) return 1;
    if (cp >= 0x1D173 && cp <= 0x1D17A) return 1;
    if (cp == 0xE0001) return 1;
    if (cp >= 0xE0020 && cp <= 0xE007F) return 1;
    return 0;
}

/* str.isspace() over the code points a filename can realistically carry. */
static int is_py_space(unsigned cp) {
    if (cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C || cp == 0x0D || cp == 0x20) return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return 1;
    if (cp >= 0x2000 && cp <= 0x200A) return 1;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000) return 1;
    return 0;
}

char *w19_media_dc1e86_clean_attachment_filename(const char *name) {
    const char *base_start, *scan;
    char *filtered, *trimmed;
    size_t out_len = 0, nchars;
    unsigned cp;

    if (!name) return dupstr("");
    /* name.replace("\\", "/").rsplit("/", 1)[-1] */
    base_start = name;
    for (scan = name; *scan; scan++)
        if (*scan == '/' || *scan == '\\') base_start = scan + 1;
    filtered = (char *)malloc(strlen(base_start) + 1);
    if (!filtered) return NULL;
    for (scan = base_start; *scan; ) {
        size_t step = u8_next(scan, &cp);
        if (!is_cc_cf(cp)) { memcpy(filtered + out_len, scan, step); out_len += step; }
        scan += step;
    }
    filtered[out_len] = '\0';

    /* .strip() */
    {
        size_t start = 0, end = out_len;
        while (start < end) {
            size_t step = u8_next(filtered + start, &cp);
            if (!is_py_space(cp)) break;
            start += step;
        }
        while (end > start) {
            size_t i = end - 1;
            while (i > start && (((unsigned char)filtered[i]) & 0xC0) == 0x80 && i > start) i--;
            u8_next(filtered + i, &cp);
            if (!is_py_space(cp)) break;
            end = i;
        }
        trimmed = dup_range(filtered + start, end - start);
        free(filtered);
        if (!trimmed) return NULL;
    }

    if (!trimmed[0] || !strcmp(trimmed, ".") || !strcmp(trimmed, "..")) {
        free(trimmed);
        return dupstr("");
    }

    nchars = u8_count(trimmed);
    if (nchars > MAX_FILENAME_CHARS) {
        /* stem, dot, ext = base.rpartition(".") */
        const char *last_dot = strrchr(trimmed, '.');
        char *keep = NULL;
        if (last_dot && last_dot != trimmed) {
            const char *ext = last_dot + 1;
            if (u8_count(ext) <= 16) {
                size_t elen = strlen(ext);
                keep = (char *)malloc(elen + 2);
                if (!keep) { free(trimmed); return NULL; }
                keep[0] = '.';
                memcpy(keep + 1, ext, elen + 1);
            }
        }
        {
            size_t keep_chars = keep ? u8_count(keep) : 0;
            char *head = u8_prefix(trimmed, MAX_FILENAME_CHARS - keep_chars);
            char *joined = NULL;
            if (head) {
                size_t hlen = strlen(head), klen = keep ? strlen(keep) : 0;
                joined = (char *)malloc(hlen + klen + 1);
                if (joined) {
                    memcpy(joined, head, hlen);
                    if (klen) memcpy(joined + hlen, keep, klen);
                    joined[hlen + klen] = '\0';
                }
                free(head);
            }
            free(keep);
            free(trimmed);
            return joined ? joined : dupstr("");
        }
    }
    return trimmed;
}

/* -- percent decoding (urllib.parse.unquote, byte oriented) ------------------- */

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char *pct_decode(const char *s) {
    size_t n = strlen(s), i, o = 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; ) {
        int hi = (s[i] == '%' && i + 2 < n) ? hexval((unsigned char)s[i + 1]) : -1;
        int lo = hi >= 0 ? hexval((unsigned char)s[i + 2]) : -1;
        if (hi >= 0 && lo >= 0) {
            out[o++] = (char)((hi << 4) | lo);
            i += 3;
        } else {
            out[o++] = s[i++];
        }
    }
    out[o] = '\0';
    return out;
}

/* -- URL splitting (urllib.parse.urlsplit, the fields this gate needs) -------- */

typedef struct {
    int ok;              /* 0 -> Python would raise ValueError (bad port / IPv6) */
    int netloc_len;
    char scheme[64];
    char netloc[1024];
    char path[8192];
    char host[512];
    int port;            /* -1 when absent */
} uparts_t;

static int parse_port(uparts_t *u, const char *pstr) {
    long v = 0;
    const char *q;
    if (!pstr || !*pstr) { u->port = -1; return 1; }
    for (q = pstr; *q; q++) {
        if (*q < '0' || *q > '9') return 0;
        v = v * 10 + (*q - '0');
        if (v > 1000000) return 0;
    }
    if (v > 65535) return 0;
    u->port = (int)v;
    return 1;
}

static void set_host(uparts_t *u, const char *h, size_t n) {
    size_t i;
    if (n >= sizeof u->host) n = sizeof u->host - 1;
    for (i = 0; i < n; i++) {
        char c = h[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        u->host[i] = c;
    }
    while (n > 0 && u->host[n - 1] == '.') n--;
    u->host[n] = '\0';
}

static int url_split(const char *url, uparts_t *u) {
    const char *sep, *p;
    size_t n;
    memset(u, 0, sizeof *u);
    u->ok = 1;
    u->port = -1;
    if (!url) url = "";
    sep = strstr(url, "://");
    p = url;
    if (sep) {
        n = (size_t)(sep - url);
        if (n >= sizeof u->scheme) n = sizeof u->scheme - 1;
        memcpy(u->scheme, url, n);
        u->scheme[n] = '\0';
        lower_in_place(u->scheme);
        p = sep + 3;
    } else if (url[0] == '/' && url[1] == '/') {
        p = url + 2;
    }
    if (p != url) {
        size_t nl = strcspn(p, "/?#");
        if (nl >= sizeof u->netloc) nl = sizeof u->netloc - 1;
        memcpy(u->netloc, p, nl);
        u->netloc[nl] = '\0';
        u->netloc_len = (int)nl;
        p += nl;
    }
    {
        size_t pl = strcspn(p, "?#");
        if (pl >= sizeof u->path) pl = sizeof u->path - 1;
        memcpy(u->path, p, pl);
        u->path[pl] = '\0';
    }
    {
        const char *auth = u->netloc;
        const char *at = strrchr(auth, '@');
        const char *hp = at ? at + 1 : auth;
        if (!*hp) { u->host[0] = '\0'; u->port = -1; return u->ok; }
        if (hp[0] == '[') {
            const char *close = strchr(hp, ']');
            if (!close) { u->ok = 0; return 0; }
            set_host(u, hp + 1, (size_t)(close - hp - 1));
            if (close[1] == ':') { if (!parse_port(u, close + 2)) { u->ok = 0; return 0; } }
            else if (close[1] != '\0') { u->ok = 0; return 0; }
        } else {
            const char *colon = strrchr(hp, ':');
            if (colon) {
                if (strchr(hp, ':') != colon) { u->ok = 0; return 0; }
                set_host(u, hp, (size_t)(colon - hp));
                if (!parse_port(u, colon + 1)) { u->ok = 0; return 0; }
            } else {
                set_host(u, hp, strlen(hp));
            }
        }
    }
    return u->ok;
}

/* url_origin: (scheme, host, effective port); 0 when Python raises ValueError. */
static int url_origin(const char *url, char *scheme, size_t scap, char *host, size_t hcap, int *port) {
    uparts_t u;
    int eff;
    if (!url_split(url, &u) || !u.ok) return 0;
    snprintf(scheme, scap, "%s", u.scheme);
    snprintf(host, hcap, "%s", u.host);
    eff = u.port;
    if (eff < 0) {
        if (!strcmp(u.scheme, "http")) eff = 80;
        else if (!strcmp(u.scheme, "https")) eff = 443;
        else eff = -1;
    }
    *port = eff;
    return 1;
}

static int origin_eq(const char *s1, const char *h1, int p1, const char *s2, const char *h2, int p2) {
    return p1 == p2 && !strcmp(s1, s2) && !strcmp(h1, h2);
}

char *w19_media_dc1e86_media_base_url(const char *relay_dial_url) {
    const char *s = relay_dial_url ? relay_dial_url : "";
    size_t n;
    char *raw, *out;
    while (*s && is_ascii_space((unsigned char)*s)) s++;
    n = strlen(s);
    while (n > 0 && is_ascii_space((unsigned char)s[n - 1])) n--;
    raw = dup_range(s, n);
    if (!raw) return NULL;
    while (n > 0 && raw[n - 1] == '/') { raw[--n] = '\0'; }
    if (!strncmp(raw, "ws://", 5)) {
        size_t tail = strlen(raw + 5);
        out = (char *)malloc(7 + tail + 1);
        if (!out) { free(raw); return NULL; }
        memcpy(out, "http://", 7);
        memcpy(out + 7, raw + 5, tail + 1);
        free(raw);
        raw = out;
    } else if (!strncmp(raw, "wss://", 6)) {
        size_t tail = strlen(raw + 6);
        out = (char *)malloc(8 + tail + 1);
        if (!out) { free(raw); return NULL; }
        memcpy(out, "https://", 8);
        memcpy(out + 8, raw + 6, tail + 1);
        free(raw);
        raw = out;
    }
    /* str.removesuffix("/relay") */
    n = strlen(raw);
    if (n >= 6 && !strcmp(raw + n - 6, "/relay")) raw[n - 6] = '\0';
    return raw;
}

int w19_media_dc1e86_enabled(const char *base_url, const char *gateway_id, const char *secret) {
    const char *b = base_url ? base_url : "";
    const char *g = gateway_id ? gateway_id : "";
    const char *s = secret ? secret : "";
    size_t n = strlen(b);
    while (n > 0 && b[n - 1] == '/') n--;
    return n > 0 && g[0] && s[0];
}

int w19_media_dc1e86_is_relay_media_url(const char *base_url, const char *url) {
    char *base;
    uparts_t c, b;
    char cs[64], ch[512], bs[64], bh[512];
    int cp, bp;
    char *decoded, *seg, *prefix;
    size_t i, ubase, prefix_len;
    int hit;

    base = dupstr(base_url ? base_url : "");
    if (!base) return 0;
    {
        size_t n = strlen(base);
        while (n > 0 && base[n - 1] == '/') base[--n] = '\0';
    }
    if (!url_origin(url ? url : "", cs, sizeof cs, ch, sizeof ch, &cp)) { free(base); return 0; }
    if (!url_origin(base, bs, sizeof bs, bh, sizeof bh, &bp)) { free(base); return 0; }
    if (!url_split(url ? url : "", &c) || !url_split(base, &b)) { free(base); return 0; }
    if (strchr(c.netloc, '@') || strchr(c.netloc, '\\')) { free(base); return 0; }
    if (strcmp(cs, "http") && strcmp(cs, "https")) { free(base); return 0; }
    if (!ch[0] || !origin_eq(cs, ch, cp, bs, bh, bp)) { free(base); return 0; }

    decoded = pct_decode(c.path);
    if (!decoded) { free(base); return 0; }
    for (i = 0; decoded[i]; i++)
        if (decoded[i] == '\\') decoded[i] = '/';
    seg = decoded;
    while (seg && *seg) {
        char *slash = strchr(seg, '/');
        size_t seg_len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') { free(decoded); free(base); return 0; }
        seg = slash ? slash + 1 : NULL;
    }
    free(decoded);

    /* f"{base.path.rstrip('/')}/relay/media/" */
    ubase = strlen(b.path);
    while (ubase > 0 && b.path[ubase - 1] == '/') ubase--;
    prefix = (char *)malloc(ubase + strlen("/relay/media/") + 1);
    if (!prefix) { free(base); return 0; }
    memcpy(prefix, b.path, ubase);
    memcpy(prefix + ubase, "/relay/media/", strlen("/relay/media/") + 1);
    prefix_len = strlen(prefix);
    hit = !strncmp(c.path, prefix, prefix_len);
    free(prefix);
    free(base);
    return hit;
}

int w19_media_dc1e86_warn_foreign_rehost(const char *base_url, const char *url, const json_t *seen) {
    char *base;
    uparts_t u;
    char cs[64], ch[512], bs[64], bh[512];
    int cp, bp, i;

    if (!url || !*url) return 0;
    if (!url_origin(url, cs, sizeof cs, ch, sizeof ch, &cp)) return 0;
    base = dupstr(base_url ? base_url : "");
    if (!base) return 0;
    {
        size_t n = strlen(base);
        while (n > 0 && base[n - 1] == '/') base[--n] = '\0';
    }
    if (!url_origin(base, bs, sizeof bs, bh, sizeof bh, &bp) || origin_eq(cs, ch, cp, bs, bh, bp)) {
        free(base);
        return 0;
    }
    free(base);
    if (!url_split(url, &u) || strstr(u.path, "/relay/media/") == NULL) return 0;
    if (seen && seen->type == JSON_ARRAY) {
        for (i = 0; i < (int)json_len(seen); i++) {
            json_t *row = json_get(seen, (size_t)i);
            json_t *e0, *e1, *e2;
            if (!row || row->type != JSON_ARRAY || json_len(row) != 3) continue;
            e0 = json_get(row, 0);
            e1 = json_get(row, 1);
            e2 = json_get(row, 2);
            if (origin_eq(cs, ch, cp,
                          (e0 && e0->type == JSON_STRING) ? e0->str_val : "",
                          (e1 && e1->type == JSON_STRING) ? e1->str_val : "",
                          (e2 && e2->type == JSON_NUMBER) ? (int)e2->num_val : -1))
                return 0;
        }
    }
    return 1;
}

/* -- Content-Disposition (RFC 6266) ------------------------------------------ */

/* email.utils.unquote */
static char *unquote(const char *s) {
    size_t n = strlen(s);
    if (n > 1 && s[0] == '"' && s[n - 1] == '"') {
        char *mid = dup_range(s + 1, n - 2);
        char *out;
        size_t i, o = 0;
        if (!mid) return NULL;
        out = (char *)malloc(strlen(mid) + 1);
        if (!out) { free(mid); return NULL; }
        for (i = 0; mid[i]; i++) {
            if (mid[i] == '\\' && (mid[i + 1] == '\\' || mid[i + 1] == '"')) out[o++] = mid[++i];
            else out[o++] = mid[i];
        }
        out[o] = '\0';
        free(mid);
        return out;
    }
    if (n > 1 && s[0] == '<' && s[n - 1] == '>') return dup_range(s + 1, n - 2);
    return dupstr(s);
}

/* One parameter value as the email parser hands it over: a quoted string is
 * unescaped and de-quoted; an unterminated quote is kept verbatim.  The value is
 * bounded by the parameter's own end (never runs into later parameters). */
static char *parse_param_value(const char *raw, size_t n) {
    size_t i, o = 0;
    char *out;
    int closed = 0;
    while (n > 0 && is_ascii_space((unsigned char)*raw)) { raw++; n--; }
    while (n > 0 && is_ascii_space((unsigned char)raw[n - 1])) n--;
    if (n == 0) return dupstr("");
    if (raw[0] == '"') {
        for (i = 1; i < n; i++) {
            if (raw[i] == '\\' && i + 1 < n) { i++; continue; }
            if (raw[i] == '"') { closed = 1; n = i; break; }
        }
        if (!closed) return dup_range(raw, n);
        out = (char *)malloc(n + 1);
        if (!out) return NULL;
        for (i = 1; i < n; i++) {
            if (raw[i] == '\\' && i + 1 < n) { out[o++] = raw[++i]; continue; }
            out[o++] = raw[i];
        }
        out[o] = '\0';
        return out;
    }
    return dup_range(raw, n);
}

/* str(rawbytes, charset, errors="replace") for the charsets this gate can meet. */
static char *collapse_rfc2231(const char *charset, const char *text) {
    char *bytes, *out;
    size_t i, o = 0, n;
    if (!charset || !*charset) charset = "us-ascii";
    bytes = pct_decode(text ? text : "");
    if (!bytes) return NULL;
    n = strlen(bytes);
    if (!strcasecmp(charset, "utf-8") || !strcasecmp(charset, "utf8")) return bytes;
    if (!strcasecmp(charset, "us-ascii") || !strcasecmp(charset, "ascii")) {
        out = (char *)malloc(n * 3 + 1);
        if (!out) { free(bytes); return NULL; }
        for (i = 0; i < n; i++) {
            unsigned char c = (unsigned char)bytes[i];
            if (c < 0x80) out[o++] = (char)c;
            else { out[o++] = (char)0xEF; out[o++] = (char)0xBF; out[o++] = (char)0xBD; }
        }
        out[o] = '\0';
        free(bytes);
        return out;
    }
    if (!strcasecmp(charset, "iso-8859-1") || !strcasecmp(charset, "latin-1") ||
        !strcasecmp(charset, "latin1") || !strcasecmp(charset, "iso8859-1")) {
        out = (char *)malloc(n * 2 + 1);
        if (!out) { free(bytes); return NULL; }
        for (i = 0; i < n; i++) o += u8_put((unsigned char)bytes[i], out + o);
        out[o] = '\0';
        free(bytes);
        return out;
    }
    return bytes;   /* unsupported multi-byte charset: treat the bytes as UTF-8 */
}

char *w19_media_dc1e86_content_disposition_filename(const char *header) {
    const char *p;
    char *plain = NULL, *extended = NULL, *result, *cleaned;

    if (!header || !*header) return dupstr("");

    p = header;
    while (*p && *p != ';') {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
        }
        if (*p) p++;
    }
    while (*p == ';') {
        const char *start, *eq = NULL, *q;
        char *key, *rawval;
        int star = 0, in_quote = 0;

        p++;
        start = p;
        q = p;
        while (*q) {
            if (*q == '\\' && in_quote && q[1]) { q += 2; continue; }
            if (*q == '"') { in_quote = !in_quote; q++; continue; }
            if (*q == ';' && !in_quote) break;
            q++;
        }
        p = q;
        for (q = start; q < p; q++) if (*q == '=') { eq = q; break; }
        if (!eq) continue;

        key = dup_range(start, (size_t)(eq - start));
        if (!key) continue;
        {
            char *k = key;
            size_t kn;
            while (*k && is_ascii_space((unsigned char)*k)) k++;
            kn = strlen(k);
            while (kn > 0 && is_ascii_space((unsigned char)k[kn - 1])) kn--;
            k[kn] = '\0';
            lower_in_place(k);
            {
                size_t bl = strlen(k);
                if (bl > 0 && k[bl - 1] == '*') { k[bl - 1] = '\0'; star = 1; }
            }
            if (strcmp(k, "filename") != 0) { free(key); continue; }
        }
        rawval = parse_param_value(eq + 1, (size_t)(p - (eq + 1)));
        if (!rawval) { free(key); continue; }
        if (star) {
            char *q1 = strchr(rawval, '\'');
            char *q2 = q1 ? strchr(q1 + 1, '\'') : NULL;
            char *charset = NULL, *text = NULL, *conv = NULL;
            if (q1 && q2) {
                charset = dup_range(rawval, (size_t)(q1 - rawval));
                text = dup_range(q2 + 1, strlen(q2 + 1));
            } else {
                charset = dupstr("");
                text = dupstr(rawval);
            }
            if (charset && text) conv = collapse_rfc2231(charset, text);
            free(charset);
            free(text);
            if (conv && conv[0] && (!extended || !extended[0])) {
                free(extended);
                extended = conv;
            } else {
                free(conv);
            }
        } else {
            char *uq = unquote(rawval);
            if (uq && uq[0] && (!plain || !plain[0])) {
                free(plain);
                plain = uq;
            } else {
                free(uq);
            }
        }
        free(rawval);
        free(key);
    }

    result = (extended && extended[0]) ? dupstr(extended) : dupstr(plain ? plain : "");
    free(plain);
    free(extended);
    if (!result) return NULL;
    cleaned = w19_media_dc1e86_clean_attachment_filename(result);
    free(result);
    return cleaned;
}
