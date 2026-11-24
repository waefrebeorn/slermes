/*
 * web_server_remote_media.c — C home for upstream Hermes hermes_cli/web_server_remote_media.py.
 * STATUS: PORTED (gate w25_webserverr_68f039) — differential oracle green.
 * PoP: w25_webserverr_68f039_position @ hermes_cli/web_server_remote_media.py:_position
 * PoP: w25_webserverr_68f039_byte_range @ hermes_cli/web_server_remote_media.py:byte_range
 * PoP: w25_webserverr_68f039_response @ hermes_cli/web_server_remote_media.py:remote_media_response
 * PoP: w25_webserverr_68f039_constants @ hermes_cli/web_server_remote_media.py:<module constants>
 *
 * Starlette's FileResponse answers Range locally; a file on the SSH machine has
 * no local handle, so this route reads exactly the requested slice through the
 * workspace adapter.  The adapter (``stream_file``), the ASGI request and the
 * streaming body are seams; the ported part is the single-range grammar, the
 * position clamp that keeps a huge Range number away from ``int()``, and the
 * status/header envelope that slice is served under.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include "noc_w25_webserverr_68f039.h"

#define RM_MAX_DIGITS W25_WEBSERVERR_68F039_MAX_POSITION_DIGITS

static int rm_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int rm_isdigit(char c) { return c >= '0' && c <= '9'; }

/* _SINGLE_RANGE = ^\s*bytes\s*=(\d*)\s*-(\d*)\s*$ , re.IGNORECASE.
 * Returns 1 and fills the two digit runs (as offsets/lengths) on a match. */
static int rm_match_single_range(const char *header, const char **fd, size_t *fl,
                                 const char **ld, size_t *ll) {
    const char *p;
    const char *a, *b;
    size_t n;

    if (!header) header = "";
    p = header;
    while (rm_is_space(*p)) p++;
    if (strncasecmp(p, "bytes", 5) != 0) return 0;
    p += 5;
    while (rm_is_space(*p)) p++;
    if (*p != '=') return 0;
    p++;
    while (rm_is_space(*p)) p++;
    a = p;
    while (rm_isdigit(*p)) p++;
    b = p;
    while (rm_is_space(*p)) p++;
    if (*p != '-') return 0;
    p++;
    while (rm_is_space(*p)) p++;
    n = 0;
    while (rm_isdigit(p[n])) n++;
    {
        size_t k = n;
        while (rm_is_space(p[k])) k++;
        if (p[k] != '\0') return 0;
    }
    *fd = a; *fl = (size_t)(b - a);
    *ld = p; *ll = n;
    return 1;
}

/* int(digits) with the "too long to be in any file" clamp. */
static unsigned long long rm_position(const char *digits, size_t len) {
    const char *p = digits;
    size_t n = len, i;
    unsigned long long v = 0;

    while (n > 0 && *p == '0') { p++; n--; }
    if (n == 0) return 0;
    if (n > RM_MAX_DIGITS) return (unsigned long long)W25_WEBSERVERR_68F039_PAST_ANY_FILE;
    for (i = 0; i < n; i++) v = v * 10ULL + (unsigned long long)(p[i] - '0');
    return v;
}

json_t *w25_webserverr_68f039_position(const char *digits) {
    const char *d = digits ? digits : "";
    size_t n = strlen(d), i;
    for (i = 0; i < n; i++) if (!rm_isdigit(d[i])) return json_null();
    return json_number((double)rm_position(d, n));
}

/* RFC 9110 single-range reading.  1 = span, 0 = not a header we read, -1 = 416. */
static int rm_span(const char *header, long long size, long long *first, long long *last) {
    const char *fd = NULL, *ld = NULL;
    size_t fl = 0, ll = 0;
    unsigned long long f, l;

    if (!rm_match_single_range(header, &fd, &fl, &ld, &ll)) return 0;
    if (fl == 0 && ll == 0) return 0; /* "bytes=-" */
    if (fl == 0) { /* a suffix: the last N bytes */
        unsigned long long suffix = rm_position(ld, ll);
        if (suffix == 0 || size == 0) return -1; /* 416 */
        *first = (long long)suffix >= size ? 0 : size - (long long)suffix;
        *last = size - 1;
        return 1;
    }
    f = rm_position(fd, fl);
    if (ll == 0) { /* open ended: to the end of the file */
        if ((long long)f >= size) return -1;
        *first = (long long)f;
        *last = size - 1;
        return 1;
    }
    l = rm_position(ld, ll);
    if (l < f) return 0; /* a last byte before the first: ignored, as RFC 9110 allows */
    if ((long long)f >= size) return -1;
    *first = (long long)f;
    *last = l >= (unsigned long long)size ? size - 1 : (long long)l;
    return 1;
}

