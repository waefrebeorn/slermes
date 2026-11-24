/* JSON-lines harness for the w21_adapterrea_e0609e gate
 * (plugins/platforms/slack/adapter_reactions_context.py).
 *
 * One request object per line: {"func":"marker","msg":{...}} /
 * {"func":"name","value":...} / {"func":"limit"} / {"func":"more","parts":N}.
 * The response carries "out" plus every input key echoed back.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_adapterrea_e0609e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "marker")) {
            out = w21_adapterrea_e0609e_slack_reactions_marker(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "name")) {
            out = w21_adapterrea_e0609e_sanitize_name(json_obj_get(in, "value"));
        } else if (!strcmp(func, "limit")) {
            out = json_number((double)w21_adapterrea_e0609e_marker_max());
        } else if (!strcmp(func, "more")) {
            json_t *parts = json_obj_get(in, "parts");
            out = w21_adapterrea_e0609e_more_label(parts ? (int)parts->num_val : 0);
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
