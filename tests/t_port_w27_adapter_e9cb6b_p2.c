/* JSON-lines harness for the w27_adapter_e9cb6b_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_e9cb6b_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "normalize_e2ee_mode")) {
            out = w27_adapter_normalize_e2ee_mode(json_obj_get(in, "value"));
        } else if (!strcmp(func, "resolve_e2ee_mode")) {
            out = w27_adapter_resolve_e2ee_mode(json_obj_get(in, "explicit"),
                                                json_obj_get(in, "legacy_enabled"));
        } else if (!strcmp(func, "env_truthy")) {
            out = w27_adapter_env_truthy(json_obj_get(in, "value"));
        } else if (!strcmp(func, "env_number")) {
            out = w27_adapter_env_number(json_obj_get(in, "value"), json_obj_get(in, "default"),
                                         json_get_bool(in, "want_float", false));
        } else if (!strcmp(func, "csv_set")) {
            out = w27_adapter_csv_set(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "thread_root")) {
            out = w27_adapter_thread_root(json_obj_get(in, "relates_to"));
        } else if (!strcmp(func, "extra_truthy")) {
            out = w27_adapter_extra_truthy(json_obj_get(in, "configured"));
        } else if (!strcmp(func, "configured_bool")) {
            out = w27_adapter_configured_bool(json_obj_get(in, "configured"));
        } else if (!strcmp(func, "parse_require_mention")) {
            out = w27_adapter_parse_require_mention(json_obj_get(in, "configured"));
        } else if (!strcmp(func, "parse_thread_require_mention")) {
            out = w27_adapter_parse_thread_require_mention(json_obj_get(in, "configured"));
        } else if (!strcmp(func, "extract_server_ed25519")) {
            out = w27_adapter_extract_server_ed25519(json_obj_get(in, "keys"));
        } else if (!strcmp(func, "is_connected")) {
            out = w27_adapter_is_connected(json_obj_get(in, "homeserver"),
                                           json_obj_get(in, "token"));
        } else if (!strcmp(func, "session_scope")) {
            out = w27_adapter_session_scope(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "reactions_enabled")) {
            out = w27_adapter_reactions_enabled(json_obj_get(in, "raw"));
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
