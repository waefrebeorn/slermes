/* JSON-lines harness for the w28_webserverd_919ecc_p2 gate
 * (plugin-hub shaping + plugin-API trust gates). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "noc_w28_webserverd_919ecc_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "strip_manifest")) return w28_webserverd_919ecc_p2_strip_manifest(json_obj_get(in, "p"));
    if (!strcmp(func, "runtime_status")) return w28_webserverd_919ecc_p2_runtime_status(in);
    if (!strcmp(func, "mount_skip_reason")) return w28_webserverd_919ecc_p2_mount_skip_reason(in);
    if (!strcmp(func, "plugin_entry")) return w28_webserverd_919ecc_p2_plugin_entry(in);
    if (!strcmp(func, "safe_api_relpath")) return w28_webserverd_919ecc_p2_safe_api_relpath(in);
    if (!strcmp(func, "api_name")) return w28_webserverd_919ecc_p2_api_name(in);
    if (!strcmp(func, "api_mounted")) return w28_webserverd_919ecc_p2_api_mounted(in);
    if (!strcmp(func, "hot_name_ok")) return w28_webserverd_919ecc_p2_hot_name_ok(in);
    if (!strcmp(func, "login_command")) return w28_webserverd_919ecc_p2_login_command(in);
    if (!strcmp(func, "auth_hint")) return w28_webserverd_919ecc_p2_auth_hint(in);
    if (!strcmp(func, "mount_decision")) return w28_webserverd_919ecc_p2_mount_decision(in);
    if (!strcmp(func, "env_truthy")) return w28_webserverd_919ecc_p2_env_truthy(in);
    if (!strcmp(func, "enable_sets")) return w28_webserverd_919ecc_p2_enable_sets(in);
    if (!strcmp(func, "orphan_dashboard")) return w28_webserverd_919ecc_p2_orphan_dashboard(in);
    if (!strcmp(func, "cache_hit")) return w28_webserverd_919ecc_p2_cache_hit(in);
    if (!strcmp(func, "splice_index")) return w28_webserverd_919ecc_p2_splice_index(in);
    if (!strcmp(func, "constants")) return w28_webserverd_919ecc_p2_constants();
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[524288];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
