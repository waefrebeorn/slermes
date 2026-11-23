/*
 * secret_scope.c — faithful C11 port of upstream Hermes agent/secret_scope.py.
 *
 * PoP: secretscope_strip_inline_comment @ agent/secret_scope.py:_strip_inline_comment
 * PoP: secretscope_parse_env_value @ agent/secret_scope.py:_parse_env_value
 * PoP: secretscope_parse_env_text @ agent/secret_scope.py:_parse_env_text
 * PoP: secretscope_get_secret @ agent/secret_scope.py:get_secret
 *
 * The process-global / context-local state (os.environ, bound scope,
 * multiplex flags, launch-body scope, the global-env allowlist) is injected
 * through explicit arguments so no process environment is mutated (the
 * Windows CRT setenv("") pitfall).
 */
#include "noc_w6_secretscope_bc3fee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *ss_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static const char *ss_strip(const char *s) {
    if (!s) return "";
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}
static size_t ss_strip_len(const char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    return n;
}
/* Python value.strip() applied to a (possibly interior) span. */
static char *ss_stripped_dup(const char *s) {
    const char *t = ss_strip(s);
    return ss_ndup(t, ss_strip_len(t));
}

json_t *secretscope_strip_inline_comment(const char *value) {
    char *v = ss_stripped_dup(value ? value : "");
    if (!v[0]) return json_string(v);
    char quote = v[0];
    if (quote == '\'' || quote == '"') {
        size_t n = strlen(v);
        size_t i = 1;
        while (i < n) {
            char ch = v[i];
            if (quote == '"' && ch == '\\') { i += 2; continue; }
            if (ch == quote) {
                const char *rest = ss_strip(v + i + 1);
                if (*rest == '#') {
                    char *cut = ss_ndup(v, i + 1);
                    free(v);
                    json_t *r = json_string(cut);
                    free(cut);
                    return r;
                }
                return json_string(v);
            }
            i++;
        }
        return json_string(v);   /* unterminated quote: unchanged */
    }
    /* unquoted: re.split(r"\s+#", value, maxsplit=1)[0].strip() */
    size_t n = strlen(v);
    size_t cut = n;
    for (size_t i = 0; i + 1 < n; i++) {
        if (isspace((unsigned char)v[i]) && v[i + 1] == '#') { cut = i; break; }
    }
    char *out = ss_ndup(v, cut);
    char *out2 = ss_stripped_dup(out);
    free(out); free(v);
    json_t *r = json_string(out2);
    free(out2);
    return r;
}

json_t *secretscope_parse_env_value(const char *raw) {
    const char *s0 = ss_strip(raw ? raw : "");
    size_t n = strlen(s0);
    if (n >= 2 && s0[0] == '"' && s0[n - 1] == '"') {
        const char *q = s0 + 1;
        size_t qn = n - 2;
        char *out = (char *)malloc(qn + 1);
        size_t o = 0;
        for (size_t i = 0; i < qn; i++) {
            int escaped = (q[i] == '\\' && i + 1 < qn && (q[i + 1] == '"' || q[i + 1] == '\\'));
            out[o++] = escaped ? q[++i] : q[i];
        }
        out[o] = '\0';
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    if (n >= 2 && s0[0] == '\'' && s0[n - 1] == '\'') {
        char *out = ss_ndup(s0 + 1, n - 2);
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    char *bare = ss_stripped_dup(raw ? raw : "");
    json_t *r = json_string(bare);
    free(bare);
    return r;
}

json_t *secretscope_parse_env_text(const char *text) {
    json_t *secrets = json_object();
    if (!text) return secrets;
    /* splitlines(): iterate lines; '\r' is removed by strip() on each line. */
    const char *p = text;
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        char *raw = ss_ndup(p, len);
        char *line = ss_stripped_dup(raw);
        free(raw);
        if (!line[0] || line[0] == '#') { free(line); p = e ? e + 1 : p + strlen(p); continue; }
        const char *body = line;
        if (strncmp(body, "export ", 7) == 0) {
            body = ss_strip(body + 7);
        }
        const char *eq = strchr(body, '=');
        if (eq) {
            char *key = ss_ndup(body, (size_t)(eq - body));
            char *key_trim = ss_stripped_dup(key);
            free(key);
            if (key_trim[0]) {
                json_t *comment = secretscope_strip_inline_comment(eq + 1);
                json_t *parsed = secretscope_parse_env_value(json_string_value(comment));
                json_obj_del(secrets, key_trim);      /* duplicate: last wins */
                json_set(secrets, key_trim, parsed);
                json_free(comment);
            }
            free(key_trim);
        }
        free(line);
        if (!e) break;
        p = e + 1;
    }
    return secrets;
}

/* --- get_secret state helpers --- */
static int ss_in_array(const json_t *arr, const char *name) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = json_string_value(json_get(arr, i));
        if (s && !strcmp(s, name)) return 1;
    }
    return 0;
}
static int ss_startswith_any(const json_t *arr, const char *name) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *s = json_string_value(json_get(arr, i));
        if (s && *s && strncmp(name, s, strlen(s)) == 0) return 1;
    }
    return 0;
}
static json_t *ss_environ_or(const char *name, const json_t *def, const json_t *env) {
    json_t *v = (env && env->type == JSON_OBJECT) ? json_obj_get(env, name) : NULL;
    if (v && v->type != JSON_NULL) return json_copy(v);
    return json_copy(def ? def : json_null());
}
static json_t *ss_unscoped(void) {
    json_t *o = json_object();
    json_set(o, "error", json_string("UnscopedSecretError"));
    return o;
}

json_t *secretscope_get_secret(const char *name, const json_t *def,
                               const json_t *env, const json_t *bound,
                               int multiplex_active, int serves_routed,
                               const json_t *launch_scope,
                               const json_t *global_names,
                               const json_t *global_prefixes,
                               const json_t *launch_only_names) {
    if (!name) name = "";
    int is_global = ss_in_array(global_names, name) ||
                    ss_startswith_any(global_prefixes, name) ||
                    ss_in_array(launch_only_names, name);
    if (is_global) return ss_environ_or(name, def, env);

    if (bound && bound->type == JSON_OBJECT) {
        json_t *val = json_obj_get(bound, name);
        if (val && val->type != JSON_NULL) return json_copy(val);
        return serves_routed ? json_copy(def ? def : json_null())
                             : ss_environ_or(name, def, env);
    }
    if (multiplex_active) {
        if (launch_scope && launch_scope->type == JSON_OBJECT) {
            json_t *v = json_obj_get(launch_scope, name);
            if (v && v->type != JSON_NULL) return json_copy(v);
            return json_copy(def ? def : json_null());   /* dict.get(name, default) */
        }
        return ss_unscoped();
    }
    return ss_environ_or(name, def, env);
}
