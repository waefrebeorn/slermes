/*
 * bot_failure_reasons.c — C home for upstream Hermes tools/bot_failure_reasons.py.
 * PoP: w12_botfailure_9eb29a_retry_action @ tools/bot_failure_reasons.py:retry_action
 * PoP: w12_botfailure_9eb29a_turn_failure_text @ tools/bot_failure_reasons.py:turn_failure_text
 * PoP: w12_botfailure_9eb29a_classify_agent_error @ tools/bot_failure_reasons.py:classify_agent_error
 * PoP: w12_botfailure_9eb29a_result_retry_action @ tools/bot_failure_reasons.py:result_retry_action
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w12_botfailure_9eb29a.h"

/* --- growable string buffer --- */
typedef struct { char *data; size_t len, cap; } w12_sb_t;
static void w12_sb_init(w12_sb_t *b) {
    b->cap = 256; b->len = 0; b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}
static void w12_sb_free(w12_sb_t *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }
static void w12_sb_addn(w12_sb_t *b, const char *s, size_t n) {
    if (!b->data || !s || !n) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap;
        while (nc < b->len + n + 1) nc *= 2;
        char *nd = (char *)realloc(b->data, nc);
        if (!nd) return;
        b->data = nd; b->cap = nc;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n; b->data[b->len] = '\0'; /* NUL at logical length */
}
static void w12_sb_add(w12_sb_t *b, const char *s) { if (s) w12_sb_addn(b, s, strlen(s)); }
static char *w12_sb_take(w12_sb_t *b) {
    char *p = b->data; b->data = NULL; b->len = b->cap = 0; return p;
}

/* Python str.strip() for ASCII whitespace; NULL -> NULL. */
static char *w12_strip(const char *s) {
    char *out; size_t n;
    if (!s) return NULL;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}
static char *w12_lower_dup(const char *s) {
    size_t n = s ? strlen(s) : 0, i;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}
static int w12_py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

char *w12_botfailure_9eb29a_retry_action(const char *reason) {
    if (!reason) reason = "";
    if (!strcmp(reason, "runtime_offline") || !strcmp(reason, "delivery_timeout") ||
        !strcmp(reason, "provider_rate_limit") || !strcmp(reason, "provider_server_error"))
        return strdup("resume");
    if (!strcmp(reason, "context_overflow")) return strdup("compress_then_resume");
    return strdup("none");
}

char *w12_botfailure_9eb29a_turn_failure_text(const char *stdout_text, const char *stderr_text) {
    const char *streams[2];
    int i, first = 1;
    w12_sb_t b;
    w12_sb_init(&b);
    streams[0] = stdout_text; streams[1] = stderr_text;
    for (i = 0; i < 2; i++) {
        char *stripped;
        if (!streams[i]) continue;
        stripped = w12_strip(streams[i]);
        if (!stripped || !*stripped) { free(stripped); continue; }
        if (!first) w12_sb_add(&b, "\n");
        w12_sb_add(&b, stripped);
        first = 0;
        free(stripped);
    }
    return w12_sb_take(&b);
}

