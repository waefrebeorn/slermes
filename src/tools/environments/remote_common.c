/*
 * remote_common.c - C11 port of upstream tools/environments/remote_common.py
 * (gate w24_remotecomm_c4bdad).
 *
 * PoP: w24_remotecomm_c4bdad_shlex_quote @ tools/environments/remote_common.py:shlex.quote (stdlib, used by prepend_unset)
 * PoP: w24_remotecomm_c4bdad_prepend_unset @ tools/environments/remote_common.py:prepend_unset
 * PoP: w24_remotecomm_c4bdad_bash_argv @ tools/environments/remote_common.py:bash_argv
 * PoP: w24_remotecomm_c4bdad_client_env_with @ tools/environments/remote_common.py:client_env_with
 * PoP: w24_remotecomm_c4bdad_resolve_passthrough @ tools/environments/remote_common.py:resolve_passthrough_env
 * PoP: w24_remotecomm_c4bdad_constants @ tools/environments/remote_common.py:<literal patterns>
 *
 * The module's pure core, ported exactly: shlex's quote/unsafe-character rule,
 * the sorted-and-quoted ``unset`` prefix, the spawn argv tail, the client env
 * merge, and the passthrough set algebra (implicit = registry - internal
 * secrets; forward = explicit | (implicit - blocklist); value = env or .env;
 * unset = multiplex-scoped names the active scope lacks).  Everything the
 * upstream reads from the process (os.environ, the .env loader, the passthrough
 * registry, the secret/scope predicates, the resolver) arrives as a value.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_remotecomm_c4bdad.h"

/* ---- text helpers ---------------------------------------------------- */

static char *rc_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

/* shlex's _find_unsafe: [^\w@%+=:,./-] with re.ASCII. */
static int rc_shlex_safe(const char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '_' || c == '@' || c == '%' || c == '+' || c == '=' || c == ':'
            || c == ',' || c == '.' || c == '/' || c == '-') continue;
        return 0;
    }
    return 1;
}

/* malloc'd shlex.quote(s) */
static char *rc_quote(const char *s) {
    size_t i, out = 0, n;
    char *buf;
    if (!s) s = "";
    if (!s[0]) return rc_dup("''");
    if (rc_shlex_safe(s)) return rc_dup(s);
    n = strlen(s);
    buf = (char *)malloc(n * 4 + 3);
    if (!buf) return NULL;
    buf[out++] = '\'';
    for (i = 0; i < n; i++) {
        if (s[i] == '\'') {
            memcpy(buf + out, "'\"'\"'", 5);
            out += 5;
        } else {
            buf[out++] = s[i];
        }
    }
    buf[out++] = '\'';
    buf[out] = '\0';
    return buf;
}

/* ^[A-Za-z_][A-Za-z0-9_]*$ */
static int rc_is_shell_env_name(const char *s) {
    size_t i;
    if (!s || !s[0]) return 0;
    for (i = 0; s[i]; i++) {
        unsigned char c = (unsigned char)s[i];
        int alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        int alnum = alpha || (c >= '0' && c <= '9');
        if (i == 0 ? !alpha : !alnum) return 0;
    }
    return 1;
}

/* ---- name sets ------------------------------------------------------- */

typedef struct {
    char **items;
    size_t n, cap;
} rc_set;

static void rc_set_free(rc_set *s) {
    for (size_t i = 0; i < s->n; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL;
    s->n = s->cap = 0;
}

static int rc_set_has(const rc_set *s, const char *key) {
    for (size_t i = 0; i < s->n; i++)
        if (strcmp(s->items[i], key) == 0) return 1;
    return 0;
}

static void rc_set_add(rc_set *s, const char *key) {
    if (!key || rc_set_has(s, key)) return;
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **grown = (char **)realloc(s->items, cap * sizeof(char *));
        if (!grown) return;
        s->items = grown;
        s->cap = cap;
    }
    s->items[s->n] = rc_dup(key);
    if (s->items[s->n]) s->n++;
}

static void rc_set_from_json(rc_set *s, const json_t *arr) {
    size_t i, n;
    if (!arr || arr->type != JSON_ARRAY) return;
    n = json_len(arr);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING) rc_set_add(s, v->str_val);
    }
}

static int rc_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Python truthiness of a JSON value (values here are env strings / null). */
static int rc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* ---- exported port -------------------------------------------------- */

json_t *w24_remotecomm_c4bdad_shlex_quote(const json_t *s) {
    char *q;
    json_t *out;
    if (!s || s->type != JSON_STRING) return json_string("''");
    q = rc_quote(s->str_val ? s->str_val : "");
    out = q ? json_string(q) : json_null();
    free(q);
    return out;
}

