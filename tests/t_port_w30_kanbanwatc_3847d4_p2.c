/* JSON-lines harness for the w30_kanbanwatc_3847d4_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_kanbanwatc_3847d4_p2.h"

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
        if (!strcmp(func, "platform_names")) {
            out = w30_kanbanwatc_3847d4_p2_platform_names(in);
        } else if (!strcmp(func, "collect_scope")) {
            out = w30_kanbanwatc_3847d4_p2_collect_scope(in);
        } else if (!strcmp(func, "chat_type_of")) {
            out = w30_kanbanwatc_3847d4_p2_chat_type_of(in);
        } else if (!strcmp(func, "wake_scope_id")) {
            out = w30_kanbanwatc_3847d4_p2_wake_scope_id(in);
        } else if (!strcmp(func, "anchorless_warn")) {
            out = w30_kanbanwatc_3847d4_p2_anchorless_warn(in);
        } else if (!strcmp(func, "unroutable_warn")) {
            out = w30_kanbanwatc_3847d4_p2_unroutable_warn(in);
        } else if (!strcmp(func, "resolve_adapter")) {
            out = w30_kanbanwatc_3847d4_p2_resolve_adapter(in);
        } else if (!strcmp(func, "wake_plan")) {
            out = w30_kanbanwatc_3847d4_p2_wake_plan(in);
        } else if (!strcmp(func, "failure_step")) {
            out = w30_kanbanwatc_3847d4_p2_failure_step(in);
        } else if (!strcmp(func, "after_delivery")) {
            out = w30_kanbanwatc_3847d4_p2_after_delivery(in);
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
