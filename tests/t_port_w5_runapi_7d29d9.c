/* Port gate harness for gateway/run_api.py (gate w5_runapi_7d29d9). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_runapi_7d29d9.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "public_url")) {
            out = runapi_listener_public_url(
                json_get_bool(in, "webapp_present", false) ? 1 : 0,
                json_obj_get(in, "public_url"));
        } else if (!strcmp(func, "embedded")) {
            out = runapi_embedded_chat_enabled(
                json_get_bool(in, "loaded_web", false) ? 1 : 0,
                json_get_bool(in, "has_flag", false) ? 1 : 0,
                json_get_bool(in, "flag_val", false) ? 1 : 0);
        } else if (!strcmp(func, "socket")) {
            out = runapi_socket_options(
                json_get_num(in, "port", 0),
                json_get_bool(in, "is_nt", false) ? 1 : 0,
                json_get_bool(in, "dual_stack", false) ? 1 : 0);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
