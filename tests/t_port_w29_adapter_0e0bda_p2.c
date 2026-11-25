/* JSON-lines harness for the w29_adapter_0e0bda_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_0e0bda_p2.h"

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
        if (!strcmp(func, "base_url")) {
            out = w29_adapter_0e0bda_base_url(json_obj_get(in, "host"), json_obj_get(in, "port"),
                                              json_obj_get(in, "public_url"));
        } else if (!strcmp(func, "route_for_path")) {
            out = w29_adapter_0e0bda_route_for_path(json_obj_get(in, "agents"), json_obj_get(in, "path"));
        } else if (!strcmp(func, "route_for_request")) {
            out = w29_adapter_0e0bda_route_for_request(json_obj_get(in, "agents"), json_obj_get(in, "path"),
                                                       json_obj_get(in, "params"));
        } else if (!strcmp(func, "served_agent_summary")) {
            out = w29_adapter_0e0bda_served_agent_summary(json_obj_get(in, "agents"), json_obj_get(in, "host"),
                                                          json_obj_get(in, "port"), json_obj_get(in, "public_url"));
        } else if (!strcmp(func, "scope_for_agent")) {
            out = w29_adapter_0e0bda_scope_for_agent(json_obj_get(in, "agents"), json_obj_get(in, "agent_slug"));
        } else if (!strcmp(func, "build_card")) {
            out = w29_adapter_0e0bda_build_card(json_obj_get(in, "agents"), json_obj_get(in, "agent_slug"),
                                                json_obj_get(in, "host"), json_obj_get(in, "port"),
                                                json_obj_get(in, "public_url"), json_obj_get(in, "agent_name"),
                                                json_obj_get(in, "localhost_only"),
                                                json_obj_get(in, "provider_org"), json_obj_get(in, "provider_url"));
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
