/* JSON-lines harness for the w33_servers_a4580e gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_servers_a4580e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "ext_or_basename"))
        return w33_servers_a4580e_ext_or_basename(json_get_str(in, "path", NULL));
    if (!strcmp(func, "language_id"))
        return w33_servers_a4580e_language_id(json_get_str(in, "path", NULL),
                                              json_get_str(in, "srv_language_id", NULL));
    if (!strcmp(func, "language_table"))
        return w33_servers_a4580e_language_table();
    if (!strcmp(func, "matches"))
        return w33_servers_a4580e_matches(json_get_str(in, "path", NULL),
                                          json_obj_get(in, "extensions"));
    if (!strcmp(func, "find_server"))
        return w33_servers_a4580e_find_server(json_get_str(in, "path", NULL),
                                             json_obj_get(in, "custom"));
    if (!strcmp(func, "registry"))
        return w33_servers_a4580e_registry();
    if (!strcmp(func, "custom_servers"))
        return w33_servers_a4580e_custom_servers(json_obj_get(in, "cfg"));
    if (!strcmp(func, "root_decision"))
        return w33_servers_a4580e_root_decision(in);
    if (!strcmp(func, "untrusted_safe"))
        return w33_servers_a4580e_untrusted_safe(json_get_str(in, "server_id", NULL));
    if (!strcmp(func, "simple_spawn"))
        return w33_servers_a4580e_simple_spawn(in);
    if (!strcmp(func, "custom_argv"))
        return w33_servers_a4580e_custom_argv(in);
    return json_string("UNKNOWN_FUNC");
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
