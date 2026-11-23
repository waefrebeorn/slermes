/*
 * setup_quick.c — C home for upstream Hermes hermes_cli/setup_quick.py.
 * PoP: w8_setupquick_c8d6f5_minimize_config @ hermes_cli/setup_quick.py:_blank_slate_minimize_config
 * PoP: w8_setupquick_c8d6f5_messaging_platform @ hermes_cli/setup_quick.py:_run_quick_setup
 */
#include "noc_w8_setupquick_c8d6f5.h"
#include "hermes_json.h"
#include <string.h>

static json_t *obj_default(json_t *obj, const char *key) {
    /* Python dict.setdefault: return existing value, else insert a fresh {}. */
    json_t *v = json_obj_get(obj, key);
    if (v) return v;
    v = json_object();
    json_set(obj, key, v);
    return v;
}

struct json_t *w8_setupquick_c8d6f5_minimize_config(const struct json_t *config) {
    json_t *out = config && config->type == JSON_OBJECT ? json_copy(config) : json_object();
    json_set(obj_default(out, "agent"), "max_turns", json_number(90));
    json_set(obj_default(out, "compression"), "enabled", json_bool(false));
    json_t *mem = obj_default(out, "memory");
    json_set(mem, "memory_enabled", json_bool(false));
    json_set(mem, "user_profile_enabled", json_bool(false));
    json_set(obj_default(out, "checkpoints"), "enabled", json_bool(false));
    json_set(obj_default(out, "smart_model_routing"), "enabled", json_bool(false));
    return out;
}

struct json_t *w8_setupquick_c8d6f5_messaging_platform(const char *var_name) {
    if (!var_name) return json_null();
    static const char *const needles[] = {"TELEGRAM", "DISCORD", "SLACK"};
    static const char *const labels[]   = {"Telegram", "Discord", "Slack"};
    static const char *const emojis[]   = {"\xF0\x9F\x93\xB1", "\xF0\x9F\x92\xAC", "\xF0\x9F\x92\xBC"};
    for (int i = 0; i < 3; i++) {
        if (strstr(var_name, needles[i])) {
            json_t *arr = json_array();
            json_append(arr, json_string(labels[i]));
            json_append(arr, json_string(emojis[i]));
            return arr;
        }
    }
    return json_null();
}
