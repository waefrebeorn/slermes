/*
 * auxiliary_managed_local.c - C home for upstream Hermes agent/auxiliary_managed_local.py.
 * PoP: w16_auxiliarym_09359b_netloc @ agent/auxiliary_managed_local.py:urlparse(base).netloc.lower()
 * PoP: w16_auxiliarym_09359b_managed @ agent/auxiliary_managed_local.py:_is_managed_local_endpoint
 *
 * Faithful ports of the endpoint comparison: urlparse(base).netloc.lower() and
 * the early-exit boolean. The state-file/TTL plumbing of _managed_local_netloc
 * is injected as the 'managed' netloc argument (the oracle patches the live
 * helper to return it), exactly as the env/file seam guidance requires.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_auxiliarym_09359b.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default:          return 1;
    }
}
static void lower_ascii(char *s) {
    for (; s && *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}
/* urlparse(url).netloc.lower(); *err=1 mirrors the ValueError path (bad IPv6 brackets). */
static char *parse_netloc(const char *url, int *err) {
    *err = 0;
    if (!url) return dupstr("");
    const char *s = url;
    /* optional scheme: letter then letters/digits/+-. up to ':' */
    if (isalpha((unsigned char)*s)) {
        const char *q = s;
        while (*q && (isalnum((unsigned char)*q) || *q == '+' || *q == '-' || *q == '.')) q++;
        if (*q == ':') s = q + 1;
    }
    if (!(s[0] == '/' && s[1] == '/')) return dupstr("");
    const char *start = s + 2, *end = start;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t n = (size_t)(end - start);
    int ob = 0, cb = 0;
    for (size_t i = 0; i < n; i++) {
        if (start[i] == '[') ob++;
        else if (start[i] == ']') cb++;
    }
    if (ob != cb) { *err = 1; return dupstr(""); }
    char *out = (char *)malloc(n + 1);
    if (!out) return dupstr("");
    if (n) memcpy(out, start, n);
    out[n] = '\0';
    lower_ascii(out);
    return out;
}
static const char *url_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

struct json_t *w16_auxiliarym_09359b_netloc(const struct json_t *base_url) {
    int err = 0;
    char *nl = parse_netloc(url_str(base_url), &err);
    json_t *out = json_string(err ? "" : (nl ? nl : ""));
    free(nl);
    return out;
}

struct json_t *w16_auxiliarym_09359b_managed(const struct json_t *base_url,
        const struct json_t *managed) {
    if (is_falsy(base_url) || is_falsy(managed)) return json_bool(0);
    int err = 0;
    char *nl = parse_netloc(url_str(base_url), &err);
    const char *m = url_str(managed);
    int res = (!err && nl && m && strcmp(nl, m) == 0) ? 1 : 0;
    free(nl);
    return json_bool(res);
}
