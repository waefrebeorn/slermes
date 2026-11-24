/*
 * standalone_rest.c - C11 port of upstream
 * plugins/platforms/discord/standalone_rest.py (gate w24_standalone_e0a2a7).
 *
 * PoP: w24_standalone_e0a2a7_api_url @ plugins/platforms/discord/standalone_rest.py:api_url
 * PoP: w24_standalone_e0a2a7_seconds @ plugins/platforms/discord/standalone_rest.py:_seconds
 * PoP: w24_standalone_e0a2a7_pace @ plugins/platforms/discord/standalone_rest.py:post_paced
 * PoP: w24_standalone_e0a2a7_constants @ plugins/platforms/discord/standalone_rest.py:_MAX_RATE_LIMIT_RETRIES
 *
 * What is ported is the pacing decision the out-of-process sender makes for
 * itself when discord.py's rate-limit bucket is not there: how a route URL is
 * built, how a Retry-After/Reset-After value is clamped, and the bounded loop
 * that waits out a bucket and retries a 429. The HTTP session and the sleep
 * are seams: ``pace`` replays a scripted list of responses on a virtual
 * monotonic clock and reports exactly the waits post_paced would have taken.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_standalone_e0a2a7.h"

/* ---- Python float() over the shapes json carries ---------------------- */
static int sr_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char sr_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int sr_ci_prefix(const char *p, const char *lit) {
    while (*lit) {
        if (sr_lower(*p) != *lit) return 0;
        p++; lit++;
    }
    return 1;
}

/* A run of decimal digits under Python's single-underscore-between-digits
 * rule.  Returns NULL when there is no digit or an underscore is misplaced;
 * appends the digits (underscores dropped) to buf. */
static const char *sr_digit_run(const char *p, char *buf, size_t *n, size_t cap, size_t *count) {
    int prev_digit = 0;
    *count = 0;
    for (;;) {
        if (*p >= '0' && *p <= '9') {
            if (*n + 1 >= cap) return NULL;
            buf[(*n)++] = *p++;
            prev_digit = 1;
            (*count)++;
        } else if (*p == '_') {
            if (!prev_digit || !(p[1] >= '0' && p[1] <= '9')) return NULL;
            p++;
            prev_digit = 0;
        } else {
            break;
        }
    }
    return *count ? p : NULL;
}

/* Python float(str): whitespace, sign, digits/underscores, an optional
 * fraction and exponent, and the inf/infinity/nan spellings.  Returns 0 where
 * Python raises ValueError. */
static int sr_parse_float(const char *s, double *out) {
    const char *p = s;
    char cleaned[512];   /* a numeric string longer than this is the seam: unparsable */
    size_t n = 0, digits = 0, d = 0;
    int sign = 1;

    if (!s) return 0;
    while (sr_is_space(*p)) p++;
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }

    if (sr_ci_prefix(p, "infinity")) {
        p += 8;
        while (sr_is_space(*p)) p++;
        if (*p) return 0;
        *out = sign < 0 ? -INFINITY : INFINITY;
        return 1;
    }
    if (sr_ci_prefix(p, "inf")) {
        p += 3;
        while (sr_is_space(*p)) p++;
        if (*p) return 0;
        *out = sign < 0 ? -INFINITY : INFINITY;
        return 1;
    }
    if (sr_ci_prefix(p, "nan")) {
        p += 3;
        while (sr_is_space(*p)) p++;
        if (*p) return 0;
        *out = NAN;
        return 1;
    }

    /* int part, then an optional fraction (".5" and "1." are both valid) */
    if (*p >= '0' && *p <= '9') {
        p = sr_digit_run(p, cleaned, &n, sizeof cleaned, &d);
        if (!p) return 0;
        digits += d;
    } else if (*p == '.' && p[1] >= '0' && p[1] <= '9') {
        cleaned[n++] = '0';
        digits++;
    }
    if (*p == '.') {
        if (n + 2 >= sizeof cleaned) return 0;
        cleaned[n++] = '.';
        p++;
        if (*p >= '0' && *p <= '9') {
            p = sr_digit_run(p, cleaned, &n, sizeof cleaned, &d);
            if (!p) return 0;
            digits += d;
        }
    }
    if (!digits) return 0;
    if (sign < 0) {
        if (n + 2 >= sizeof cleaned) return 0;
        memmove(cleaned + 1, cleaned, n);
        cleaned[0] = '-';
        n++;
    }
    if (*p == 'e' || *p == 'E') {
        if (n + 2 >= sizeof cleaned) return 0;
        cleaned[n++] = 'e';
        p++;
        if (*p == '+' || *p == '-') cleaned[n++] = *p++;
        p = sr_digit_run(p, cleaned, &n, sizeof cleaned, &d);
        if (!p) return 0;
    }
    while (sr_is_space(*p)) p++;
    if (*p) return 0;
    cleaned[n] = '\0';
    *out = strtod(cleaned, NULL);
    return 1;
}

