/* Port gate harness for hermes_cli/prune_advisor.py (gate w10_pruneadvis_d472b3). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_pruneadvis_d472b3.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "low")) {
            out = w10_pruneadvis_d472b3_low(json_obj_get(in,"db_bytes"), json_obj_get(in,"free_bytes"), json_obj_get(in,"total_bytes"));
        } else if (!strcmp(func, "retention_days")) {
            out = w10_pruneadvis_d472b3_retention_days(json_obj_get(in,"sessions"));
        } else if (!strcmp(func, "gb")) {
            out = w10_pruneadvis_d472b3_gb(json_obj_get(in,"n"));
        } else if (!strcmp(func, "sessions")) {
            out = w10_pruneadvis_d472b3_sessions(json_obj_get(in,"n"));
        } else if (!strcmp(func, "helps")) {
            out = w10_pruneadvis_d472b3_helps(json_obj_get(in,"db_bytes"), json_obj_get(in,"free_bytes"),
                     json_obj_get(in,"total_bytes"), json_obj_get(in,"reclaim_bytes"), json_obj_get(in,"prunable_sessions"));
        } else if (!strcmp(func, "advice_lines")) {
            out = w10_pruneadvis_d472b3_advice_lines(json_obj_get(in,"advice"), json_get_str(in,"command_prefix","/"));
        } else if (!strcmp(func, "notice_payload")) {
            out = w10_pruneadvis_d472b3_notice_payload(json_obj_get(in,"advice"));
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
