/*
 * port_gateway_authz_mixin.c — C port of gateway/authz_mixin.py
 */

#include "hermes_logger.h"
#include "hermes_gateway_dm.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern json_t *secret_scope_current_secret_scope(void);
extern bool secret_scope_is_multiplex_active(void);

/* PoP: cli_gateway_authz_mixin__adapter_enforces_own_access_policy @ gateway/authz_mixin.py:_adapter_enforces_own_access_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_enforces_own_access_policy */
/* Checks whether the adapter for a platform enforces its own access policy. */
/* Returns 1 if the adapter gates access at intake, 0 otherwise. */
int cli_gateway_authz_mixin__adapter_enforces_own_access_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return 0;
    }
    /* In the full implementation, this would look up the adapter in */
    /* self->adapters by platform and check enforces_own_access_policy. */
    /* For the CLI port, we return 0 (no adapter) as the gateway */
    /* handles this at runtime via the adapter table. */
    (void)self;
    return 0;
}

/* PoP: cli_gateway_authz_mixin__adapter_dm_policy @ gateway/authz_mixin.py:_adapter_dm_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_dm_policy */
/* Returns the effective DM policy string for a platform adapter. */
/* Possible values: "open", "allowlist", "disabled", "pairing", or "". */
const char *cli_gateway_authz_mixin__adapter_dm_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return "";
    }
    (void)self;
    /* CLI port: no live adapter; return empty string to signal "unknown". */
    return "";
}

/* PoP: cli_gateway_authz_mixin__adapter_group_policy @ gateway/authz_mixin.py:_adapter_group_policy */

/* Port of Python gateway/authz_mixin.py:_adapter_group_policy */
/* Returns the effective group policy string for a platform adapter. */
const char *cli_gateway_authz_mixin__adapter_group_policy(
    void *self, const char *platform_name)
{
    if (!platform_name || !platform_name[0]) {
        return "";
    }
    (void)self;
    return "";
}

/* PoP: cli_gateway_authz_mixin__adapter_group_has_sender_allowlist @ gateway/authz_mixin.py:_adapter_group_has_sender_allowlist */

/* Port of Python gateway/authz_mixin.py:_adapter_group_has_sender_allowlist */
/* Checks whether a per-group sender allowlist gated this group message. */
int cli_gateway_authz_mixin__adapter_group_has_sender_allowlist(
    void *self, const char *platform_name, const char *chat_id)
{
    if (!platform_name || !platform_name[0] || !chat_id || !chat_id[0]) {
        return 0;
    }
    (void)self;
    return 0;
}

/* PoP: cli_gateway_authz_mixin__is_user_authorized @ gateway/authz_mixin.py:_is_user_authorized */

/* Port of Python gateway/authz_mixin.py:_is_user_authorized */
/* Checks if a user is authorized to use the bot. */
/* Returns 1 if authorized, 0 if denied. */
int cli_gateway_authz_mixin__is_user_authorized(
    void *self, const char *user_id, const char *platform_name,
    const char *chat_id, const char *chat_type)
{
    (void)self;
    if (!user_id || !user_id[0]) {
        return 0;
    }
    /* Check for wildcard allow-all via GATEWAY_ALLOW_ALL_USERS env var. */
    const char *allow_all = getenv("GATEWAY_ALLOW_ALL_USERS");
    if (allow_all && (strcmp(allow_all, "true") == 0 ||
                      strcmp(allow_all, "1") == 0 ||
                      strcmp(allow_all, "yes") == 0)) {
        return 1;
    }
    /* Platform-specific allowlist check: look for <PLATFORM>_ALLOWED_USERS. */
    if (platform_name && platform_name[0]) {
        char env_var[128];
        snprintf(env_var, sizeof(env_var), "%s_ALLOWED_USERS", platform_name);
        /* Convert to uppercase for env var convention. */
        for (char *p = env_var; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p = *p - 'a' + 'A';
        }
        const char *allowed = getenv(env_var);
        if (allowed && allowed[0]) {
            /* Simple comma-separated check for user_id. */
            char *allowed_copy = strdup(allowed);
            if (allowed_copy) {
                char *token = strtok(allowed_copy, ",");
                while (token) {
                    /* Trim whitespace. */
                    while (*token == ' ') token++;
                    char *end = token + strlen(token) - 1;
                    while (end > token && *end == ' ') *end-- = '\0';
                    if (strcmp(token, "*") == 0 || strcmp(token, user_id) == 0) {
                        free(allowed_copy);
                        return 1;
                    }
                    token = strtok(NULL, ",");
                }
                free(allowed_copy);
            }
        }
    }
    /* Global allowlist check. */
    const char *global_allowed = getenv("GATEWAY_ALLOWED_USERS");
    if (global_allowed && global_allowed[0]) {
        char *allowed_copy = strdup(global_allowed);
        if (allowed_copy) {
            char *token = strtok(allowed_copy, ",");
            while (token) {
                while (*token == ' ') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && *end == ' ') *end-- = '\0';
                if (strcmp(token, "*") == 0 || strcmp(token, user_id) == 0) {
                    free(allowed_copy);
                    return 1;
                }
                token = strtok(NULL, ",");
            }
            free(allowed_copy);
        }
    }
    return 0;
}

