/*
 * auxiliary_unavailable.c — faithful C11 port of agent/auxiliary_unavailable.py
 * (gate w14_auxiliaryu_c855f2).
 *
 * PoP: w14_auxiliaryu_c855f2_sentence @ agent/auxiliary_unavailable.py:_sentence
 * PoP: w14_auxiliaryu_c855f2_missing_provider_credentials_message_ex @ agent/auxiliary_unavailable.py:missing_provider_credentials_message
 *
 * The provider-message entry is an explicit `_ex` seam: the two live
 * in-function lookups (PROVIDER_REGISTRY / LLAMACPP_ALIASES) and the
 * pool_cooldown_message() probe are injected from the fixture, exactly as the
 * oracle injects them into the live function. The message-building branch
 * logic itself is the real port.
 */
#include "noc_w14_auxiliaryu_c855f2.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static bool py_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == 0.0) return dupstr("0");
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else
            snprintf(buf, sizeof(buf), "%g", d);
        return dupstr(buf);
    }
    return dupstr(v->type == JSON_ARRAY ? "[]" : "{}");
}

/* ASCII str.strip() then rstrip('.') then + '.' — NUL written at new length. */
char *w14_auxiliaryu_c855f2_sentence(const json_t *text) {
    char *s = py_str(text);
    size_t n, a = 0, b;
    char *out;
    if (!s) return NULL;
    n = strlen(s); b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a && b > a) memmove(s, s + a, b - a);
    b -= a;
    while (b > 0 && s[b - 1] == '.') b--;
    s[b] = '\0';
    out = (char *)malloc(b + 2);
    if (out) { memcpy(out, s, b); out[b] = '.'; out[b + 1] = '\0'; }
    free(s);
    return out;
}

typedef struct { char *p; size_t len, cap; } strbuf;

static void sb_init(strbuf *b) {
    b->cap = 256; b->len = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}
static void sb_add(strbuf *b, const char *s) {
    size_t n;
    if (!b->p || !s) return;
    n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        while (b->len + n + 1 > b->cap) b->cap *= 2;
        b->p = (char *)realloc(b->p, b->cap);
        if (!b->p) return;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

char *w14_auxiliaryu_c855f2_missing_provider_credentials_message_ex(
        const char *provider_id, const json_t *cooldown, const json_t *pconfig, bool llama_alias) {
    const char *pid = provider_id ? provider_id : "";
    bool pc_present = (pconfig && pconfig->type == JSON_OBJECT);
    const json_t *envs = pc_present ? json_obj_get(pconfig, "api_key_env_vars") : NULL;
    const json_t *auth_type = pc_present ? json_obj_get(pconfig, "auth_type") : NULL;
    const char *problem = "no API key was found";
    char *remedy = NULL;
    char *first_env = NULL;
    char *auth_s = NULL;
    char *upper = NULL;
    static const char *SWITCH = "switch to a different provider with `hermes model`.";
    strbuf b;

    if (cooldown && cooldown->type == JSON_STRING && cooldown->str_val && cooldown->str_val[0]) {
        return dupstr(cooldown->str_val); /* live: `if cooldown: return cooldown` */
    }

    if (llama_alias) {
        problem = "no local llama.cpp server is running";
        remedy = dupstr("Start it in Settings → Providers → Local models");
    } else if (envs && envs->type == JSON_ARRAY && envs->c.count > 0) {
        first_env = py_str(json_get(envs, 0));
        remedy = (char *)malloc(strlen(first_env) + 40);
        if (remedy) snprintf(remedy, strlen(first_env) + 40, "Set the %s environment variable", first_env);
    } else if (!pc_present) {
        size_t i;
        upper = dupstr(pid);
        for (i = 0; upper[i]; ++i) {
            if (upper[i] == '-') upper[i] = '_';
            else upper[i] = (char)toupper((unsigned char)upper[i]);
        }
        remedy = (char *)malloc(strlen(upper) + 40);
        if (remedy) snprintf(remedy, strlen(upper) + 40, "Set the %s_API_KEY environment variable", upper);
    } else {
        auth_s = py_str(auth_type);
        if (!strncmp(auth_s, "oauth", 5)) {
            problem = "no credentials were found";
            remedy = (char *)malloc(strlen(pid) + 60);
            if (remedy) snprintf(remedy, strlen(pid) + 60, "Run `hermes auth add %s` to sign in", pid);
        } else {
            problem = "no credentials were found";
        }
    }

    sb_init(&b);
    sb_add(&b, "Provider '");
    sb_add(&b, pid);
    sb_add(&b, "' is set in config.yaml but ");
    sb_add(&b, problem);
    sb_add(&b, ". ");
    if (remedy && remedy[0]) {
        sb_add(&b, remedy);
        sb_add(&b, ", or ");
        sb_add(&b, SWITCH);
    } else {
        /* str.capitalize(): first char uppercase, rest lowercase. */
        sb_add(&b, "Switch to a different provider with `hermes model`.");
    }
    free(first_env); free(auth_s); free(upper); free(remedy);
    return b.p;
}