json_t *w25_webserverr_68f039_byte_range(const json_t *header, long long size) {
    const char *h = NULL;
    long long first = 0, last = 0;
    int rc;
    json_t *out;

    if (header && header->type == JSON_STRING) h = header->str_val;
    else if (header && header->type != JSON_NULL) return json_null();
    rc = rm_span(h, size, &first, &last);
    if (rc == 0) return json_null();
    if (rc < 0) {
        out = json_object();
        json_set(out, "unsatisfiable", json_number((double)size));
        return out;
    }
    out = json_object();
    json_set(out, "first", json_number((double)first));
    json_set(out, "last", json_number((double)last));
    return out;
}

/* urllib.parse.quote over the UTF-8 bytes of a basename (safe=""). */
static char *rm_quote(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(s), i;
    char *out = (char *)malloc(n * 3 + 1);
    char *q = out;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '.' || c == '-' || c == '~') {
            *q++ = (char)c;
        } else {
            *q++ = '%'; *q++ = hex[c >> 4]; *q++ = hex[c & 0x0f];
        }
    }
    *q = '\0';
    return out;
}

/* PurePosixPath(target).name: the last component, trailing "/" and "."
 * components dropped, and "" for a path that names none. */
static const char *rm_basename(const char *target, char *buf, size_t cap) {
    const char *s = target ? target : "";
    const char *p = s, *comp = NULL;
    size_t n = 0, len = 0;

    while (*p) {
        const char *start;
        while (*p == '/') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != '/') p++;
        n = (size_t)(p - start);
        if (n == 1 && start[0] == '.') continue;
        comp = start;
        len = n;
    }
    if (!comp || len + 1 > cap) { if (cap) buf[0] = '\0'; return buf; }
    memcpy(buf, comp, len);
    buf[len] = '\0';
    return buf;
}

static char *rm_disposition(const char *target) {
    char base[1024];
    char *quoted, *out;
    size_t need;

    rm_basename(target, base, sizeof(base));
    quoted = rm_quote(base);
    if (!quoted) return NULL;
    need = strlen("inline; filename*=UTF-8''") + strlen(quoted) + 1;
    out = (char *)malloc(need);
    if (out) snprintf(out, need, "inline; filename*=UTF-8''%s", quoted);
    free(quoted);
    return out;
}

static void rm_set_ll(json_t *o, const char *key, long long v) {
    json_set(o, key, json_number((double)v));
}

json_t *w25_webserverr_68f039_response(const json_t *request, const char *target,
                                       long long size, const char *media_type) {
    const char *method = "GET";
    const char *range = NULL;
    long long first = 0, last = size - 1, length;
    int ranged = 0, rc;
    json_t *out, *headers, *read = NULL;
    char buf[128], *disp;

    if (request && request->type == JSON_OBJECT) {
        const char *m = json_get_str(request, "method", NULL);
        const json_t *hdrs;
        if (m) method = m;
        hdrs = json_obj_get(request, "headers");
        if (hdrs && hdrs->type == JSON_OBJECT) {
            const json_t *r = NULL;
            size_t i;
            for (i = 0; i < json_object_size(hdrs); i++) { /* name is case-insensitive */
                const char *k = json_object_get_key_at(hdrs, i);
                if (k && !strcasecmp(k, "range")) { r = json_object_get_at(hdrs, i); break; }
            }
            if (r && r->type == JSON_STRING) range = r->str_val;
        }
    }

    rc = rm_span(range, size, &first, &last);
    if (rc < 0) {
        out = json_object();
        json_set(out, "unsatisfiable", json_number((double)size));
        return out;
    }
    if (rc == 0) { first = 0; last = size - 1; } else { ranged = 1; }

    length = last - first + 1;
    if (length < 0) length = 0;

    out = json_object();
    headers = json_object();
    json_set(headers, "accept-ranges", json_string("bytes"));
    disp = rm_disposition(target);
    json_set(headers, "content-disposition", json_string(disp ? disp : "inline"));
    free(disp);
    snprintf(buf, sizeof(buf), "%lld", length);
    json_set(headers, "content-length", json_string(buf));
    json_set(headers, "x-content-type-options", json_string("nosniff"));
    if (ranged) {
        snprintf(buf, sizeof(buf), "bytes %lld-%lld/%lld", first, last, size);
        json_set(headers, "content-range", json_string(buf));
    }
    json_set(headers, "content-type", json_string(media_type ? media_type : ""));

    rm_set_ll(out, "status", ranged ? 206 : 200);
    json_set(out, "headers", headers);
    if (!strcmp(method, "HEAD") || length == 0) {
        read = json_null();
    } else if (ranged) {
        read = json_object();
        rm_set_ll(read, "offset", first);
        rm_set_ll(read, "length", length);
    } else {
        read = json_string("full");
    }
    json_set(out, "read", read);
    return out;
}

json_t *w25_webserverr_68f039_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_position_digits", json_number((double)RM_MAX_DIGITS));
    json_set(o, "past_any_file", json_number(W25_WEBSERVERR_68F039_PAST_ANY_FILE));
    return o;
}
