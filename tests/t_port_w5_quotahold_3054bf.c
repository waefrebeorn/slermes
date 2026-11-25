/* Port gate harness for cron/quota_hold.py (gate w5_quotahold_3054bf). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_quotahold_3054bf.h"

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
        if (!strcmp(func, "hold_seconds")) {
            out = quotahold_hold_seconds(json_obj_get(in, "chain"));
        } else if (!strcmp(func, "hold_active")) {
            out = quotahold_hold_active(job, json_get_str(in, "now", ""));
        } else if (!strcmp(func, "clear")) {
            out = quotahold_clear_state(job);
        } else if (!strcmp(func, "is_recovery")) {
            out = quotahold_is_recovery_fire(job, json_get_str(in, "next_run", ""));
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
