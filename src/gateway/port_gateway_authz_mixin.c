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

#include "hermes_gateway.h"
#include "hermes_gateway_config.h"
#include "hermes_json.h"
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