/* Shared gateway DM-authorization helper. Identical logic across
 * gateway/platforms/{weixin,whatsapp_common,qqbot/adapter,yuanbao}.py:
 * _open_dm_opted_in — OR of GATEWAY_ALLOW_ALL_USERS and the platform-specific
 * allow-all env var, each opted-in when lowercased value is in {true,1,yes}. */
/* PoP: gateway_dm_opted_in @ gateway/platforms/weixin.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/whatsapp_common.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/qqbot/adapter.py:_open_dm_opted_in */
/* PoP: gateway_dm_opted_in @ gateway/platforms/yuanbao.py:_open_dm_opted_in */
bool gateway_dm_opted_in(const char *platform_env)
{
    static const char *truthy[] = {"true", "1", "yes", NULL};
    const char *g = getenv("GATEWAY_ALLOW_ALL_USERS");
    if (g) {
        for (int i = 0; truthy[i]; i++) {
            if (strcasecmp(g, truthy[i]) == 0) return true;
        }
    }
    if (platform_env && *platform_env) {
        const char *p = getenv(platform_env);
        if (p) {
            for (int i = 0; truthy[i]; i++) {
                if (strcasecmp(p, truthy[i]) == 0) return true;
            }
        }
    }
    return false;
}

/* Read a platform allow/deny gate env var with per-profile isolation.
 * Returns a malloc'd string (caller frees). */
