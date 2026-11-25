#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_cuabackend_0e9d63.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_json(char *owned) {
    json_t *v = owned ? json_string(owned) : json_null();
    free(owned);
    return v;
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
        if (!strcmp(func, "resolve_app_path")) {
            out = str_json(w19_cuabackend_0e9d63_resolve_app_path(json_get_str(in, "realpath", ""),
                                                                  json_get_bool(in, "isfile", false),
                                                                  json_get_bool(in, "x_ok", false)));
        } else if (!strcmp(func, "validate_signature")) {
            out = w19_cuabackend_0e9d63_validate_signature(json_get_str(in, "app_path", ""),
                                                           json_get_bool(in, "have_codesign", true),
                                                           (int)json_get_num(in, "returncode", 0),
                                                           json_get_str(in, "stderr", ""),
                                                           json_get_bool(in, "allow_unsigned", false));
        } else if (!strcmp(func, "spawn_command")) {
            out = w19_cuabackend_0e9d63_spawn_command(json_get_str(in, "driver_cmd", ""),
                                                      json_obj_get(in, "serve_args"),
                                                      json_get_str(in, "platform", ""),
                                                      json_get_str(in, "app_path", NULL),
                                                      json_get_str(in, "realpath", ""),
                                                      json_get_bool(in, "isfile", false),
                                                      json_get_bool(in, "x_ok", false),
                                                      json_get_bool(in, "have_codesign", true),
                                                      (int)json_get_num(in, "returncode", 0),
                                                      json_get_str(in, "stderr", ""),
                                                      json_get_bool(in, "allow_unsigned", false));
        } else if (!strcmp(func, "daemon_init")) {
            out = w19_cuabackend_0e9d63_daemon_init(json_get_str(in, "driver_cmd", ""),
                                                    json_get_str(in, "permission_mode", ""),
                                                    json_get_str(in, "capability_manifest", ""),
                                                    json_get_str(in, "manifest_abspath", ""),
                                                    json_get_bool(in, "manifest_exists", false),
                                                    json_get_bool(in, "mode_independent", false),
                                                    json_get_str(in, "platform", ""),
                                                    json_get_str(in, "token", ""),
                                                    json_get_str(in, "tmpdir", "/tmp"));
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
