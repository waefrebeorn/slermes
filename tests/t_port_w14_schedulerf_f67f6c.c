#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_schedulerf_f67f6c.h"

static void emit_key(const char *key, struct json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static struct json_t *emit_str(char *s) {
    struct json_t *out = s ? json_string(s) : json_null();
    free(s);
    return out;
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *name = json_get_str(in, "job_name", "");
        const char *jid = json_get_str(in, "job_id", "");
        const char *home = json_get_str(in, "home", "/home/u/.hermes");
        struct json_t *out = NULL;
        if (!strcmp(func, "output_dir")) {
            out = emit_str(w14_schedulerf_f67f6c_output_dir(jid, home));
        } else if (!strcmp(func, "generic")) {
            out = emit_str(w14_schedulerf_f67f6c_generic(name, jid,
                      json_get_str(in, "cleaned_error", ""), home));
        } else if (!strcmp(func, "script_timeout")) {
            out = emit_str(w14_schedulerf_f67f6c_script_timeout(name, jid, home));
        } else if (!strcmp(func, "inactivity")) {
            out = emit_str(w14_schedulerf_f67f6c_inactivity(name, jid, home));
        } else if (!strcmp(func, "blocked_config")) {
            out = emit_str(w14_schedulerf_f67f6c_blocked_config(name,
                      json_get_str(in, "reason", "")));
        } else if (!strcmp(func, "provider_notice")) {
            struct json_t *cv = json_obj_get(in, "cause");
            const char *cause = (cv && cv->type == JSON_STRING) ? cv->str_val : NULL;
            out = emit_str(w14_schedulerf_f67f6c_provider_notice(name, jid,
                      json_get_str(in, "reason", ""),
                      json_get_str(in, "backup_provider_phrase", ""),
                      cause,
                      json_get_str(in, "relogin", "")));
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