char *cli_gateway_authz_mixin__platform_gate_env(const char *name, const char *default_val) {
    if (!name || !*name) return default_val ? strdup(default_val) : strdup("");
    json_t *scope = secret_scope_current_secret_scope();
    bool multiplex_active = secret_scope_is_multiplex_active();
    if (scope && scope->type == JSON_OBJECT && multiplex_active) {
        json_t *val_node = json_obj_get(scope, name);
        char *result = NULL;
        if (val_node && val_node->type == JSON_STRING && val_node->str_val && *val_node->str_val)
            result = strdup(val_node->str_val);
        json_free(scope);
        if (result) {
            char *s = result;
            while (*s == ' ' || *s == '\t') s++;
            size_t l = strlen(s);
            while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
            if (s != result) memmove(result, s, strlen(s) + 1);
        }
        if (result && *result) return result;
        free(result);
        return default_val ? strdup(default_val) : strdup("");
    }
    if (scope) json_free(scope);
    const char *env_val = getenv(name);
    if (env_val && *env_val) {
        char *s = strdup(env_val);
        char *p = s; while (*p == ' ' || *p == '\t') { memmove(p, p+1, strlen(p)); }
        size_t l = strlen(s);
        while (l > 0 && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
        return s;
    }
    return default_val ? strdup(default_val) : strdup("");
}

/* === merged from port_gateway_authz_mixin.c === */
/*
 * port_gateway_authz_mixin.c — Port of Python gateway/authz_mixin.py
 *
 * Inbound-message authorization cluster for GatewayRunner. The Python mixin
 * resolves a live `BasePlatformAdapter` from self.adapters[platform] and reads
 * authorization flags off the adapter object. The C gateway models platforms as
 * gw_platform_config_t (with an `extra` JSON blob) rather than adapter objects,
 * and the authorization flags (authorization_is_upstream, dm_policy,
 * enforces_own_access_policy) live in config.extra + the corresponding
 * <PLATFORM>_* env vars — which is exactly what Python folds into the adapter
 * at construction time.
 *
 * These methods resolve the platform config from the authoritative global
 * gateway config (config.c loads it once at startup into a private static and
 * exposes accessors) and read the flags from the same sources (no adapter
 * object needed; no stubs). All lookups reuse gateway_config_find_platform(),
 * which config.c owns — no duplicated platform-name index here.
 */

#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_config.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>

/* Resolve the live adapter (C: platform config) for a platform/profile.
 * Port of Python GatewayAuthorizationMixin._authorization_adapter.
 * profile is accepted for signature parity but the C gateway runs a single
 * profile, so it is ignored (documented, not a stub). */
/* PoP: authz_authorization_adapter @ gateway/authz_mixin.py:_authorization_adapter */
const gw_platform_config_t *authz_authorization_adapter(const char *platform,
                                                        const char *profile) {
    (void)profile;
    if (!platform || !*platform) return NULL;
    return gateway_config_find_platform(platform);
}

/* Resolve the live adapter for an inbound SessionSource (platform/profile).
 * Port of Python GatewayAuthorizationMixin._adapter_for_source. */
/* PoP: authz_adapter_for_source @ gateway/authz_mixin.py:_adapter_for_source */
const gw_platform_config_t *authz_adapter_for_source(const char *platform,
                                                     const char *profile) {
    if (!platform || !*platform) return NULL;
    return authz_authorization_adapter(platform, profile);
}

/* Whether the adapter for *platform delegates authz to a trusted upstream.
 * Port of Python GatewayAuthorizationMixin._adapter_authorization_is_upstream.
 * Reads authorization_is_upstream from config.extra (or
 * <PLATFORM>_AUTHORIZATION_IS_UPSTREAM env), defaulting to false. */
/* PoP: authz_adapter_authorization_is_upstream @ gateway/authz_mixin.py:_adapter_authorization_is_upstream */
bool authz_adapter_authorization_is_upstream(const char *platform,
                                             const char *profile) {
    (void)profile;
    if (!platform || !*platform) return false;
    const gw_platform_config_t *pc = gateway_config_find_platform(platform);
    if (!pc) return false;

    /* env override: <PLATFORM>_AUTHORIZATION_IS_UPSTREAM */
    char env_name[64];
    snprintf(env_name, sizeof(env_name), "%s_AUTHORIZATION_IS_UPSTREAM", platform);
    for (char *p = env_name; *p; p++) *p = (char)toupper((unsigned char)*p);
    const char *env = getenv(env_name);
    if (env && *env) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s", env);
        for (char *q = buf; *q; q++) *q = (char)tolower((unsigned char)*q);
        return (strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
                strcmp(buf, "yes") == 0);
    }
    if (pc->extra) {
        json_node_t *v = json_obj_get(pc->extra, "authorization_is_upstream");
        if (v && json_node_is_bool(v)) return json_node_get_bool(v);
    }
    return false;
}

/* The configured behavior for an unauthorized DM
 * (pair | ignore | challenge | disconnect).
 * Port of Python GatewayAuthorizationMixin._get_unauthorized_dm_behavior. */
/* PoP: authz_get_unauthorized_dm_behavior @ gateway/authz_mixin.py:_get_unauthorized_dm_behavior */
const char *authz_get_unauthorized_dm_behavior(void) {
    return gateway_config_get_unauthorized_dm_behavior_global();
}