json_t *w24_remotecomm_c4bdad_prepend_unset(const json_t *cmd, const json_t *names) {
    char **list = NULL;
    size_t i, n = 0, need;
    char *line, *p;
    json_t *out;

    if (!cmd || cmd->type != JSON_STRING) return json_null();
    if (names && names->type == JSON_ARRAY) n = json_len(names);
    if (n == 0) return json_string(cmd->str_val ? cmd->str_val : "");
    list = (char **)calloc(n, sizeof(char *));
    if (!list) return json_null();
    need = 1;
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(names, i);
        if (v && v->type == JSON_STRING) list[i] = rc_dup(v->str_val ? v->str_val : "");
        else list[i] = rc_dup("");
        need += strlen(list[i]) * 4 + 3;
    }
    qsort(list, n, sizeof(char *), rc_cmp_str);      /* sorted(), duplicates kept */

    need += 32 + strlen(cmd->str_val ? cmd->str_val : "");
    line = (char *)malloc(need);
    if (!line) {
        for (i = 0; i < n; i++) free(list[i]);
        free(list);
        return json_null();
    }
    p = line;
    p += sprintf(p, "unset ");
    for (i = 0; i < n; i++) {
        char *q = rc_quote(list[i]);
        if (!q) break;
        if (i) *p++ = ' ';
        memcpy(p, q, strlen(q));
        p += strlen(q);
        free(q);
        free(list[i]);
    }
    sprintf(p, " 2>/dev/null || true\n%s", cmd->str_val ? cmd->str_val : "");
    out = json_string(line);
    free(line);
    free(list);
    return out;
}

json_t *w24_remotecomm_c4bdad_bash_argv(const json_t *cmd, const json_t *login) {
    json_t *arr = json_array();
    const char *text = cmd && cmd->type == JSON_STRING ? cmd->str_val : NULL;
    json_append(arr, json_string("bash"));
    if (rc_truthy(login)) json_append(arr, json_string("-l"));
    json_append(arr, json_string("-c"));
    json_append(arr, json_string(text ? text : ""));
    return arr;
}

json_t *w24_remotecomm_c4bdad_client_env_with(const json_t *values, const json_t *host_env) {
    size_t i, n;
    json_t *out;

    if (!values || values->type != JSON_OBJECT || json_object_size(values) == 0)
        return json_null();                                  /* None = inherit */

    out = json_object();
    if (host_env && host_env->type == JSON_OBJECT) {
        n = json_object_size(host_env);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(host_env, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(host_env, i)));
        }
    }
    n = json_object_size(values);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(values, i);
        if (k) json_set(out, k, json_copy(json_object_get_at(values, i)));
    }
    return out;
}

json_t *w24_remotecomm_c4bdad_resolve_passthrough(const json_t *explicit_forward,
                                                  const json_t *passthrough,
                                                  const json_t *env,
                                                  const json_t *hermes_env,
                                                  const json_t *multiplex,
                                                  const json_t *internal_secret,
                                                  const json_t *blocklisted,
                                                  const json_t *global_env) {
    rc_set reg = {0}, internal = {0}, blocked = {0}, global_scope = {0}, forward = {0};
    rc_set unset = {0};
    json_t *out, *exec, *unset_arr;
    size_t i;
    int multi = rc_truthy(multiplex);

    rc_set_from_json(&reg, passthrough);
    rc_set_from_json(&internal, internal_secret);
    rc_set_from_json(&blocked, blocklisted);
    rc_set_from_json(&global_scope, global_env);

    /* implicit = passthrough - internal secrets */
    for (i = 0; i < reg.n; i++)
        if (!rc_set_has(&internal, reg.items[i])) rc_set_add(&forward, reg.items[i]);
    /* forward = explicit | (implicit - blocklisted) */
    {
        rc_set only = {0};
        for (i = 0; i < forward.n; i++)
            if (!rc_set_has(&blocked, forward.items[i])) rc_set_add(&only, forward.items[i]);
        rc_set_free(&forward);
        forward = only;
    }
    rc_set_from_json(&forward, explicit_forward);

    if (forward.n) qsort(forward.items, forward.n, sizeof(char *), rc_cmp_str);

    exec = json_object();
    for (i = 0; i < forward.n; i++) {
        const char *key = forward.items[i];
        const json_t *value = json_obj_get(env, key);
        if (!rc_truthy(value)) value = json_obj_get(hermes_env, key);   /* `or` */
        if (value && value->type != JSON_NULL) {
            json_set(exec, key, json_copy(value));
        } else if (multi && !rc_set_has(&global_scope, key) && rc_is_shell_env_name(key)) {
            rc_set_add(&unset, key);
        }
    }

    unset_arr = json_array();
    if (unset.n) qsort(unset.items, unset.n, sizeof(char *), rc_cmp_str);
    for (i = 0; i < unset.n; i++) json_append(unset_arr, json_string(unset.items[i]));

    out = json_object();
    json_set(out, "exec_env", exec);
    json_set(out, "unset", unset_arr);

    rc_set_free(&reg);
    rc_set_free(&internal);
    rc_set_free(&blocked);
    rc_set_free(&global_scope);
    rc_set_free(&forward);
    rc_set_free(&unset);
    return out;
}

json_t *w24_remotecomm_c4bdad_constants(void) {
    json_t *o = json_object();
    json_set(o, "shell_env_name_pattern", json_string("^[A-Za-z_][A-Za-z0-9_]*$"));
    json_set(o, "bash", json_string("bash"));
    json_set(o, "login_flag", json_string("-l"));
    json_set(o, "command_flag", json_string("-c"));
    return o;
}
