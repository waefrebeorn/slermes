/*
 * github_api.c — C home for upstream Hermes hermes_cli/github_api.py.
 *
 * PoP: w8_githubapi_45460f_token_from_env @ hermes_cli/github_api.py:github_token_from_env
 * PoP: w8_githubapi_45460f_header_int @ hermes_cli/github_api.py:_header_int
 * PoP: w8_githubapi_45460f_describe_failure @ hermes_cli/github_api.py:describe_github_failure
 *
 * The exception object is represented by an injected `spec` fixture object;
 * time-dependent rate-limit copy uses the explicit `now` seam. All exports are
 * w8_githubapi_45460f_ prefixed.
 */
#include "noc_w8_githubapi_45460f.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ga_strdup(const char *s) {
    size_t n = strlen(s ? s : "");
    char *o = (char *)malloc(n + 1);
    if (o) { if (n) memcpy(o, s ? s : "", n); o[n] = '\0'; }
    return o;
}
/* strdup of exactly n bytes */
static char *ga_strdup_n_local(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (o) { if (n) memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* Python int(str) base 10 for header/env values. Returns 1 on success. */
static int ga_py_int(const char *v, long long *out) {
    if (!v) return 0;
    while (*v && isspace((unsigned char)*v)) v++;
    const char *e = v + strlen(v);
    while (e > v && isspace((unsigned char)e[-1])) e--;
    if (e == v) return 0;
    char buf[128];
    size_t n = 0;
    int sign = 1;
    if (*v == '+' || *v == '-') { if (*v == '-') sign = -1; v++; }
    if (v == e) return 0;
    for (const char *p = v; p < e; ++p) {
        if (*p < '0' || *p > '9' || n + 1 >= sizeof(buf)) return 0;
        buf[n++] = *p;
    }
    buf[n] = '\0';
    *out = sign < 0 ? -strtoll(buf, NULL, 10) : strtoll(buf, NULL, 10);
    return 1;
}

char *w8_githubapi_45460f_token_from_env(const json_t *env) {
    static const char *const names[] = { "GITHUB_TOKEN", "GH_TOKEN" };
    for (size_t i = 0; i < 2; ++i) {
        const char *v = json_get_str(env, names[i], "");
        const char *p = v ? v : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        if (e > p) return ga_strdup_n_local(p, (size_t)(e - p));
    }
    return NULL;
}

/* local helper defined above */
json_t *w8_githubapi_45460f_header_int(const struct json_t *headers, const char *name) {
    json_t *v = json_obj_get(headers, name);
    if (!v) return json_null();   /* headers.get(name,"") -> "" -> int("") fails -> None */
    long long out = 0;
    if (v->type == JSON_STRING) {
        if (!ga_py_int(v->str_val, &out)) return json_null();
    } else if (v->type == JSON_NUMBER) {
        if (v->num_val != (double)(long long)v->num_val) return json_null();
        out = (long long)v->num_val;
    } else {
        return json_null();
    }
    return json_number((double)out);
}

static char *ga_fmt(const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return ga_strdup(buf);
}

char *w8_githubapi_45460f_describe_failure(const struct json_t *spec) {
    const char *kind = json_get_str(spec, "kind", "other");
    int authenticated = json_get_bool(spec, "authenticated", false) ? 1 : 0;

    if (!strcmp(kind, "http")) {
        int status = (int)json_get_num(spec, "status", 0);
        json_t *headers = json_obj_get(spec, "headers");
        long long remaining = 0;
        json_t *rem = w8_githubapi_45460f_header_int(headers, "x-ratelimit-remaining");
        int has_remaining = rem && rem->type == JSON_NUMBER;
        if (has_remaining) remaining = (long long)rem->num_val;
        json_free(rem);
        if ((status == 403 || status == 429) && has_remaining && remaining == 0) {
            long long reset = 0;
            int has_reset = 0;
            json_t *rs = w8_githubapi_45460f_header_int(headers, "x-ratelimit-reset");
            if (rs && rs->type == JSON_NUMBER) { reset = (long long)rs->num_val; has_reset = 1; }
            json_free(rs);
            const char *when = "within an hour";
            char whenbuf[80];
            if (has_reset) {
                long long now = (long long)json_get_num(spec, "now", 0);
                long long minutes = (reset - now + 59) / 60;   /* positive -> Python // */
                if (minutes >= 1) {
                    if (minutes == 1) when = "in about a minute";
                    else { snprintf(whenbuf, sizeof(whenbuf), "in about %lld minutes", minutes); when = whenbuf; }
                }
            }
            if (authenticated)
                return ga_fmt("GitHub API rate limit reached for your GITHUB_TOKEN (HTTP %d) "
                              "\xE2\x80\x94 it resets %s.", status, when);
            return ga_fmt("GitHub API rate limit reached (HTTP %d): anonymous requests are limited "
                          "to 60 per hour per network address, shared with everyone behind the same "
                          "connection. It resets %s; setting GITHUB_TOKEN in the environment lifts the limit.",
                          status, when);
        }
        if (status >= 500)
            return ga_fmt("GitHub is having trouble (HTTP %d from api.github.com) "
                          "\xE2\x80\x94 check githubstatus.com and try again later.", status);
        return ga_fmt("api.github.com answered HTTP %d.", status);
    }
    if (!strcmp(kind, "url") || !strcmp(kind, "url_timeout")) {
        if (!strcmp(kind, "url_timeout"))
            return ga_strdup("api.github.com did not answer within 10 seconds.");
        const char *reason = json_get_str(spec, "reason", "");
        /* "timed out" in str(reason).lower() */
        char low[256];
        size_t n = strlen(reason ? reason : "");
        if (n >= sizeof(low)) n = sizeof(low) - 1;
        for (size_t i = 0; i < n; ++i) low[i] = (char)tolower((unsigned char)reason[i]);
        low[n] = '\0';
        if (strstr(low, "timed out") != NULL)
            return ga_strdup("api.github.com did not answer within 10 seconds.");
        return ga_fmt("Connection to api.github.com failed (%s) "
                      "\xE2\x80\x94 check your connection, firewall or proxy.", reason ? reason : "");
    }
    if (!strcmp(kind, "timeout"))
        return ga_strdup("api.github.com did not answer within 10 seconds.");
    return ga_fmt("api.github.com: %s", json_get_str(spec, "message", ""));
}
