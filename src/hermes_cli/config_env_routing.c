/*
 * config_env_routing.c - C home for upstream Hermes hermes_cli/config_env_routing.py.
 *
 * PoP: w17_configenv_64fb4f_is_registered_env_name @ hermes_cli/config_env_routing.py:is_registered_env_name
 * PoP: w17_configenv_64fb4f_is_env_setting_key @ hermes_cli/config_env_routing.py:is_env_setting_key
 *
 * Registry memberships (OPTIONAL_ENV_VARS / _EXTRA_ENV_KEYS / setup-hidden) are
 * injected from the fixture; the routing rule itself is the key's shape.
 */
#include "noc_w17_configenv_64fb4f.h"
#include <stdlib.h>
#include <string.h>

static int in_array(const json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < arr->c.count; ++i) {
        json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static char *upper_ascii(const char *s) {
    size_t n = strlen(s ? s : ""), i;
    char *d = (char *)malloc(n + 1);
    if (!d) return NULL;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        d[i] = (char)(c >= 'a' && c <= 'z' ? (c - 32) : c);
    }
    d[n] = '\0';
    return d;
}

json_t *w17_configenv_64fb4f_is_registered_env_name(const char *name, const json_t *registry) {
    const char *n = name ? name : "";
    if (in_array(json_obj_get(registry, "optional"), n)) return json_bool(true);
    if (in_array(json_obj_get(registry, "extra"), n)) return json_bool(true);
    if (in_array(json_obj_get(registry, "hidden"), n)) return json_bool(true);
    return json_bool(false);
}

/* UPPER_SNAKE: ^[A-Z][A-Z0-9_]*$ */
static int is_upper_snake(const char *s) {
    size_t i, n = strlen(s);
    if (n == 0 || s[0] < 'A' || s[0] > 'Z') return 0;
    for (i = 1; i < n; ++i) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

json_t *w17_configenv_64fb4f_is_env_setting_key(const char *key, const json_t *registry) {
    const char *k = key ? key : "";
    char *up;
    json_t *res;
    if (strchr(k, '.')) return json_bool(false);
    if (is_upper_snake(k)) return json_bool(true);
    up = upper_ascii(k);
    res = w17_configenv_64fb4f_is_registered_env_name(up ? up : "", registry);
    free(up);
    return res;
}
