/*
 * models_process_fingerprint.c — C port of upstream Hermes
 * hermes_cli/models_process_fingerprint.py.
 * PoP: modfinger_process_catalog_identity @ hermes_cli/models_process_fingerprint.py:process_catalog_identity
 *
 * Dependencies (get_provider_profile, external_process_command,
 * resolve_external_process_command, os.environ, os.path.realpath) are supplied
 * by the harness/oracle through the ctx JSON object so the C side only
 * implements this module's own identity/parts logic.
 */
#include "noc_modfinger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void append_kv(json_t *parts, const char *k, const char *v) {
    size_t n = strlen(k) + strlen(v) + 2;
    char *s = (char *)malloc(n);
    if (!s) return;
    snprintf(s, n, "%s=%s", k, v);
    json_append(parts, json_string(s));
    free(s);
}

static const char *env_value(json_t *env, const char *key) {
    json_t *v = env ? json_obj_get(env, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}

json_t *modfinger_process_catalog_identity(json_t *ctx) {
    json_t *out = json_object();
    json_t *prof = ctx ? json_obj_get(ctx, "profile") : NULL;
    if (!prof || prof->type == JSON_NULL) {
        json_set(out, "profile", json_null());
        json_set(out, "parts", json_array());
        return out;
    }
    const char *auth = json_get_str(prof, "auth_type", "");
    if (strcmp(auth, "external_process")) {
        json_set(out, "profile", json_null());
        json_set(out, "parts", json_array());
        return out;
    }

    json_set(out, "profile", json_string("PROFILE"));
    json_t *parts = json_array();
    json_t *env = json_obj_get(ctx, "env");

    /* for ev in (*process_command_env_vars, process_args_env_var) if ev */
    json_t *cev = json_obj_get(prof, "process_command_env_vars");
    if (cev && cev->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(cev); i++) {
            json_t *e = json_get(cev, i);
            const char *ev = (e && e->type == JSON_STRING) ? e->str_val : "";
            if (!ev || !*ev) continue;
            append_kv(parts, ev, env_value(env, ev));
        }
    }
    const char *aev = json_get_str(prof, "process_args_env_var", "");
    if (aev && *aev) append_kv(parts, aev, env_value(env, aev));

    /* binary = resolve_external_process_command(...); realpath stubbed as identity */
    json_t *bin = json_obj_get(ctx, "binary");
    const char *b = (bin && bin->type == JSON_STRING) ? bin->str_val : "";
    append_kv(parts, "binary", b);

    json_set(out, "parts", parts);
    return out;
}
