/*
 * handoff_target.c — C port of upstream Hermes gateway/handoff_target.py.
 *
 * PoP: handofftarg_relay_fronts @ gateway/handoff_target.py:_relay_fronts
 * PoP: handofftarg_resolve_handoff_home @ gateway/handoff_target.py:resolve_handoff_home
 *
 * The config, Platform enum and relay identity list resolved by the live
 * module at import/call time are injected from the fixture exactly as the
 * oracle registers them in sys.modules.
 */
#include "noc_w8_handofftarg_ecfd33.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>

static int arr_has_str(json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}
static int truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return json_len(v) > 0 || json_object_size(v) > 0;
    return 0;
}
static json_t *err(const char *reason, const char *platform) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "reason", json_string(reason));
    json_set(o, "platform", json_string(platform ? platform : ""));
    return o;
}
static json_t *ok_home(const char *chat_id, const char *platform) {
    json_t *v = json_object();
    json_set(v, "chat_id", json_string(chat_id ? chat_id : ""));
    json_set(v, "platform", json_string(platform ? platform : ""));
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", v);
    return o;
}

int handofftarg_relay_fronts(int relay_enabled, json_t *relay_names,
                             const char *platform_name) {
    return relay_enabled && arr_has_str(relay_names, platform_name);
}

json_t *handofftarg_resolve_handoff_home(
    const char *platform_name, json_t *known_platforms,
    json_t *platforms, json_t *homes,
    int relay_enabled, json_t *relay_names) {
    if (!platform_name) platform_name = "";
    /* Platform(platform_name): ValueError -> HandoffTargetError('unknown_platform', ...) */
    if (!arr_has_str(known_platforms, platform_name))
        return err("unknown_platform", platform_name);

    json_t *native = platforms ? json_obj_get(platforms, platform_name) : NULL;
    int native_enabled = native && native->type == JSON_OBJECT
        ? truthy(json_obj_get(native, "enabled")) : 0;
    int relayed = handofftarg_relay_fronts(relay_enabled, relay_names, platform_name);
    if (!native_enabled && !relayed)
        return err("platform_not_enabled", platform_name);

    json_t *home = homes ? json_obj_get(homes, platform_name) : NULL;
    const char *chat_id = (home && home->type == JSON_OBJECT)
        ? json_get_str(home, "chat_id", NULL) : NULL;
    if (!home || home->type != JSON_OBJECT || !chat_id || !chat_id[0])
        return err("no_home_channel", platform_name);
    return ok_home(chat_id, platform_name);
}
