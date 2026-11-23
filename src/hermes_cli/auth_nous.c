/*
 * auth_nous.c — faithful C11 port of upstream Hermes hermes_cli/auth_nous.py.
 *
 * PoP: w7_authnous_65e5cf_scope_values @ hermes_cli/auth_nous.py:_scope_values
 * PoP: w7_authnous_65e5cf_model_priority @ hermes_cli/auth_nous.py:_model_priority
 * PoP: w7_authnous_65e5cf_validate_inference_url @ hermes_cli/auth_nous.py:_validate_nous_inference_url_from_network
 * PoP: w7_authnous_65e5cf_portal_host @ hermes_cli/auth_nous.py:_portal_host
 */
#include "hermes_json.h"
#include "noc_w7_authnous_65e5cf.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define AN_DEFAULT_PORTAL_URL "https://portal.nousresearch.com"
#define AN_DEFAULT_PORTAL_HOST "portal.nousresearch.com"

/* --- _scope_values ---------------------------------------------------------- */
struct an_scopes { char **items; size_t n, cap; };

static void an_scopes_add(struct an_scopes *s, const char *tok) {
    if (!tok || !*tok) return;
    for (size_t i = 0; i < s->n; i++)
        if (!strcmp(s->items[i], tok)) return;   /* set semantics */
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **p = (char **)realloc(s->items, cap * sizeof(char *));
        if (!p) return;
        s->items = p; s->cap = cap;
    }
    s->items[s->n++] = strdup(tok);
}

static void an_scopes_collect(const struct json_t *raw, struct an_scopes *out) {
    if (!raw) return;
    if (raw->type == JSON_STRING && raw->str_val) {
        char *copy = strdup(raw->str_val);
        if (!copy) return;
        for (char *p = copy; *p; p++)
            if (*p == ',') *p = ' ';
        char *save = NULL;
        for (char *tok = strtok_r(copy, " \t\r\n", &save); tok;
             tok = strtok_r(NULL, " \t\r\n", &save))
            an_scopes_add(out, tok);
        free(copy);
    } else if (raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw); i++) {
            struct json_t *item = json_get((struct json_t *)raw, i);
            if (item && item->type == JSON_STRING)
                an_scopes_collect(item, out);
        }
    }
}

static int an_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

struct json_t *w7_authnous_65e5cf_scope_values(const struct json_t *raw_scope) {
    struct an_scopes set = {0};
    an_scopes_collect(raw_scope, &set);
    if (set.n > 1) qsort(set.items, set.n, sizeof(char *), an_cmp_str);
    struct json_t *arr = json_array();
    for (size_t i = 0; i < set.n; i++) {
        json_append(arr, json_string(set.items[i]));
        free(set.items[i]);
    }
    free(set.items);
    return arr;
}

/* --- _model_priority -------------------------------------------------------- */
static int an_contains_ci(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (!n) return 1;
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < n && tolower((unsigned char)p[i]) == (char)needle[i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

struct json_t *w7_authnous_65e5cf_model_priority(const char *mid) {
    const char *m = mid ? mid : "";
    int rank = an_contains_ci(m, "opus") ? 0
             : (an_contains_ci(m, "pro") && !an_contains_ci(m, "sonnet")) ? 1
             : an_contains_ci(m, "sonnet") ? 3 : 2;
    struct json_t *arr = json_array();
    json_append(arr, json_int(rank));
    json_append(arr, json_string(m));
    return arr;
}

/* --- url parsing helpers ---------------------------------------------------- */
/* urlparse(url).hostname (lowercased); empty string when none. */
static char *an_hostname(const char *url) {
    const char *p = url ? url : "";
    const char *sep = strstr(p, "://");
    /* urlparse() only yields a hostname for a scheme://authority (or //authority). */
    if (!sep && strncmp(p, "//", 2) != 0) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    const char *rest = sep ? sep + 3 : p + 2;
    const char *end = rest + strlen(rest);
    for (const char *q = rest; q < end && *q; q++)
        if (*q == '/' || *q == '?' || *q == '#') { end = q; break; }
    const char *at = memchr(rest, '@', (size_t)(end - rest));
    if (at) rest = at + 1;
    const char *colon = memchr(rest, ':', (size_t)(end - rest));
    if (colon) end = colon;
    size_t n = (size_t)(end - rest);
    char *host = (char *)malloc(n + 1);
    if (!host) return NULL;
    for (size_t i = 0; i < n; i++) host[i] = (char)tolower((unsigned char)rest[i]);
    host[n] = '\0';
    return host;
}

static char *an_scheme(const char *url) {
    const char *p = url ? url : "";
    const char *sep = strstr(p, "://");
    if (!sep) return strdup("");
    size_t n = (size_t)(sep - p);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    for (size_t i = 0; i < n; i++) s[i] = (char)tolower((unsigned char)p[i]);
    s[n] = '\0';
    return s;
}

static char *an_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n); out[n] = '\0';
    return out;
}

static int an_host_allowed(const char *host, const char *override) {
    if (!host || !*host) return 0;
    if (!strcmp(host, "inference-api.nousresearch.com") ||
        !strcmp(host, "welcome-api.nousresearch.com"))
        return 1;
    if (!override || !*override) return 0;
    char *oh = an_hostname(override);
    if (!oh) return 0;
    int ok = !strcmp(oh, host);
    free(oh);
    return ok;
}

struct json_t *w7_authnous_65e5cf_validate_inference_url(const char *url, const char *override) {
    char *cleaned = an_strip(url);
    if (!cleaned || !*cleaned) { free(cleaned); return json_null(); }
    char *scheme = an_scheme(cleaned);
    char *host = an_hostname(cleaned);
    int ok = scheme && !strcmp(scheme, "https") && an_host_allowed(host, override);
    free(scheme); free(host);
    if (!ok) { free(cleaned); return json_null(); }
    size_t n = strlen(cleaned);
    while (n > 0 && cleaned[n - 1] == '/') cleaned[--n] = '\0';
    struct json_t *r = json_string(cleaned);
    free(cleaned);
    return r;
}

struct json_t *w7_authnous_65e5cf_portal_host(const char *portal_url) {
    const char *src = (portal_url && *portal_url) ? portal_url : AN_DEFAULT_PORTAL_URL;
    char *host = an_hostname(src);
    struct json_t *r = json_string((host && *host) ? host : AN_DEFAULT_PORTAL_HOST);
    free(host);
    return r;
}
