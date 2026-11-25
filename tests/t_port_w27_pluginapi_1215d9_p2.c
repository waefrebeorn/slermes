/* JSON-lines harness for the w27_pluginapi_1215d9_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_pluginapi_1215d9_p2.h"

typedef json_t *(*fn_t)(const json_t *);

static const struct {
    const char *name;
    fn_t fn;
} TABLE[] = {
    {"status_refusal", w27p2_status_refusal},
    {"status_table", w27p2_status_table},
    {"patch_status_route", w27p2_patch_status_route},
    {"open_parent_refusal", w27p2_open_parent_refusal},
    {"model_override_value", w27p2_model_override_value},
    {"reasoning_effort_value", w27p2_reasoning_effort_value},
    {"override_wanted", w27p2_override_wanted},
    {"drag_route", w27p2_drag_route},
    {"bulk_ids", w27p2_bulk_ids},
    {"comment_body_gate", w27p2_comment_body_gate},
    {"review_assignee_deferred", w27p2_review_assignee_deferred},
};

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
        json_t *in, *out = NULL;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
            if (!strcmp(func, TABLE[i].name)) { out = TABLE[i].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (i = 0; i < json_object_size(in); i++) {
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
