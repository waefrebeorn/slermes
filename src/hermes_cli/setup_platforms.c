/*
 * setup_platforms.c — faithful C11 port of upstream Hermes
 * hermes_cli/setup_platforms.py (wave14 gate w14_setupplatf_2d6bdc).
 *
 * PoP: w14_setupplatf_2d6bdc_valid_telegram_token @ hermes_cli/setup_platforms.py:_is_valid_telegram_bot_token
 * PoP: w14_setupplatf_2d6bdc_is_progress @ hermes_cli/setup_platforms.py:_is_progress
 * PoP: w14_setupplatf_2d6bdc_allowlist @ hermes_cli/setup_platforms.py:_save_allowlist
 * PoP: w14_setupplatf_2d6bdc_save_port @ hermes_cli/setup_platforms.py:_save_port
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ^\d+:[A-Za-z0-9_-]{30,}$ */
static int valid_token(const char *s) {
    size_t i = 0, hash = 0;
    if (!s) return 0;
    while (s[i] >= '0' && s[i] <= '9') i++;
    if (i == 0 || s[i] != ':') return 0;
    i++;
    while (s[i]) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
        hash++;
        i++;
    }
    return hash >= 30;
}

struct json_t *w14_setupplatf_2d6bdc_valid_telegram_token(const struct json_t *in) {
    return json_bool(valid_token(json_get_str(in, "token", "")) ? 1 : 0);
}

/* status.lower(); not (== "not configured" or startswith partially/plugin disabled) */
static int starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}

struct json_t *w14_setupplatf_2d6bdc_is_progress(const struct json_t *in) {
    const char *s = json_get_str(in, "status", "");
    size_t i, n = s ? strlen(s) : 0;
    char *low = (char *)malloc(n + 1);
    int progress = 1;
    if (!low) return json_bool(1);
    for (i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)s[i]);
    low[n] = '\0';
    if (!strcmp(low, "not configured") || starts_with_ci(s, "partially") ||
        starts_with_ci(s, "plugin disabled")) progress = 0;
    free(low);
    return json_bool(progress);
}

/* users.replace(" ", "") — only the ASCII space character is removed. */
struct json_t *w14_setupplatf_2d6bdc_allowlist(const struct json_t *in) {
    const char *users = json_get_str(in, "users", "");
    size_t i, n = users ? strlen(users) : 0, k = 0;
    char *buf = (char *)malloc(n + 1);
    if (!buf) return json_string("");
    for (i = 0; i < n; i++) {
        if (users[i] != ' ') buf[k++] = users[i];
    }
    buf[k] = '\0';  /* NUL at the (possibly shortened) length */
    {
        struct json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

/* Python int(value) accepting leading/trailing ASCII whitespace and a sign. */
static int py_int(const char *s, long long *out) {
    const char *p = s;
    int neg = 0, digits = 0;
    long long v = 0;
    if (!p) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; digits++; }
    if (!digits) return 0;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return 0;
    *out = neg ? -v : v;
    return 1;
}

struct json_t *w14_setupplatf_2d6bdc_save_port(const struct json_t *in) {
    const char *value = json_get_str(in, "value", "");
    struct json_t *out = json_object();
    long long parsed = 0;
    char buf[64];
    if (!value || !value[0]) {
        json_set(out, "action", json_string("none"));
        json_set(out, "value", json_string(""));
        return out;
    }
    if (py_int(value, &parsed)) {
        snprintf(buf, sizeof buf, "%lld", parsed);
        json_set(out, "action", json_string("save"));
        json_set(out, "value", json_string(buf));
    } else {
        json_set(out, "action", json_string("warn"));
        json_set(out, "value", json_string(""));
    }
    return out;
}
