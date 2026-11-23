/*
 * auth.c — C home for upstream Hermes acp_adapter/auth.py.
 *
 * PoP: w5_auth_845ce1_terminal_setup_auth_method_id
 *      @ acp_adapter/auth.py:TERMINAL_SETUP_AUTH_METHOD_ID
 * PoP: w5_auth_845ce1_detect_provider @ acp_adapter/auth.py:detect_provider
 * PoP: w5_auth_845ce1_build_auth_methods @ acp_adapter/auth.py:build_auth_methods
 *
 * Runtime-provider resolution and the acp.schema constructors are seams; the
 * credential/decision logic and the exact advertised strings are ported.
 * Exported symbols are w5_auth_845ce1_ prefixed.
 */
#include "noc_w5_auth_845ce1.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *w5_auth_845ce1_terminal_setup_auth_method_id(void) {
    return "hermes-setup";
}

static const char *auth_get_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static int auth_nonblank(const char *s) {
    if (!s) return 0;
    for (const char *p = s; *p; ++p) if (!isspace((unsigned char)*p)) return 1;
    return 0;
}

static char *auth_strip_lower(const char *s) {
    if (!s) return NULL;
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) ++a;
    const char *z = a + strlen(a);
    while (z > a && isspace((unsigned char)z[-1])) --z;
    size_t n = (size_t)(z - a);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; ++i) o[i] = (char)tolower((unsigned char)a[i]);
    o[n] = '\0';
    return o;
}

char *w5_auth_845ce1_detect_provider_ex(const struct json_t *runtime, int api_key_callable,
                                        int resolve_error) {
    if (resolve_error) return NULL;  /* except Exception: pass -> None */
    const char *provider = auth_get_str(runtime, "provider");
    if (!auth_nonblank(provider)) return NULL;
    int key_ok = api_key_callable || auth_nonblank(auth_get_str(runtime, "api_key"));
    if (!key_ok) return NULL;
    return auth_strip_lower(provider);
}

struct json_t *w5_auth_845ce1_build_auth_methods_ex(const char *provider) {
    json_t *methods = json_array();
    if (provider && *provider) {
        json_t *agent = json_object();
        char name[512], desc[768];
        snprintf(name, sizeof(name), "%s runtime credentials", provider);
        snprintf(desc, sizeof(desc),
                 "Authenticate Hermes using the currently configured %s runtime credentials.", provider);
        json_set(agent, "id", json_string(provider));
        json_set(agent, "name", json_string(name));
        json_set(agent, "description", json_string(desc));
        json_append(methods, agent);
    }
    json_t *terminal = json_object();
    json_set(terminal, "id", json_string(w5_auth_845ce1_terminal_setup_auth_method_id()));
    json_set(terminal, "name", json_string("Configure Hermes provider"));
    json_set(terminal, "type", json_string("terminal"));
    json_t *args = json_array();
    json_append(args, json_string("--setup"));
    json_set(terminal, "args", args);
    json_set(terminal, "description", json_string(
        "Open Hermes' interactive model/provider setup in a terminal. "
        "Use this when Hermes has not been configured on this machine yet."));
    json_append(methods, terminal);
    return methods;
}

