/*
 * approval_context.c - C home for upstream Hermes tools/approval_context.py.
 *
 * PoP: w9_approvalcon_d5abfd_normalize_mode @ tools/approval_context.py:_normalize_approval_mode
 * PoP: w9_approvalcon_d5abfd_format_window @ tools/approval_context.py:format_approval_window
 * PoP: w9_approvalcon_d5abfd_binary_mode @ tools/approval_context.py:_binary_approval_mode
 * PoP: w9_approvalcon_d5abfd_unattended_platform @ tools/approval_context.py:_is_unattended_platform_approval_context
 * PoP: w9_approvalcon_d5abfd_prompt_authority @ tools/approval_context.py:prompt_authority
 */
#include "noc_w9_approvalcon_d5abfd.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python str(value).lower().strip() for JSON scalars. */
static char *lower_strip(const char *s) {
    if (!s) s = "";
    const unsigned char *a = (const unsigned char *)s;
    while (*a && isspace(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && isspace(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *p = (char *)malloc(n + 1);
    if (p) {
        for (size_t i = 0; i < n; ++i) p[i] = (char)tolower(a[i]);
        p[n] = '\0';
    }
    return p;
}

static char *json_str_coerce(const struct json_t *v) {
    if (!v) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        char buf[64];
        if (isfinite(d) && d == (double)(long long)d && d >= -9e15 && d <= 9e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return dupstr(buf);
    }
    return dupstr("");
}

char *w9_approvalcon_d5abfd_normalize_mode(const struct json_t *value) {
    if (value && value->type == JSON_BOOL)
        return dupstr(value->bool_val ? "manual" : "off");
    if (value && value->type == JSON_STRING) {
        char *n = lower_strip(value->str_val ? value->str_val : "");
        if (n && *n && (!strcmp(n, "manual") || !strcmp(n, "smart") || !strcmp(n, "off"))) {
            char *r = dupstr(n);
            free(n);
            return r;
        }
        free(n);
    }
    return dupstr("manual");
}

char *w9_approvalcon_d5abfd_format_window(const char *seconds_dec) {
    long long sec = 0;
    if (seconds_dec && *seconds_dec) {
        char *end = NULL;
        errno = 0;
        long long v = strtoll(seconds_dec, &end, 10);
        if (end && end != seconds_dec) sec = v;
    }
    if (sec < 0) sec = 0;
    const char *unit;
    long long count;
    if (sec && sec % 3600 == 0) { unit = "hour"; count = sec / 3600; }
    else if (sec && sec % 60 == 0) { unit = "minute"; count = sec / 60; }
    else { unit = "second"; count = sec; }
    const char *suf = (count == 1) ? "one" : "other";
    char out[160];
    snprintf(out, sizeof(out), "approval.window.%s_%s|%lld", unit, suf, count);
    return dupstr(out);
}

char *w9_approvalcon_d5abfd_binary_mode(const struct json_t *value) {
    char *s = json_str_coerce(value);
    char *n = lower_strip(s ? s : "");
    free(s);
    int ok = n && (!strcmp(n, "approve") || !strcmp(n, "off") ||
                   !strcmp(n, "allow") || !strcmp(n, "yes"));
    free(n);
    return dupstr(ok ? "approve" : "deny");
}

int w9_approvalcon_d5abfd_unattended_platform(const char *platform) {
    if (!platform) return 0;
    return !strcmp(platform, "webhook") || !strcmp(platform, "msgraph_webhook") ||
           !strcmp(platform, "api_server");
}

struct json_t *w9_approvalcon_d5abfd_prompt_authority(const struct json_t *req, const struct json_t *env,
                                    const struct json_t *approvers) {
    struct json_t *out = json_object();
    const char *names[3] = {"requester_id", "requester_name", "chat_type"};
    const char *envs[3] = {"HERMES_SESSION_ACTOR_ID", "HERMES_SESSION_ACTOR_NAME",
                            "HERMES_SESSION_CHAT_TYPE"};
    for (int i = 0; i < 3; ++i) {
        struct json_t *v = req ? json_obj_get(req, names[i]) : NULL;
        char *s;
        if (v && v->type != JSON_NULL) {
            s = json_str_coerce(v);
        } else {
            const char *e = env ? json_get_str(env, envs[i], "") : "";
            s = dupstr(e ? e : "");
        }
        json_set(out, names[i], json_string(s ? s : ""));
        free(s);
    }
    if (!approvers || approvers->type == JSON_NULL) {
        struct json_t *a = json_array();
        json_append(a, json_string("*"));
        json_set(out, "approvers", a);
    } else {
        struct json_t *a = json_array();
        for (size_t i = 0; i < json_len(approvers); ++i) {
            struct json_t *it = json_get(approvers, i);
            json_append(a, it ? json_copy(it) : json_null());
        }
        json_set(out, "approvers", a);
    }
    return out;
}
