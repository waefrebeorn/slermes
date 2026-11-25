/* JSON-lines harness for the w30_kanbanwatc_3847d4_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_kanbanwatc_3847d4_p1.h"

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
        if (!strcmp(func, "constants")) {
            out = w30_kanbanwatc_3847d4_p1_constants();
        } else if (!strcmp(func, "diagnostic_event")) {
            out = w30_kanbanwatc_3847d4_p1_diagnostic_event(in);
        } else if (!strcmp(func, "first_line")) {
            out = w30_kanbanwatc_3847d4_p1_first_line(in);
        } else if (!strcmp(func, "clip")) {
            out = w30_kanbanwatc_3847d4_p1_clip(in);
        } else if (!strcmp(func, "safe_review_reason")) {
            out = w30_kanbanwatc_3847d4_p1_safe_review_reason(in);
        } else if (!strcmp(func, "format_event")) {
            out = w30_kanbanwatc_3847d4_p1_format_event(in);
        } else if (!strcmp(func, "format_batch")) {
            out = w30_kanbanwatc_3847d4_p1_format_batch(in);
        } else if (!strcmp(func, "ping_plan")) {
            out = w30_kanbanwatc_3847d4_p1_ping_plan(in);
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
