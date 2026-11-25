#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_backupcron_66b2ef.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "prompt_degraded")) {
            out = json_bool(w18_backupcron_66b2ef_prompt_degraded(json_obj_get(in, "job")) ? true : false);
        } else if (!strcmp(func, "cron_jobs_list")) {
            out = w18_backupcron_66b2ef_cron_jobs_list(json_obj_get(in, "doc"));
        } else if (!strcmp(func, "snapshot_prompts_by_id")) {
            out = w18_backupcron_66b2ef_snapshot_prompts_by_id(json_obj_get(in, "doc"));
        } else if (!strcmp(func, "restore_degraded_prompts")) {
            out = w18_backupcron_66b2ef_restore_degraded_prompts(
                json_obj_get(in, "jobs"), json_obj_get(in, "snap_prompts"),
                json_get_bool(in, "apply", false) ? 1 : 0);
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
