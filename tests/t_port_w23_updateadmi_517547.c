/* JSON-lines harness for the w23_updateadmi_517547 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_updateadmi_517547.h"

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
        if (!strcmp(func, "field_names")) {
            out = w23_updateadmi_517547_field_names(json_obj_get(in, "data"));
        } else if (!strcmp(func, "topic_lane")) {
            out = w23_updateadmi_517547_topic_lane(json_obj_get(in, "message"));
        } else if (!strcmp(func, "lane_keys")) {
            out = w23_updateadmi_517547_lane_keys(json_obj_get(in, "chat_id"),
                                                  json_obj_get(in, "message"),
                                                  json_obj_get(in, "adopt"));
        } else if (!strcmp(func, "update_limit")) {
            out = w23_updateadmi_517547_update_limit(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "defaults")) {
            out = w23_updateadmi_517547_defaults();
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
