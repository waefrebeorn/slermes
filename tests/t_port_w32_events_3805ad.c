/* JSON-lines harness for the w32_events_3805ad gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_events_3805ad.h"

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
        if (!strcmp(func, "catalog")) {
            out = w32_events_3805ad_catalog();
        } else if (!strcmp(func, "schema_all")) {
            out = w32_events_3805ad_schema_all();
        } else if (!strcmp(func, "schema")) {
            out = w32_events_3805ad_schema(json_obj_get(in, "event"));
        } else if (!strcmp(func, "problems")) {
            out = w32_events_3805ad_problems(json_obj_get(in, "event"), json_obj_get(in, "payload"));
        } else if (!strcmp(func, "enums")) {
            out = w32_events_3805ad_enums();
        } else if (!strcmp(func, "select")) {
            out = w32_events_3805ad_select(json_obj_get(in, "filter"));
        } else if (!strcmp(func, "frames")) {
            out = w32_events_3805ad_frames(json_obj_get(in, "frames"));
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
