/*
 * profile_model_seed.c — C11 port of upstream Hermes hermes_cli/profile_model_seed.py.
 *
 * PoP: w17_profilemod_d242f9_requested @ hermes_cli/profile_model_seed.py:_requested_provider
 * PoP: w17_profilemod_d242f9_aliases   @ hermes_cli/profile_model_seed.py:_aliases
 * PoP: w17_profilemod_d242f9_merge     @ hermes_cli/profile_model_seed.py:_merge_endpoint_seed
 *
 * The oracle stubs _expand_env_vars (identity) and custom_provider_aliases
 * ({name, provider_key} minus empties), so this port matches those seams exactly.
 * No filesystem, env mutation or credential access is performed.
 */
#include "noc_w17_profilemod_d242f9.h"
#include "hermes_json.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char *nonempty_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) ? v->str_val : NULL;
}

struct json_t *w17_profilemod_d242f9_requested(const struct json_t *model_cfg) {
    if (!model_cfg || model_cfg->type != JSON_OBJECT) return json_null();
    const json_t *p = json_obj_get(model_cfg, "provider");
    if (!p || p->type != JSON_STRING) return json_null();
    return json_string(p->str_val ? p->str_val : "");
}

/* Build the alias set for an entry (stubbed custom_provider_aliases). */
static void aliases_into(const json_t *entry, const char *key, json_t *out) {
    if (!entry || entry->type != JSON_OBJECT) return;
    const char *k = (key && *key) ? key : nonempty_str(json_obj_get(entry, "provider_key"));
    if (!k) k = "";
    const char *name = nonempty_str(json_obj_get(entry, "name"));
    if (!name) name = k;
    if (*name) json_append(out, json_string(name));
    if (*k && strcmp(k, name)) json_append(out, json_string(k));
}

struct json_t *w17_profilemod_d242f9_aliases(const struct json_t *entry, const char *key) {
    json_t *out = json_array();
    aliases_into(entry, key ? key : "", out);
    size_t n = json_len(out);
    json_t *sorted = json_array();
    bool used[8] = {0};
    for (size_t picked = 0; picked < n; picked++) {
        const char *best = NULL;
        for (size_t i = 0; i < n; i++) {
            if (used[i]) continue;
            const char *s = json_string_value(json_get(out, i));
            if (!best || strcmp(s, best) < 0) best = s;
        }
        for (size_t i = 0; i < n; i++) {
            if (used[i]) continue;
            const char *s = json_string_value(json_get(out, i));
            if (s && !strcmp(s, best)) { used[i] = true; break; }
        }
        json_append(sorted, json_string(best ? best : ""));
    }
    json_free(out);
    return sorted;
}

static int claimed_intersects(const json_t *claimed, const json_t *entry, const char *key) {
    if (!claimed || claimed->type != JSON_ARRAY || json_len(claimed) == 0) return 0;
    json_t *aliases = json_array();
    aliases_into(entry, key ? key : "", aliases);
    int hit = 0;
    for (size_t i = 0; i < json_len(aliases) && !hit; i++) {
        const char *a = json_string_value(json_get(aliases, i));
        for (size_t j = 0; j < json_len(claimed); j++) {
            const char *c = json_string_value(json_get(claimed, j));
            if (a && c && !strcmp(a, c)) { hit = 1; break; }
        }
    }
    json_free(aliases);
    return hit;
}

struct json_t *w17_profilemod_d242f9_merge(const struct json_t *dst,
                                           const struct json_t *seed,
                                           const struct json_t *claimed) {
    json_t *merged = json_object();
    if (dst && dst->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(dst); i++) {
            const char *k = json_object_get_key_at(dst, i);
            if (k) json_set(merged, k, json_copy(json_object_get_at(dst, i)));
        }
    }

    const json_t *seed_prov = (seed && seed->type == JSON_OBJECT) ? json_obj_get(seed, "providers") : NULL;
    int have_seed_prov = (seed_prov && seed_prov->type == JSON_OBJECT && json_object_size(seed_prov) > 0);
    const json_t *seed_custom = (seed && seed->type == JSON_OBJECT) ? json_obj_get(seed, "custom_providers") : NULL;
    int have_seed_custom = (seed_custom && seed_custom->type == JSON_ARRAY && json_len(seed_custom) > 0);

    const json_t *dst_prov = (dst && dst->type == JSON_OBJECT) ? json_obj_get(dst, "providers") : NULL;
    if ((dst_prov && dst_prov->type == JSON_OBJECT) || have_seed_prov) {
        json_t *providers = json_object();
        if (have_seed_prov) {
            for (size_t i = 0; i < json_object_size(seed_prov); i++) {
                const char *k = json_object_get_key_at(seed_prov, i);
                if (k) json_set(providers, k, json_copy(json_object_get_at(seed_prov, i)));
            }
        }
        if (dst_prov && dst_prov->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(dst_prov); i++) {
                const char *k = json_object_get_key_at(dst_prov, i);
                if (!k) continue;
                json_t *v = json_object_get_at(dst_prov, i);
                if (have_seed_prov && json_obj_get(seed_prov, k)) continue;
                if (claimed_intersects(claimed, v, k)) continue;
                json_set(providers, k, json_copy(v));
            }
        }
        json_set(merged, "providers", providers);
    }

    const json_t *dst_custom = (dst && dst->type == JSON_OBJECT) ? json_obj_get(dst, "custom_providers") : NULL;
    if ((dst_custom && dst_custom->type == JSON_ARRAY) || have_seed_custom) {
        json_t *custom = json_array();
        if (have_seed_custom) {
            for (size_t i = 0; i < json_len(seed_custom); i++)
                json_append(custom, json_copy(json_get(seed_custom, i)));
        }
        if (dst_custom && dst_custom->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(dst_custom); i++) {
                json_t *e = json_get(dst_custom, i);
                if (claimed_intersects(claimed, e, "")) continue;
                json_append(custom, json_copy(e));
            }
        }
        json_set(merged, "custom_providers", custom);
    }
    return merged;
}
