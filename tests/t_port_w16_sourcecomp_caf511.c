/* JSON-lines harness for the w16_sourcecomp_caf511 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sourcecomp_caf511.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_opt(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "prepared")) {
            char *s = w16_sourcecomp_caf511_prepared_flag();
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "lock_wait")) {
            out = json_number((double)w16_sourcecomp_caf511_lock_wait_seconds());
        } else if (!strcmp(func, "acquire")) {
            out = w16_sourcecomp_caf511_acquire_waiting(
                json_get_num(in, "wait", 0.0),
                json_obj_get(in, "clocks"),
                json_obj_get(in, "acquire"),
                (int)json_get_num(in, "holder_pid", 0.0),
                str_opt(in, "holder_reason"));
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
