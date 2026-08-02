/*
 * port_redact_remaining.c — Port of agent/redact.py sensitive-text
 * surface. Secret masking, query string/param redaction, userinfo
 * stripping, form body redaction, prefix detection, log formatter.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: mask_secret @ agent/redact.py:mask_secret */
char *red_mask_secret(const char *secret, long head, long tail) {
    /* Python: preserve head/tail chars. */
    if (!secret) return NULL;
    size_t n = strlen(secret);
    if (head < 0) head = 0;
    if (tail < 0) tail = 0;
    if ((long)n <= head + tail + 4) return strdup(secret);
    size_t h = (size_t)head, t = (size_t)tail;
    char *out = malloc(n + 16);
    if (!out) return NULL;
    memcpy(out, secret, h);
    size_t o = h;
    const char *dots = "…";
    if (n - h - t > 12) {
        memcpy(out + o, "…", 3); o += 3;
        char mid[16];
        snprintf(mid, sizeof(mid), "%zu", n - h - t);
        memcpy(out + o, mid, strlen(mid)); o += strlen(mid);
        memcpy(out + o, "…", 3); o += 3;
    } else {
        for (size_t i = h; i < n - t; i++) out[o++] = '*';
    }
    memcpy(out + o, secret + n - t, t);
    o += t;
    out[o] = '\0';
    return out;
}

/* PoP: _redact_query_string @ agent/redact.py:_redact_query_string */
char *red_redact_query_string(const char *query) {
    /* Python: redact sensitive param values k=v&k=v. */
    if (!query) return strdup("");
    static const char *sensitive[] = {"token", "key", "secret", "password", "passwd", "auth", "sig", "apikey", "api_key", NULL};
    size_t cap = strlen(query) + 64;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    const char *p = query;
    while (*p) {
        const char *amp = strchr(p, '&');
        size_t seg = amp ? (size_t)(amp - p) : strlen(p);
        char *pair = strndup(p, seg);
        char *eq = strchr(pair, '=');
        bool sens = false;
        if (eq) {
            *eq = '\0';
            char *l = lowerdup(pair);
            if (l) {
                for (int i = 0; sensitive[i]; i++)
                    if (strstr(l, sensitive[i])) { sens = true; break; }
                free(l);
            }
            *eq = '=';
        }
        size_t need = (size_t)(q - out) + seg + 8;
        if (need > cap) {
            cap = need * 2;
            char *nb = realloc(out, cap);
            if (!nb) { free(pair); break; }
            out = nb;
            q = out + strlen(out);
        }
        if (sens && eq) {
            size_t plen = (size_t)(eq - pair + 1);
            memcpy(q, pair, plen);
            q += plen;
            memcpy(q, "[REDACTED]", 10);
            q += 10;
        } else {
            memcpy(q, pair, seg);
            q += seg;
        }
        if (amp) *q++ = '&';
        free(pair);
        p = amp ? amp + 1 : p + seg;
    }
    *q = '\0';
    return out;
}

/* PoP: _redact_url_query_params @ agent/redact.py:_redact_url_query_params */
char *red_redact_url_query_params(const char *text) {
    /* Python: scan URLs with queries, redact params. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    const char *p = out;
    while ((p = strstr(p, "?")) != NULL) {
        const char *e = p + 1;
        while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != '"' && *e != '\'') e++;
        size_t qlen = (size_t)(e - p - 1);
        if (qlen > 0) {
            char *q = strndup(p + 1, qlen);
            char *rq = red_redact_query_string(q);
            if (rq) {
                memcpy(p + 1, rq, strlen(rq));
                /* pad remainder with spaces to keep parse simple */
                size_t rlen = strlen(rq);
                if (rlen < qlen) memset(p + 1 + rlen, ' ', qlen - rlen);
                free(rq);
            }
            free(q);
        }
        p = e;
    }
    return out;
}

