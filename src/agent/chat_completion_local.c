/*
 * chat_completion_local.c — faithful C11 port of agent/chat_completion_local.py
 * (gate w14_chatcomple_840065).
 *
 * PoP: w14_chatcomple_840065_stale_giveup_ex @ agent/chat_completion_local.py:stale_giveup
 * PoP: w14_chatcomple_840065_streams_llamacpp_ex @ agent/chat_completion_local.py:_streams_llamacpp
 * PoP: w14_chatcomple_840065_retry_recovered_local_server_ex @ agent/chat_completion_local.py:retry_recovered_local_server
 *
 * The three seams inject the live function's environment / registry /
 * collaborator results so the deterministic branch logic is the real port.
 */
#include "noc_w14_chatcomple_840065.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t a = 0, b;
    if (!t) return NULL;
    b = strlen(t);
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';
    return t;
}

/* Python int(str) subset: sign + decimal digits with single underscores. */
static bool py_int_parse(const char *s, long long *out) {
    size_t n, i, k = 0;
    bool neg = false, prev_us = false;
    char *clean;
    char *end;
    long long v;

    if (!s) return false;
    n = strlen(s);
    if (!n) return false;
    i = 0;
    if (s[0] == '+' || s[0] == '-') { neg = (s[0] == '-'); i = 1; }
    if (i >= n) return false;
    clean = (char *)malloc(n + 1);
    if (!clean) return false;
    for (; i < n; ++i) {
        if (s[i] == '_') {
            if (k == 0 || prev_us || i + 1 >= n || s[i + 1] < '0' || s[i + 1] > '9') {
                free(clean); return false;
            }
            prev_us = true;
            continue;
        }
        if (s[i] < '0' || s[i] > '9') { free(clean); return false; }
        clean[k++] = s[i];
        prev_us = false;
    }
    clean[k] = '\0';
    errno = 0;
    v = strtoll(clean, &end, 10);
    if (end != clean + k || errno == ERANGE) { free(clean); return false; }
    free(clean);
    *out = neg ? -v : v;
    return true;
}

/* env_int("HERMES_STREAM_STALE_GIVEUP", 5) with the injected raw env value. */
int w14_chatcomple_840065_stale_giveup_ex(const char *env_value) {
    char *t = strip_dup(env_value ? env_value : "");
    long long v = 5;
    bool ok = false;
    if (t && t[0]) ok = py_int_parse(t, &v);
    free(t);
    return ok ? (int)v : 5;
}

bool w14_chatcomple_840065_streams_llamacpp_ex(const char *api_mode, const char *provider,
                                              const char *base_url, bool managed) {
    (void)provider; (void)base_url; /* injected `managed` already folds both */
    if (!api_mode) return false;    /* None not in ("", "chat_completions") */
    if (!strcmp(api_mode, "") || !strcmp(api_mode, "chat_completions")) return managed;
    return false;
}

json_t *w14_chatcomple_840065_retry_recovered_local_server_ex(long long giveup, long long streak,
                                                             bool streams, bool answers) {
    json_t *o = json_object();
    bool ok = (giveup > 0) && (streak >= giveup) && streams && answers;
    json_set(o, "result", json_bool(ok));
    if (ok) json_set(o, "set", json_number((double)(giveup - 1)));
    else json_set(o, "set", json_null());
    return o;
}
