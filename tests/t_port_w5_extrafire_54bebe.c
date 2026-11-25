/* Port gate harness for cron/extra_fire.py (gate w5_extrafire_54bebe). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_extrafire_54bebe.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *job = json_obj_get(in, "job");
        json_t *out = NULL;
        if (!strcmp(func, "queued")) {
            out = extrafire_queued(job);
        } else if (!strcmp(func, "restore")) {
            out = extrafire_restore_target(job);
        } else if (!strcmp(func, "is_extra")) {
            out = extrafire_is_extra(job);
        } else if (!strcmp(func, "held")) {
            out = extrafire_held_markers(job);
        } else if (!strcmp(func, "pending")) {
            out = extrafire_pending_slot(job, json_get_str(in, "now", ""),
                                         json_get_bool(in, "manual_fire", false) ? 1 : 0);
        } else if (!strcmp(func, "stamp")) {
            out = extrafire_stamp_claim(job, json_get_str(in, "slot", ""));
        } else if (!strcmp(func, "reschedule")) {
            out = extrafire_reschedule(job, json_get_str(in, "at", ""));
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
