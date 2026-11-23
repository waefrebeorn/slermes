/* t_port_w6_buildagent_361323.c — JSON-lines harness for docker/build_agent.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_buildagent_361323.h"

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
        const char *root = json_get_str(in, "root", "");
        const char *target = json_get_str(in, "target", "");
        const char *site = json_get_str(in, "site", "");
        json_t *out = NULL;

        if (!strcmp(func, "assemble_inputs")) {
            out = w6_buildagent_361323_assemble_inputs(root, target, site);
        } else if (!strcmp(func, "link_target")) {
            char *r = w6_buildagent_361323_link_target(json_get_str(in, "command", ""));
            out = json_string(r ? r : "");
            free(r);
        } else if (!strcmp(func, "assemble_image")) {
            out = w6_buildagent_361323_assemble_image_ex(root, target, site,
                                                          json_obj_get(in, "commands"));
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