/* _STATUS = (?:error code:?\s*|status(?:\s*code)?:?\s*|http\s*) -- match <prefix><code>\b. */
static int w12_status_code(const char *low, const char *code) {
    static const char *const prefixes[3] = {"error code", "status", "http"};
    int k;
    for (k = 0; k < 3; k++) {
        const char *prefix = prefixes[k], *p = low, *q;
        while ((p = strstr(p, prefix)) != NULL) {
            q = p + strlen(prefix);
            if (k == 1) { /* status(?:\s*code)? */
                while (*q && isspace((unsigned char)*q)) q++;
                if (!strncmp(q, "code", 4)) q += 4;
            }
            if (*q == ':') q++;
            while (*q && isspace((unsigned char)*q)) q++;
            if (!strncmp(q, code, strlen(code))) {
                const char *r = q + strlen(code);
                if (!isalnum((unsigned char)*r) && *r != '_') return 1; /* \b */
            }
            p++;
        }
    }
    return 0;
}
static int w12_status_5xx(const char *low) {
    static const char *const prefixes[3] = {"error code", "status", "http"};
    int k;
    for (k = 0; k < 3; k++) {
        const char *prefix = prefixes[k], *p = low, *q;
        while ((p = strstr(p, prefix)) != NULL) {
            q = p + strlen(prefix);
            if (k == 1) {
                while (*q && isspace((unsigned char)*q)) q++;
                if (!strncmp(q, "code", 4)) q += 4;
            }
            if (*q == ':') q++;
            while (*q && isspace((unsigned char)*q)) q++;
            if (q[0] == '5' && isdigit((unsigned char)q[1]) && isdigit((unsigned char)q[2])) {
                char next = q[3];
                if (!isalnum((unsigned char)next) && next != '_') return 1;
            }
            p++;
        }
    }
    return 0;
}
/* rate.?limit */
static int w12_rate_limit(const char *low) {
    const char *p = low;
    while ((p = strstr(p, "rate")) != NULL) {
        const char *q = p + 4;
        if (!strncmp(q, "limit", 5)) return 1;
        if (q[0] && q[0] != '\n' && !strncmp(q + 1, "limit", 5)) return 1;
        p++;
    }
    return 0;
}
/* server[ _]?error */
static int w12_server_error(const char *low) {
    const char *p = low;
    while ((p = strstr(p, "server")) != NULL) {
        const char *q = p + 6;
        if (!strncmp(q, "error", 5)) return 1;
        if ((*q == ' ' || *q == '_') && !strncmp(q + 1, "error", 5)) return 1;
        p++;
    }
    return 0;
}
/* model .*(not found|does not exist) | model_not_found */
static int w12_model_unavailable(const char *low) {
    const char *p;
    if (strstr(low, "model_not_found")) return 1;
    p = low;
    while ((p = strstr(p, "model ")) != NULL) {
        const char *rest = p + 6;
        if (strstr(rest, "not found") || strstr(rest, "does not exist")) return 1;
        p++;
    }
    return 0;
}
/* could not read this profile's [^(]+\(an internal profile-scoping bug */
static int w12_target_scope(const char *low) {
    const char *needle = "could not read this profile's ";
    const char *p = strstr(low, needle), *q;
    if (p) {
        q = p + strlen(needle);
        if (*q && *q != '(') {
            while (*q && *q != '(') q++;
            if (!strncmp(q, "(an internal profile-scoping bug", 32)) return 1;
        }
    }
    if (strstr(low, "served_profile_child_env(inherit_credentials=true)")) return 1;
    return 0;
}

char *w12_botfailure_9eb29a_classify_agent_error(const char *text) {
    char *raw = text ? strdup(text) : strdup("");
    char *stripped = w12_strip(raw);
    char *low;
    const char *UNKNOWN = "unknown";
    if (!stripped || !*stripped) { free(stripped); free(raw); return strdup(UNKNOWN); }
    free(stripped);
    low = w12_lower_dup(raw);
    free(raw);
    if (!low) return strdup(UNKNOWN);

    if (w12_target_scope(low)) { free(low); return strdup("target_scope_unresolved"); }
    if (strstr(low, "authentication_error") || strstr(low, "invalid api key") ||
        w12_status_code(low, "401") || w12_status_code(low, "403")) {
        free(low); return strdup("provider_auth_or_access");
    }
    if (w12_status_code(low, "402") || strstr(low, "out of funds") ||
        strstr(low, "quota") || strstr(low, "balance")) {
        free(low); return strdup("provider_quota_limit");
    }
    if (w12_status_code(low, "429") || w12_rate_limit(low)) {
        free(low); return strdup("provider_rate_limit");
    }
    if (w12_status_5xx(low) || w12_server_error(low) || strstr(low, "overloaded")) {
        free(low); return strdup("provider_server_error");
    }
    if (strstr(low, "context length") || strstr(low, "context_overflow") ||
        strstr(low, "maximum context")) {
        free(low); return strdup("context_overflow");
    }
    if (strstr(low, "no llm provider configured") || strstr(low, "missing config") ||
        strstr(low, "no access token")) {
        free(low); return strdup("missing_config");
    }
    if (w12_model_unavailable(low)) { free(low); return strdup("model_unavailable"); }
    free(low);
    return strdup(UNKNOWN);
}

char *w12_botfailure_9eb29a_result_retry_action(const json_t *result) {
    json_t *e, *fr;
    const char *so, *se;
    char *txt, *code;
    if (!w12_py_truthy(result) || result->type != JSON_OBJECT) return strdup("none");
    if (!w12_py_truthy(json_obj_get(result, "failed"))) return strdup("none");
    e = json_obj_get(result, "error");
    fr = json_obj_get(result, "failure_reason");
    so = (e && e->type == JSON_STRING) ? e->str_val : NULL;
    se = (fr && fr->type == JSON_STRING) ? fr->str_val : NULL;
    txt = w12_botfailure_9eb29a_turn_failure_text(so, se);
    code = w12_botfailure_9eb29a_classify_agent_error(txt);
    free(txt);
    /* Python: retry_action(classify_agent_error(turn_failure_text(...))). */
    {
        char *action = w12_botfailure_9eb29a_retry_action(code);
        free(code);
        return action;
    }
}
