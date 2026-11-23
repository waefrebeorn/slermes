/*
 * auth_qwen.c — C port of upstream Hermes hermes_cli/auth_qwen.py.
 *
 * PoP: w7_authqwen_4b7464_access_token_is_expiring @ hermes_cli/auth_qwen.py:_qwen_access_token_is_expiring
 * PoP: w7_authqwen_4b7464_qwen_base_url @ hermes_cli/auth_qwen.py:_qwen_base_url
 *
 * Time-dependent and os.getenv-dependent behaviour is exposed through explicit
 * seams (now_seconds / env_value) so the differential oracle is deterministic.
 */
#include "noc_w7_authqwen_4b7464.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Python int(x): NULL/None/containers fail; bool -> 0/1; number truncates;
 * decimal string (optional sign, surrounding whitespace and digit separators)
 * is parsed base-10. Returns 0 on the exception paths. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val;
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        if (e == p) return 0;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        char digs[256];
        size_t dn = 0;
        int prev_digit = 0, any = 0;
        for (; p < e && dn + 1 < sizeof(digs); p++) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= e || !isdigit((unsigned char)p[1])) return 0;
                prev_digit = 0;
                continue;
            }
            if (!isdigit((unsigned char)*p)) return 0;
            digs[dn++] = *p;
            prev_digit = 1;
            any = 1;
        }
        if (!any || p != e) return 0;
        digs[dn] = '\0';
        errno = 0;
        char *end = NULL;
        long long val = strtoll(digs, &end, 10);
        if (errno == ERANGE || (end && *end)) return 0;
        *out = neg ? -val : val;
        return 1;
    }
    return 0;
}

int w7_authqwen_4b7464_access_token_is_expiring(const json_t *expiry_date_ms,
                                                const json_t *skew_seconds,
                                                double now_seconds) {
    long long expiry_ms;
    if (!py_int(expiry_date_ms, &expiry_ms)) return 1;       /* int() raised -> True */
    long long skew = 120;                                   /* module default */
    if (skew_seconds && !py_int(skew_seconds, &skew)) return 1;
    if (skew < 0) skew = 0;                                 /* max(0, ...) */
    double lhs = (now_seconds + (double)skew) * 1000.0;
    return lhs >= (double)expiry_ms ? 1 : 0;
}

struct json_t *w7_authqwen_4b7464_qwen_base_url(const char *env_value) {
    const char *p = env_value ? env_value : "";
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;   /* str.strip() */
    while (e > p && e[-1] == '/') e--;                    /* .rstrip("/") */
    char buf[2048];
    size_t n = (size_t)(e - p);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, p, n);
    buf[n] = '\0';
    if (buf[0] == '\0') return json_string("https://portal.qwen.ai/v1");
    return json_string(buf);
}
