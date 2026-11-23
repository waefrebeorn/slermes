#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_runwatchers_db91ce.h"

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
        if (!strcmp(func, "sleep_ticks")) {
            out = json_int(w7_runwatchers_db91ce_interruptible_sleep_ticks(
                (int)json_get_num(in, "seconds", 0), (int)json_get_num(in, "running_ticks", 0)));
        } else if (!strcmp(func, "prune_due")) {
            out = json_bool(w7_runwatchers_db91ce_prune_due(
                json_get_num(in, "now", 0), json_get_num(in, "last_ts", 0)) ? true : false);
        } else if (!strcmp(func, "stall_timeout")) {
            out = json_number(w7_runwatchers_db91ce_stall_timeout(json_get_str(in, "env", "")));
        } else if (!strcmp(func, "drain_releases")) {
            out = w7_runwatchers_db91ce_drain_releases(json_obj_get(in, "release_keys"));
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