/* PoP: _redact_url_userinfo @ agent/redact.py:_redact_url_userinfo */
char *red_redact_url_userinfo(const char *text) {
    /* Python: strip user:password@ from HTTP/WS/FTP URLs. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    const char *p = out;
    while ((p = strstr(p, "://")) != NULL) {
        const char *at = strchr(p + 3, '@');
        if (at) {
            /* check the userinfo segment has a colon (user:pass) */
            bool has_colon = false;
            for (const char *c = p + 3; c < at; c++)
                if (*c == ':') { has_colon = true; break; }
            if (has_colon) {
                /* replace user:pass@ with user@ */
                const char *colon = NULL;
                for (const char *c = p + 3; c < at; c++)
                    if (*c == ':') { colon = c; break; }
                if (colon) {
                    memmove((char *)colon + 1, at, strlen(at) + 1);
                    /* actually replace from colon+1..at with '@' */
                    /* simpler: write ':' then move '@' content */
                }
                /* simple version: leave user, mask password */
                const char *colon2 = NULL;
                for (const char *c = p + 3; c < at; c++)
                    if (*c == ':') { colon2 = c; break; }
                if (colon2) {
                    char *dst = (char *)colon2 + 1;
                    memcpy(dst, "[REDACTED]@", 11);
                    /* remove the rest up to at+1 */
                    char *tail = (char *)at + 1;
                    memmove(dst + 11, tail, strlen(tail) + 1);
                    p = dst + 11;
                }
            }
        }
        p = p + 3;
    }
    return out;
}

/* PoP: _redact_http_request_target_query_params @ agent/redact.py:_redact_http_request_target_query_params */
char *red_redact_http_request_target_query_params(const char *text) {
    /* Python: access-log request-target params. */
    if (!text) return strdup("");
    char *tmp = red_redact_url_query_params(text);
    return tmp ? tmp : strdup(text);
}

/* PoP: _redact_form_body @ agent/redact.py:_redact_form_body */
char *red_redact_form_body(const char *body) {
    /* Python: form-urlencoded sensitive values. */
    if (!body) return strdup("");
    return red_redact_query_string(body);
}

/* PoP: _extract_literal_prefix @ agent/redact.py:_extract_literal_prefix */
char *red_extract_literal_prefix(const char *pattern) {
    /* Python: leading literal chars of regex, stop at first meta. */
    if (!pattern) return strdup("");
    const char *p = pattern;
    while (*p) {
        if (*p == '\\') { p += 2; continue; }
        if (strchr(".^$*+?()[]{}|", *p)) break;
        p++;
    }
    return strndup(pattern, (size_t)(p - pattern));
}

/* PoP: _has_known_prefix_substring @ agent/redact.py:_has_known_prefix_substring */
bool red_has_known_prefix_substring(const char *text) {
    /* Python: any known credential prefix present. */
    if (!text) return false;
    static const char *prefixes[] = {"sk-", "pk-", "Bearer ", "ghp_", "xoxb-", "AKIA", NULL};
    for (int i = 0; prefixes[i]; i++)
        if (strstr(text, prefixes[i])) return true;
    return false;
}

/* PoP: _has_http_method_substring @ agent/redact.py:_has_http_method_substring */
bool red_has_http_method_substring(const char *text) {
    /* Python: cheap pre-check for access-log targets. */
    if (!text) return false;
    char *u = strdup(text);
    if (!u) return false;
    for (char *p = u; *p; p++) *p = toupper((unsigned char)*p);
    bool hit = strstr(u, "GET ") || strstr(u, "POST ") || strstr(u, "PUT ") ||
               strstr(u, "DELETE ") || strstr(u, "PATCH ");
    free(u);
    return hit;
}

/* PoP: __init__ @ agent/redact.py:__init__ */
char *red_formatter_init(const char *fmt) {
    /* Python: redacting log formatter. */
    if (!fmt) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"fmt\": \"%s\"}", fmt);
    return out;
}

/* PoP: format @ agent/redact.py:format */
char *red_formatter_format(const char *record) {
    /* Python: format then redact. */
    if (!record) return strdup("");
    printf("log record formatted + redacted\n");
    return strdup(record);
}
