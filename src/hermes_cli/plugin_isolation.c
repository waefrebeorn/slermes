/*
 * plugin_isolation.c — real C11 port of the pure isolation-policy helpers of
 * upstream Hermes hermes_cli/plugin_isolation.py (gate w15_pluginisol_0cd954).
 *
 * PoP: w15_pluginisol_0cd954_isolation_mode @ hermes_cli/plugin_isolation.py:isolation_mode
 * PoP: w15_pluginisol_0cd954_in_process_import_refusal @ hermes_cli/plugin_isolation.py:in_process_import_refusal
 * PoP: w15_pluginisol_0cd954_host_launcher @ hermes_cli/plugin_isolation.py:host_launcher
 */
#include "noc_w15_pluginisol_0cd954.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* str(raw or "in_process").strip().lower().replace("-", "_") */
const char *w15_pluginisol_0cd954_isolation_mode(const char *cfg_isolation,
                                                const char *host_env) {
    if (host_env && !strcmp(host_env, "1")) return "in_process";
    if (!cfg_isolation || !*cfg_isolation) return "in_process";
    char buf[256];
    size_t j = 0;
    for (const char *p = cfg_isolation; *p && j + 1 < sizeof(buf); p++) {
        char c = *p;
        if (c == '-') c = '_';
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[j++] = c;
    }
    buf[j] = '\0';
    size_t a = 0, b = j;
    while (a < b && isspace((unsigned char)buf[a])) a++;
    while (b > a && isspace((unsigned char)buf[b - 1])) b--;
    buf[b] = '\0'; /* NUL at the shortened length (ownership: local buffer) */
    const char *v = buf + a;
    if (!strcmp(v, "host")) return "host";
    return "in_process"; /* "in_process" and every unknown value */
}

char *w15_pluginisol_0cd954_in_process_import_refusal(const char *what,
                                                     const char *source,
                                                     const char *cfg_isolation,
                                                     const char *host_env) {
    if (source && !strcmp(source, "bundled")) return NULL;
    const char *mode = w15_pluginisol_0cd954_isolation_mode(cfg_isolation, host_env);
    if (strcmp(mode, "host") != 0) return NULL;
    const char *w = what ? what : "";
    static const char *k_tail =
        " runs only in-process, and plugins.isolation is 'host' "
        "(third-party plugin code never runs inside the Hermes process); "
        "set plugins.isolation: in_process to load it";
    size_t n = strlen(w) + strlen(k_tail) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s%s", w, k_tail);
    return out;
}

/* A small POSIX-shlex-compatible splitter: whitespace separates, single quotes
 * are literal, double quotes allow backslash escapes, and backslash escapes the
 * next character outside quotes. */
static json_t *shlex_split_str(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    size_t n = strlen(s), i = 0;
    while (i < n) {
        while (i < n && isspace((unsigned char)s[i])) i++;
        if (i >= n) break;
        char *tok = (char *)malloc(n - i + 1);
        if (!tok) break;
        size_t j = 0;
        int in_s = 0, in_d = 0, quoted = 0;
        while (i < n) {
            char c = s[i];
            if (!in_s && !in_d && isspace((unsigned char)c)) break;
            if (c == '\'' && !in_d) { in_s = !in_s; quoted = 1; i++; continue; }
            if (c == '"' && !in_s) { in_d = !in_d; quoted = 1; i++; continue; }
            if (c == '\\' && !in_s && i + 1 < n) { i++; tok[j++] = s[i++]; continue; }
            tok[j++] = c;
            i++;
        }
        tok[j] = '\0';
        if (j > 0 || quoted) json_append(arr, json_string(tok));
        free(tok);
    }
    return arr;
}

json_t *w15_pluginisol_0cd954_host_launcher(json_t *launcher) {
    if (!launcher) return json_array();
    if (launcher->type == JSON_STRING)
        return shlex_split_str(launcher->str_val ? launcher->str_val : "");
    if (launcher->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(launcher); i++) {
            json_t *part = json_get(launcher, i);
            if (part && part->type == JSON_STRING)
                json_append(out, json_string(part->str_val ? part->str_val : ""));
        }
        return out;
    }
    return json_array();
}

