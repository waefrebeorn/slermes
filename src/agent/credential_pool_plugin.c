/*
 * credential_pool_plugin.c — faithful C11 port of agent/credential_pool_plugin.py
 * (gate w16_credential_33d75e).
 *
 * PoP: w16_credential_33d75e_is_terminal_plugin_refresh_error @ agent/credential_pool_plugin.py:is_terminal_plugin_refresh_error
 * PoP: w16_credential_33d75e_plugin_row_is_expiring @ agent/credential_pool_plugin.py:plugin_row_is_expiring
 * PoP: w16_credential_33d75e_apply_plugin_refresh_result @ agent/credential_pool_plugin.py:apply_plugin_refresh_result
 *
 * The AuthError grant-dead code set mirrors hermes_cli.auth._OAUTH_GRANT_DEAD_CODES.
 * The `now` clock is injected (Python reads time.time()); field names mirror
 * dataclasses.fields(PooledCredential) minus {"provider","extra"}.
 */
#include "noc_w16_credential_33d75e.h"

#include <stdlib.h>
#include <string.h>

#define EXPIRY_SKEW_MS 120000LL

static const char *const DEAD_CODES[] = {"invalid_grant", "invalid_token", "refresh_token_reused"};
static const size_t N_DEAD = sizeof(DEAD_CODES) / sizeof(DEAD_CODES[0]);
static const char *const FIELD_NAMES[] = {"access_token", "refresh_token", "expires_at_ms", "label", "id"};
static const size_t N_FIELDS = sizeof(FIELD_NAMES) / sizeof(FIELD_NAMES[0]);

static int is_dead_code(const char *code) {
    size_t i;
    if (!code) return 0;
    for (i = 0; i < N_DEAD; ++i) if (!strcmp(code, DEAD_CODES[i])) return 1;
    return 0;
}

static int is_field_name(const char *key) {
    size_t i;
    if (!key) return 0;
    for (i = 0; i < N_FIELDS; ++i) if (!strcmp(key, FIELD_NAMES[i])) return 1;
    return 0;
}

static int json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return 1;
}

json_t *w16_credential_33d75e_is_terminal_plugin_refresh_error(const json_t *exc) {
    const json_t *kind, *relogin, *code;
    int terminal = 0;
    if (!exc || exc->type != JSON_OBJECT) return json_bool(0);
    kind = json_obj_get(exc, "kind");
    if (!kind || kind->type != JSON_STRING || strcmp(kind->str_val, "auth")) return json_bool(0);
    relogin = json_obj_get(exc, "relogin_required");
    if (relogin && relogin->type == JSON_BOOL && relogin->bool_val) terminal = 1;
    code = json_obj_get(exc, "code");
    if (!terminal && code && code->type == JSON_STRING && is_dead_code(code->str_val)) terminal = 1;
    return json_bool(terminal);
}

json_t *w16_credential_33d75e_plugin_row_is_expiring(const char *expires_at_ms, const char *now_ms) {
    long long exp, now;
    if (!expires_at_ms || !expires_at_ms[0]) return json_bool(0);
    exp = strtoll(expires_at_ms, NULL, 10);
    now = strtoll(now_ms ? now_ms : "0", NULL, 10);
    return json_bool(exp <= now + EXPIRY_SKEW_MS);
}

/* Build a copy of `obj`, then overlay the keys of `changes` that are in whitelist.
 * extra gets merged (dict update) rather than replaced. */
static json_t *project_entry(const json_t *entry, const json_t *extra_overlay) {
    static const char *const order[] = {"provider", "access_token", "refresh_token",
                                        "expires_at_ms", "extra", "label", "id"};
    static const char *const defaults[] = {"", "", "", NULL, NULL, "", ""};
    json_t *out = json_object();
    size_t i;
    json_t *extra = json_object();

    for (i = 0; i < sizeof(order) / sizeof(order[0]); ++i) {
        const json_t *v = entry && entry->type == JSON_OBJECT ? json_obj_get(entry, order[i]) : NULL;
        if (!strcmp(order[i], "extra")) {
            if (v && v->type == JSON_OBJECT) {
                size_t j;
                for (j = 0; j < json_object_size(v); ++j)
                    json_set(extra, json_object_get_key_at(v, j), json_copy(json_object_get_at(v, j)));
            }
            if (extra_overlay && extra_overlay->type == JSON_OBJECT) {
                size_t j;
                for (j = 0; j < json_object_size(extra_overlay); ++j)
                    json_set(extra, json_object_get_key_at(extra_overlay, j),
                             json_copy(json_object_get_at(extra_overlay, j)));
            }
            json_set(out, "extra", extra); /* ownership passed */
            extra = NULL;
        } else if (!strcmp(order[i], "expires_at_ms")) {
            json_set(out, order[i], v ? json_copy(v) : json_null());
        } else {
            json_set(out, order[i], v ? json_copy(v) : json_string(defaults[i] ? defaults[i] : ""));
        }
    }
    free(extra);
    return out;
}

json_t *w16_credential_33d75e_apply_plugin_refresh_result(const json_t *entry, const json_t *result) {
    const json_t *result_obj = result;
    size_t i, n;
    json_t *field_updates = json_object();
    json_t *extra_updates = json_object();
    json_t *out;
    int any_field = 0, any_extra = 0;

    if (!result_obj || result_obj->type != JSON_OBJECT || json_object_size(result_obj) == 0) {
        json_free(field_updates); json_free(extra_updates);
        return project_entry(entry, NULL);
    }
    n = json_object_size(result_obj);
    for (i = 0; i < n; ++i) {
        const char *k = json_object_get_key_at(result_obj, i);
        const json_t *v = json_object_get_at(result_obj, i);
        if (!k) continue;
        if (is_field_name(k)) {
            json_set(field_updates, k, json_copy(v));
            any_field = 1;
        } else if (strcmp(k, "provider")) {
            json_set(extra_updates, k, json_copy(v));
            any_extra = 1;
        }
    }
    if (any_extra) {
        json_set(field_updates, "extra", extra_updates);
        extra_updates = NULL;
        any_field = 1;
    }
    if (!any_field) {
        json_free(field_updates); json_free(extra_updates);
        return project_entry(entry, NULL);
    }
    out = project_entry(entry, any_extra ? json_obj_get(field_updates, "extra") : NULL);
    /* Overlay only the real dataclass fields (extra already merged above). */
    for (i = 0; i < n; ++i) {
        const char *k = json_object_get_key_at(result_obj, i);
        const json_t *v = json_object_get_at(result_obj, i);
        if (k && is_field_name(k)) json_set(out, k, json_copy(v));
    }
    json_free(field_updates);
    json_free(extra_updates);
    return out;
}
