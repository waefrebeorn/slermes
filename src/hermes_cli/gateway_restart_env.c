/*
 * gateway_restart_env.c — faithful C11 port of hermes_cli/gateway_restart_env.py
 * (gate w18_gatewayres_408385).
 *
 * PoP: w18_gatewayres_408385_argv_profile_home @ hermes_cli/gateway_restart_env.py:_argv_profile_home
 * PoP: w18_gatewayres_408385_launch_bot_token_names @ hermes_cli/gateway_restart_env.py:_launch_bot_token_names
 * PoP: w18_gatewayres_408385_host_watcher_env @ hermes_cli/gateway_restart_env.py:_host_gateway_watcher_env
 */
#include "noc_w18_gatewayres_408385.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static const char *sv(const json_t *v) { return v && v->type == JSON_STRING ? v->str_val : NULL; }

/* Python: str(name or "").strip() truth check on the argv profile name. */
json_t *w18_gatewayres_408385_argv_profile_home(const json_t *argv, const char *resolved_home) {
    size_t i, n = is_arr(argv) ? json_len(argv) : 0;
    for (i = 0; i < n; i++) {
        const char *part = sv(json_get(argv, i));
        const char *name = NULL;
        if (!part) continue;
        if ((!strcmp(part, "--profile") || !strcmp(part, "-p")) && i + 1 < n)
            name = sv(json_get(argv, i + 1));
        else if (!strncmp(part, "--profile=", 10))
            name = part + 10;
        if (name && *name)
            return resolved_home ? json_string(resolved_home) : json_null();
    }
    return json_null();
}

static int contains(const char *const *set, size_t n, const char *s) {
    size_t i;
    for (i = 0; i < n; i++) if (set[i] && !strcmp(set[i], s)) return 1;
    return 0;
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}

static void add_unique(char **out, size_t *n, const char *s) {
    size_t i;
    char *copy;
    for (i = 0; i < *n; i++) if (!strcmp(out[i], s)) return;
    copy = (char *)malloc(strlen(s) + 1);
    if (!copy) return;
    strcpy(copy, s);
    out[(*n)++] = copy;
}

/* {name.upper() for name in supplied if name.upper() in adapter_secrets},
 * or set(adapter_secrets) under a service manager. Sorted for stable output. */
json_t *w18_gatewayres_408385_launch_bot_token_names(const json_t *supplied,
                                                      const json_t *adapter_secrets,
                                                      int supervised) {
    size_t i, na = is_arr(adapter_secrets) ? json_len(adapter_secrets) : 0;
    size_t ns = is_arr(supplied) ? json_len(supplied) : 0;
    size_t nout = 0, cap = na + ns + 1;
    char **allowed = (char **)calloc(cap ? cap : 1, sizeof(char *));
    char **out = (char **)calloc(cap ? cap : 1, sizeof(char *));
    json_t *arr = json_array();
    if (!allowed || !out) { free(allowed); free(out); return arr; }
    for (i = 0; i < na; i++) {
        const char *s = sv(json_get(adapter_secrets, i));
        if (s) allowed[i] = (char *)s;
    }
    if (supervised) {
        for (i = 0; i < na; i++) if (allowed[i]) add_unique(out, &nout, allowed[i]);
    } else {
        for (i = 0; i < ns; i++) {
            const char *raw = sv(json_get(supplied, i));
            char *upper, *p;
            size_t k;
            if (!raw) continue;
            upper = (char *)malloc(strlen(raw) + 1);
            if (!upper) continue;
            for (p = upper, k = 0; raw[k]; k++) p[k] = (char)toupper((unsigned char)raw[k]);
            upper[k] = '\0';
            if (contains((const char *const *)allowed, na, upper)) add_unique(out, &nout, upper);
            free(upper);
        }
    }
    qsort(out, nout, sizeof(char *), cmp_str);
    for (i = 0; i < nout; i++) { json_append(arr, json_string(out[i])); free(out[i]); }
    free(out);
    free(allowed);
    return arr;
}

/* host_gateway_child_env() then env.pop("_HERMES_GATEWAY", None). */
json_t *w18_gatewayres_408385_host_watcher_env(const json_t *env) {
    json_t *out = is_obj(env) ? json_copy(env) : json_object();
    json_obj_del(out, "_HERMES_GATEWAY");
    return out;
}

