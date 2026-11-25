/* JSON-lines harness for the w24_remotecomm_c4bdad gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_remotecomm_c4bdad.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "shlex_quote")) {
            out = w24_remotecomm_c4bdad_shlex_quote(json_obj_get(in, "s"));
        } else if (!strcmp(func, "prepend_unset")) {
            out = w24_remotecomm_c4bdad_prepend_unset(json_obj_get(in, "cmd"),
                                                      json_obj_get(in, "names"));
        } else if (!strcmp(func, "bash_argv")) {
            out = w24_remotecomm_c4bdad_bash_argv(json_obj_get(in, "cmd"),
                                                  json_obj_get(in, "login"));
        } else if (!strcmp(func, "client_env_with")) {
            out = w24_remotecomm_c4bdad_client_env_with(json_obj_get(in, "values"),
                                                        json_obj_get(in, "environ"));
        } else if (!strcmp(func, "resolve_passthrough")) {
            out = w24_remotecomm_c4bdad_resolve_passthrough(
                json_obj_get(in, "explicit_forward"), json_obj_get(in, "passthrough"),
                json_obj_get(in, "env"), json_obj_get(in, "hermes_env"),
                json_obj_get(in, "multiplex"), json_obj_get(in, "internal_secret"),
                json_obj_get(in, "blocklisted"), json_obj_get(in, "global_env"));
        } else if (!strcmp(func, "constants")) {
            out = w24_remotecomm_c4bdad_constants();
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
        json_free(in);
    }
    return 0;
}