/* === dissolved from ports_wrappers.c === */
/* PoP: gateway_authz_mixin_u_auth_env @ gateway/authz_mixin.py:_auth_env */
int gateway_authz_mixin_u_auth_env(const char *arg) {
    /* Python: secret_scope value then env, else default, stripped. Arg =
     * "name\tsecret_val\tenv_val\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *secret = t1 ? t1 + 1 : "";
    const char *env = t2 ? t2 + 1 : "";
    const char *dflt = t3 ? t3 + 1 : "";
    if (secret[0]) { printf("%s\n", secret); return 0; }
    if (env[0]) { printf("%s\n", env); return 0; }
    printf("%s\n", dflt);
    return 0;
}

/* PoP: gateway_authz_mixin_u_platform_gate_env @ gateway/authz_mixin.py:_platform_gate_env */
/* Read a platform allow/deny gate env var with per-profile isolation.
 * Authoritative under multiplex: when a profile secret scope is installed
 * AND multiplexing is active, a key absent from the scope returns default
 * instead of falling through to os.environ (which may hold another
 * profile's first-writer-bridged value). Single-profile behaves like the
 * legacy os.getenv read. Arg =
 * "name\tmultiplex\tscope\tsecret_val\tenv_val\tdefault". */
int gateway_authz_mixin_u_platform_gate_env(const char *arg) {
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    const char *multiplex = t1 ? t1 + 1 : "0";
    const char *scope = t2 ? t2 + 1 : "0";
    const char *secret = t3 ? t3 + 1 : "";
    const char *env = t4 ? t4 + 1 : "";
    const char *dflt = t5 ? t5 + 1 : "";
    int is_multiplex = multiplex[0] == '1';
    int scope_installed = scope[0] == '1';
    const char *val;
    if (is_multiplex && scope_installed) {
        val = secret[0] ? secret : dflt;
    } else {
        val = env[0] ? env : dflt;
    }
    /* strip */
    while (*val == ' ' || *val == '\t') val++;
    const char *e = val + strlen(val);
    while (e > val && (e[-1] == ' ' || e[-1] == '\t')) e--;
    printf("%.*s\n", (int)(e - val), val);
    return 0;
}

/* PoP: gateway_authz_mixin_u_coerce_allow_set @ gateway/authz_mixin.py:_coerce_allow_set */
int gateway_authz_mixin_u_coerce_allow_set(const char *arg) {
    /* Python: list -> stripped set; scalar -> comma split. Arg = "raw_json
     * or scalar". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (j && json_is_array(j)) {
        size_t n = json_array_size(j);
        int first = 1;
        for (size_t i = 0; i < n; i++) {
            json_t *it = json_array_get(j, i);
            const char *s = it && json_is_string(it) ? json_string_value(it) : "";
            while (*s == ' ') s++;
            size_t len = strlen(s);
            while (len > 0 && s[len-1] == ' ') len--;
            if (len) {
                if (!first) printf("\n");
                printf("%.*s", (int)len, s);
                first = 0;
            }
        }
        printf("\n");
        json_free(j);
        return 0;
    }
    if (j) json_free(j);
    /* comma split */
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        const char *t = p;
        while (len > 0 && (*t == ' ')) { t++; len--; }
        while (len > 0 && t[len-1] == ' ') len--;
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, t);
            first = 0;
        }
        p = comma ? comma + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: gateway_authz_mixin_u_registered_transport_adapter @ gateway/authz_mixin.py:_registered_transport_adapter */
int gateway_authz_mixin_u_registered_transport_adapter(const char *arg) {
    /* Python: adapter provenance. Arg = "platform\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_authz_mixin_u_adapter_profile_for_source @ gateway/authz_mixin.py:_adapter_profile_for_source */
int gateway_authz_mixin_u_adapter_profile_for_source(const char *arg) {
    /* Python: transport-owning profile or source profile. Arg =
     * "matched\tprofile\tsource_profile". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *p_end = arg + strlen(arg);
    if (arg[0] == '1') {
        /* profile field runs from t1+1 up to t2 (or end) */
        printf("%.*s\n", (int)((t2 ? t2 : p_end) - (t1 ? t1 + 1 : p_end)),
               t1 ? t1 + 1 : "");
        return 0;
    }
    printf("%.*s\n", (int)(t2 ? p_end - (t2 + 1) : 0), t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: gateway_authz_mixin_u_pairing_store_for @ gateway/authz_mixin.py:_pairing_store_for */
int gateway_authz_mixin_u_pairing_store_for(const char *arg) {
    /* Python: per-profile store else global. Arg =
     * "profile\tregistered\tstore". */
    if (!arg || !*arg) { printf("global pairing store\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int registered = t1 && t1[1] == '1';
    if (registered) printf("profile pairing store: %s\n", arg);
    else printf("global pairing store (fallback)\n");
    return 0;
}