/* _seconds(value): max(0.0, min(float(value), 60.0)); 1.0 on TypeError/ValueError. */
static double sr_seconds(const json_t *v) {
    double x;
    if (!v || v->type == JSON_NULL) return 1.0;
    if (v->type == JSON_BOOL) x = v->bool_val ? 1.0 : 0.0;
    else if (v->type == JSON_NUMBER) x = v->num_val;
    else if (v->type == JSON_STRING) {
        if (!sr_parse_float(v->str_val ? v->str_val : "", &x)) return 1.0;
    } else return 1.0;   /* list/dict: TypeError */
    /* min(x, 60.0) then max(0.0, .) -- evaluated in Python's order so a NaN
     * argument lands on 0.0 exactly like CPython's max(0.0, min(nan, 60.0)). */
    if (60.0 < x) x = 60.0;
    if (x > 0.0) return x;
    return 0.0;
}

json_t *w24_standalone_e0a2a7_api_url(const json_t *args) {
    const json_t *pv = json_obj_get(args, "path");
    const char *path;
    char *out;
    size_t blen = strlen(W24_STANDALONE_E0A2A7_API_BASE), plen, need;
    json_t *res;

    /* path.lstrip('/') is an attribute of str: a non-string path is the
     * AttributeError seam, not a str() conversion. */
    if (!pv || pv->type != JSON_STRING) return json_null();
    path = pv->str_val ? pv->str_val : "";
    while (blen > 0 && W24_STANDALONE_E0A2A7_API_BASE[blen - 1] == '/') blen--;
    while (*path == '/') path++;
    plen = strlen(path);
    need = blen + 1 + plen + 1;
    out = (char *)malloc(need);
    if (!out) return json_null();
    memcpy(out, W24_STANDALONE_E0A2A7_API_BASE, blen);
    out[blen] = '/';
    memcpy(out + blen + 1, path, plen + 1);
    res = json_string(out);
    free(out);
    return res;
}

/* str(headers.get("X-RateLimit-Remaining", "")) == "0" */
static int sr_remaining_is_zero(const json_t *headers) {
    const json_t *v = headers && headers->type == JSON_OBJECT
                    ? json_obj_get(headers, "X-RateLimit-Remaining") : NULL;
    if (!v || v->type == JSON_NULL) return 0;          /* "None" / "" */
    if (v->type == JSON_STRING) return strcmp(v->str_val ? v->str_val : "", "0") == 0;
    if (v->type == JSON_BOOL) return 0;                /* "True"/"False" */
    if (v->type == JSON_NUMBER) return v->num_val == 0.0;   /* json int 0 -> "0" */
    return 0;
}

json_t *w24_standalone_e0a2a7_pace(const json_t *args) {
    const json_t *responses = json_obj_get(args, "responses");
    size_t n = (responses && responses->type == JSON_ARRAY) ? json_len(responses) : 0;
    double clock = 0.0, open_at = 0.0;
    int has_open = 0, attempt, status = -1, posts = 0;
    json_t *waits = json_array(), *out;

    for (attempt = 0; attempt <= W24_STANDALONE_E0A2A7_MAX_RETRIES; attempt++) {
        const json_t *resp, *headers, *retry_hdr = NULL, *retry_body = NULL;
        json_t *parsed = NULL;
        double wait = (has_open ? open_at : 0.0) - clock;

        if (wait > 0) {
            json_append(waits, json_number(wait));
            clock += wait;
        }
        if ((size_t)posts >= n) {          /* no scripted answer left: seam */
            json_free(waits);
            return json_null();
        }
        resp = json_get(responses, (size_t)posts);
        if (!resp || resp->type != JSON_OBJECT) break;
        posts++;

        headers = json_obj_get(resp, "headers");
        if (!headers || headers->type != JSON_OBJECT) headers = NULL;
        if (sr_remaining_is_zero(headers)) {
            open_at = clock + sr_seconds(headers ? json_obj_get(headers, "X-RateLimit-Reset-After") : NULL);
            has_open = 1;
        }

        status = (int)json_get_num(resp, "status", 200.0);
        if (status == 429 && attempt < W24_STANDALONE_E0A2A7_MAX_RETRIES) {
            /* retry_after = json.loads(await resp.read() or b"{}").get(
             *     "retry_after", headers.get("Retry-After"))
             * An unreadable body, or a body that is not an object, leaves the
             * header value in place (ValueError/AttributeError are swallowed). */
            const json_t *bv = json_obj_get(resp, "body");
            const char *text = (bv && bv->type == JSON_STRING) ? bv->str_val : NULL;
            if (text && text[0]) parsed = json_parse(text, NULL);
            if (parsed && parsed->type == JSON_OBJECT) retry_body = json_obj_get(parsed, "retry_after");
            retry_hdr = headers ? json_obj_get(headers, "Retry-After") : NULL;
            open_at = clock + sr_seconds(retry_body ? retry_body : retry_hdr);
            has_open = 1;
            json_free(parsed);
            continue;
        }
        break;
    }

    out = json_object();
    json_set(out, "waits", waits);
    json_set(out, "posts", json_number((double)posts));
    json_set(out, "status", json_number((double)status));
    json_set(out, "clock", json_number(clock));
    json_set(out, "open_at", json_number(has_open ? open_at : 0.0));
    return out;
}

json_t *w24_standalone_e0a2a7_seconds(const json_t *args) {
    return json_number(sr_seconds(json_obj_get(args, "value")));
}

json_t *w24_standalone_e0a2a7_constants(void) {
    json_t *o = json_object();
    json_set(o, "max_rate_limit_retries",
             json_number((double)W24_STANDALONE_E0A2A7_MAX_RETRIES));
    json_set(o, "api_base", json_string(W24_STANDALONE_E0A2A7_API_BASE));
    return o;
}
