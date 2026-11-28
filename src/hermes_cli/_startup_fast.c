/*
 * _startup_fast.c — faithful C11 port of hermes_cli/_startup_fast.py.
 * Gate w10_startupfa_f8d1b3.
 *
 * PoP: w10_startupfa_f8d1b3_is_global_fast_version_argv @ hermes_cli/_startup_fast.py:is_global_fast_version_argv
 * PoP: w10_startupfa_f8d1b3_is_desktop_ssh_backend_argv @ hermes_cli/_startup_fast.py:is_desktop_ssh_backend_argv
 * PoP: w10_startupfa_f8d1b3_active_profile_may_override_home @ hermes_cli/_startup_fast.py:active_profile_may_override_home
 * PoP: w10_startupfa_f8d1b3_normalize_hermes_home_env @ hermes_cli/_startup_fast.py:normalize_hermes_home_env
 * PoP: w10_startupfa_f8d1b3_read_install_method @ hermes_cli/_startup_fast.py:read_install_method
 */
#include "hermes_json.h"
#include "noc_w10_startupfa_f8d1b3.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static char *strip_dup(const char *s) {
    if (!s) return NULL;
    const char *a = s;
    while (*a && ws((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && ws((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* growable char buffer */
typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 64;
        char *p = (char *)realloc(b->p, cap);
        if (!p) return;
        b->p = p; b->cap = cap;
    }
    b->p[b->n++] = c;
    b->p[b->n] = '\0';
}
static void bputs(buf_t *b, const char *s) { if (!s) return; while (*s) bput(b, *s++); }

/* ntpath.expandvars in the oracle's Windows Python supports $var, ${var} and %var%. */
static char *expand_vars(const char *s, const struct json_t *env) {
    buf_t b = {0};
    bput(&b, '\0'); b.n = 0;
    for (const char *p = s; *p; ) {
        if (*p == '$') {
            if (p[1] == '$') { bput(&b, '$'); p += 2; continue; }
            if (p[1] == '{') {
                const char *close = strchr(p + 2, '}');
                if (!close) { bput(&b, '$'); bput(&b, '{'); p += 2; continue; }
                size_t n = (size_t)(close - (p + 2));
                char *name = (char *)malloc(n + 1);
                if (!name) { bput(&b, '$'); p++; continue; }
                memcpy(name, p + 2, n); name[n] = '\0';
                const char *val = json_get_str(env, name, NULL);
                if (val) bputs(&b, val);
                else { bput(&b, '$'); bput(&b, '{'); bputs(&b, name); bput(&b, '}'); }
                free(name);
                p = close + 1;
                continue;
            }
            if (isalnum((unsigned char)p[1]) || p[1] == '_' || p[1] == '-') {
                const char *q = p + 1;
                while (isalnum((unsigned char)*q) || *q == '_' || *q == '-') q++;
                size_t n = (size_t)(q - (p + 1));
                char *name = (char *)malloc(n + 1);
                if (!name) { bput(&b, '$'); p++; continue; }
                memcpy(name, p + 1, n); name[n] = '\0';
                const char *val = json_get_str(env, name, NULL);
                if (val) bputs(&b, val);
                else { bput(&b, '$'); bputs(&b, name); }
                free(name);
                p = q;
                continue;
            }
            bput(&b, '$'); p++;
            continue;
        }
        if (*p == '%') {
            if (p[1] == '%') { bput(&b, '%'); p += 2; continue; }
            const char *close = strchr(p + 1, '%');
            if (!close) { bput(&b, '%'); p++; continue; }
            size_t n = (size_t)(close - (p + 1));
            char *name = (char *)malloc(n + 1);
            if (!name) { bput(&b, '%'); p++; continue; }
            memcpy(name, p + 1, n); name[n] = '\0';
            const char *val = json_get_str(env, name, NULL);
            if (val) bputs(&b, val);
            else { bput(&b, '%'); bputs(&b, name); bput(&b, '%'); }
            free(name);
            p = close + 1;
            continue;
        }
        bput(&b, *p++);
    }
    if (!b.p) { char *e = (char *)malloc(1); if (e) e[0] = '\0'; return e; }
    return b.p;
}

/* ntpath.expanduser: USERPROFILE first, else HOMEDRIVE+HOMEPATH; ~user only
 * when it names the current USERNAME. Fixture env injects all of these. */
static char *expand_user(const char *s, const struct json_t *env) {
    if (!s || s[0] != '~') return strdup(s ? s : "");
    size_t i = 1;
    while (s[i] && s[i] != '/' && s[i] != '\\') i++;
    const char *home = json_get_str(env, "USERPROFILE", NULL);
    if (!home) {
        const char *hp = json_get_str(env, "HOMEPATH", NULL);
        if (!hp) return strdup(s);
        const char *hd = json_get_str(env, "HOMEDRIVE", "");
        if (!hd) hd = "";
        size_t n = strlen(hd) + strlen(hp) + 1;
        char *buf = (char *)malloc(n);
        if (!buf) return strdup(s);
        snprintf(buf, n, "%s%s", hd, hp);
        home = buf;
    }
    if (i != 1) {
        char *user = (char *)malloc(i);
        if (!user) return strdup(s);
        memcpy(user, s + 1, i - 1); user[i - 1] = '\0';
        const char *uname = json_get_str(env, "USERNAME", NULL);
        int same = uname && !strcmp(user, uname);
        free(user);
        if (!same) return strdup(s);
    }
    size_t n = strlen(home) + strlen(s + i) + 1;
    char *out = (char *)malloc(n);
    if (!out) return strdup(s);
    snprintf(out, n, "%s%s", home, s + i);
    return out;
}

json_t *w10_startupfa_f8d1b3_is_global_fast_version_argv(const struct json_t *argv) {
    if (!argv || argv->type != JSON_ARRAY || argv->c.count != 1) return json_bool(false);
    json_t *a = argv->c.items[0];
    if (!a || a->type != JSON_STRING || !a->str_val) return json_bool(false);
    return json_bool((!strcmp(a->str_val, "--version") || !strcmp(a->str_val, "-V")) ? true : false);
}

json_t *w10_startupfa_f8d1b3_is_desktop_ssh_backend_argv(const struct json_t *argv) {
    if (!argv || argv->type != JSON_ARRAY) return json_bool(false);
    for (size_t i = 0; i < argv->c.count; i++) {
        json_t *a = argv->c.items[i];
        if (a && a->type == JSON_STRING && a->str_val && !strcmp(a->str_val, "--ssh-session-token-file"))
            return json_bool(true);
    }
    return json_bool(false);
}

json_t *w10_startupfa_f8d1b3_active_profile_may_override_home(const char *active_text) {
    char *active = strip_dup(active_text);
    int ok = active && *active && strcmp(active, "default") != 0;
    free(active);
    return json_bool(ok ? true : false);
}

json_t *w10_startupfa_f8d1b3_normalize_hermes_home_env(const struct json_t *raw,
                                                       const struct json_t *env) {
    const char *rawstr = (raw && raw->type == JSON_STRING && raw->str_val) ? raw->str_val : "";
    char *stripped = strip_dup(rawstr);
    if (!stripped || !*stripped) { free(stripped); return json_null(); }
    char *v = expand_vars(stripped, env);
    char *expanded = expand_user(v, env);
    free(v); free(stripped);
    if (!expanded) return json_null();
    if (strcmp(expanded, rawstr) == 0) { free(expanded); return json_null(); }
    json_t *out = json_string(expanded);
    free(expanded);
    return out;
}

json_t *w10_startupfa_f8d1b3_read_install_method(const char *text) {
    char *s = strip_dup(text);
    if (!s || !*s) { free(s); return json_null(); }
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    json_t *out = json_string(s);
    free(s);
    return out;
}

/* PoP: hermes_cli__startup_fast_u_is_global_fast_version_argv
 * @ hermes_cli/_startup_fast.py:is_global_fast_version_argv */
/* argv in (["--version"], ["-V"]) -- exactly one element, exact match. */
int hermes_cli__startup_fast_u_is_global_fast_version_argv(const char *arg) {
    json_t *a = arg ? json_parse(arg, NULL) : NULL;
    int hit = 0;
    if (a && json_is_array(a) && a->c.count == 1) {
        json_t *v = a->c.items[0];
        if (v && json_is_string(v) && v->str_val)
            hit = (strcmp(v->str_val,"--version")==0 || strcmp(v->str_val,"-V")==0);
    }
    if (a) json_free(a);
    return hit;
}
