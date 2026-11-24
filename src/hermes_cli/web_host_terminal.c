/*
 * web_host_terminal.c — C port of upstream Hermes hermes_cli/web_host_terminal.py.
 *
 * PoP: w16_webhostte_70d7f1_terminal_lc_ctype @ hermes_cli/web_host_terminal.py:terminal_lc_ctype
 * PoP: w16_webhostte_70d7f1_query_dimension @ hermes_cli/web_host_terminal.py:query_dimension
 * PoP: w16_webhostte_70d7f1_request_allowed @ hermes_cli/web_host_terminal.py:request_allowed
 *
 * request_allowed's three imported globals (app.state, _LOOPBACK_HOSTS, policy)
 * are injected as explicit arguments rather than imported from a live server.
 */
#include "noc_w16_webhostte_70d7f1.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* META_PREFIX @ hermes_cli/web_host_terminal.py:META_PREFIX */
static const char *const hermes_cli_web_host_terminal_CONST_META_PREFIX =
    "\0HERMES_TERMINAL_META:";

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *lower_strip_dup(const char *s) {
    if (!s) return dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (!p) return dupstr("");
    for (size_t i = 0; i < n; i++) p[i] = (char)tolower((unsigned char)a[i]);
    p[n] = '\0';
    return p;
}

/* Faithful subset of Python int(raw_or_default): decimal strings with optional
 * sign/whitespace/underscores, numbers truncated toward zero, bool -> 0/1.
 * Returns 0 when Python would raise (TypeError/ValueError/OverflowError) or
 * when the value is falsy and the caller must use its default. */
static int parse_py_int(const json_t *raw, long long *out) {
    if (!raw || raw->type == JSON_NULL) return 0;
    if (raw->type == JSON_BOOL) { *out = raw->bool_val ? 1 : 0; return 1; }
    if (raw->type == JSON_NUMBER) {
        double d = raw->num_val;
        if (d >= 9.22e18 || d <= -9.22e18) return 0;
        *out = (long long)d;
        return 1;
    }
    if (raw->type != JSON_STRING) return 0;
    const char *s = raw->str_val ? raw->str_val : "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    if (e == s) return 0;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (s == e) return 0;
    char buf[160];
    size_t k = 0;
    int prev_digit = 0;
    for (const char *p = s; p < e; p++) {
        if (*p == '_') {
            if (!prev_digit || p + 1 >= e || !isdigit((unsigned char)p[1])) return 0;
            prev_digit = 0;
            continue;
        }
        if (!isdigit((unsigned char)*p)) return 0;
        if (k + 1 >= sizeof(buf)) return 0;
        buf[k++] = *p;
        prev_digit = 1;
    }
    if (k == 0 || !prev_digit) return 0;
    buf[k] = '\0';
    errno = 0;
    long long v = strtoll(buf, NULL, 10);
    if (errno == ERANGE) return 0;
    *out = neg ? -v : v;
    return 1;
}

struct json_t *w16_webhostte_70d7f1_terminal_lc_ctype(const json_t *env,
                                                      const char *platform) {
    int win32 = platform && !strcmp(platform, "win32");
    if (json_truthy(json_obj_get(env, "LC_ALL")) ||
        json_truthy(json_obj_get(env, "LC_CTYPE")) || win32)
        return json_null();
    if (platform && !strcmp(platform, "darwin")) return json_string("UTF-8");
    if (json_truthy(json_obj_get(env, "LANG"))) return json_null();
    return json_string("C.UTF-8");
}

double w16_webhostte_70d7f1_query_dimension(const json_t *raw,
                                            double def, double maximum) {
    long long v;
    if (!parse_py_int(raw, &v)) return def;
    double value = (double)v;
    if (value > maximum) value = maximum;
    if (value < 2.0) value = 2.0;
    return value;
}

bool w16_webhostte_70d7f1_request_allowed(bool host_terminal, bool auth_required,
                                          const char *bound_host,
                                          const json_t *loopback_hosts) {
    if (!host_terminal) return false;
    if (auth_required) return true;
    char *host = lower_strip_dup(bound_host);
    int found = 0;
    if (json_is_array(loopback_hosts)) {
        for (size_t i = 0; i < json_len(loopback_hosts); i++) {
            const json_t *h = json_get(loopback_hosts, i);
            if (h && h->type == JSON_STRING && h->str_val && !strcmp(host, h->str_val)) {
                found = 1;
                break;
            }
        }
    }
    free(host);
    return found ? true : false;
}
