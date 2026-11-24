/* JSON-lines harness for the w29_status_6b0cd0_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_status_6b0cd0_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_key")) {
            out = w29_status_6b0cd0_is_key(json_obj_get(in, "key"));
        } else if (!strcmp(func, "key_allowed")) {
            out = w29_status_6b0cd0_key_allowed(json_obj_get(in, "key"),
                                                json_obj_get(in, "configured"));
        } else if (!strcmp(func, "public_entry")) {
            out = w29_status_6b0cd0_public_entry(json_obj_get(in, "value"));
        } else if (!strcmp(func, "merge")) {
            out = w29_status_6b0cd0_merge_platforms(json_obj_get(in, "gateway_platforms"),
                                                    json_obj_get(in, "profile_platforms"),
                                                    json_obj_get(in, "active"));
        } else if (!strcmp(func, "project")) {
            out = w29_status_6b0cd0_project_platforms(json_obj_get(in, "gateway_platforms"),
                                                       json_obj_get(in, "configured"),
                                                       json_obj_get(in, "gateway_running"),
                                                       json_obj_get(in, "gateway_state"));
        } else if (!strcmp(func, "gateway_view")) {
            out = w29_status_6b0cd0_gateway_view(in);
        } else if (!strcmp(func, "component_health")) {
            out = w29_status_6b0cd0_component_health(in);
        } else if (!strcmp(func, "overall")) {
            out = w29_status_6b0cd0_overall(json_obj_get(in, "components"));
        } else if (!strcmp(func, "fts_rebuild")) {
            out = w29_status_6b0cd0_fts_rebuild(in);
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
