#ifndef NOC_W8_HANDOFFTARG_ECDF33_H
#define NOC_W8_HANDOFFTARG_ECDF33_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: handofftarg_relay_fronts @ gateway/handoff_target.py:_relay_fronts
 *
 * Platform.RELAY's enabled flag and gateway.relay.relay_platform_identities()
 * are late imports; both are fixture seams (`relay_enabled` / `relay_names`). */
int handofftarg_relay_fronts(int relay_enabled, struct json_t *relay_names,
                             const char *platform_name);

/* PoP: handofftarg_resolve_handoff_home @ gateway/handoff_target.py:resolve_handoff_home
 *
 * Returns {"ok":true,"value":{"chat_id":...,"platform":...}} or
 * {"ok":false,"reason":<HandoffTargetError.reason>,"platform":...}.
 * Platform(enum), load_gateway_config and get_home_channel are fixture seams. */
struct json_t *handofftarg_resolve_handoff_home(
    const char *platform_name, struct json_t *known_platforms,
    struct json_t *platforms, struct json_t *homes,
    int relay_enabled, struct json_t *relay_names);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_HANDOFFTARG_ECDF33_H */
