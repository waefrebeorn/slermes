/*
 * hermes_gateway_dm.h — shared gateway DM-authorization helpers.
 * Reusable mechanism implementing the per-platform "_open_dm_opted_in"
 * logic (identical across weixin/whatsapp/qqbot/yuanbao: OR of
 * GATEWAY_ALLOW_ALL_USERS and the platform-specific allow-all env var).
 */
#ifndef HERMES_GATEWAY_DM_H
#define HERMES_GATEWAY_DM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns true when DM intake is globally opted in for a platform.
 * platform_env is the platform-specific env-var name (e.g.
 * "WEIXIN_ALLOW_ALL_USERS"); it is OR'd with GATEWAY_ALLOW_ALL_USERS.
 * Both vars are treated as opted-in when their lowercased value is one of
 * "true", "1", "yes". Mirrors gateway/platforms/*._open_dm_opted_in.
 */
bool gateway_dm_opted_in(const char *platform_env);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_DM_H */
