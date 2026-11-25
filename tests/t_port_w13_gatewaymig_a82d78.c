#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_gatewaymig_a82d78.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        int system = json_get_bool(in, "system", false) ? 1 : 0;
        const char *kind = json_get_str(in, "kind", "");
        if (!strcmp(func, "constants")) {
            out = w13_gatewaymig_a82d78_constants();
        } else if (!strcmp(func, "service_label")) {
            out = w13_gatewaymig_a82d78_service_label(kind, system);
        } else if (!strcmp(func, "remove_verb")) {
            out = w13_gatewaymig_a82d78_remove_verb(kind);
        } else if (!strcmp(func, "service_dict")) {
            out = w13_gatewaymig_a82d78_service_dict(kind, system);
        } else if (!strcmp(func, "key_names")) {
            out = w13_gatewaymig_a82d78_credential_key_names(json_get_str(in, "platform_value", ""),
                                                             json_obj_get(in, "env_keys"));
        } else if (!strcmp(func, "listener_url")) {
            out = w13_gatewaymig_a82d78_listener_url(json_get_str(in, "platform_value", ""),
                        json_get_str(in, "profile", ""), json_get_str(in, "extra_host", ""),
                        json_get_str(in, "extra_port", ""));
        } else if (!strcmp(func, "manifest_secondaries")) {
            out = w13_gatewaymig_a82d78_manifest_secondaries(json_obj_get(in, "manifest"));
        } else if (!strcmp(func, "recorded_services")) {
            out = w13_gatewaymig_a82d78_recorded_services(json_obj_get(in, "rec"));
        } else if (!strcmp(func, "recorded_user")) {
            out = w13_gatewaymig_a82d78_recorded_run_as_user(json_obj_get(in, "rec"));
        } else if (!strcmp(func, "target")) {
            out = w13_gatewaymig_a82d78_target_from_manifest(json_obj_get(in, "manifest"));
        } else if (!strcmp(func, "not_yet_served")) {
            out = w13_gatewaymig_a82d78_manifest_not_yet_served(json_obj_get(in, "manifest"),
                        json_obj_get(in, "live_served"), json_obj_get(in, "parked"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
