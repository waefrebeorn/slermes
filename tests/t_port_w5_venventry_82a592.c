#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_venventry_82a592.h"

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
        json_t *out = json_object();
        if (!strcmp(func, "plan")) {
            json_t *ja = json_obj_get(in, "argv");
            size_t argc = (ja && ja->type == JSON_ARRAY) ? json_len(ja) : 0;
            const char **av = (const char **)calloc(argc ? argc : 1, sizeof(char *));
            for (size_t i = 0; i < argc; i++) {
                json_t *e = json_get(ja, i);
                av[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
            }
            int safe = json_get_bool(in, "safe", false) ? 1 : 0;
            const char *cwd = json_get_str(in, "cwd", "");
            w5_venventry_82a592_plan_t plan;
            int ok = w5_venventry_82a592_plan(av, argc, safe, cwd, &plan);
            free(av);
            if (ok) {
                json_set(out, "action", json_string(plan.action));
                json_set(out, "site", json_string(plan.site ? plan.site : ""));
                json_set(out, "inserted", json_bool(plan.inserted ? true : false));
                json_set(out, "insert_path", json_string(plan.insert_path ? plan.insert_path : ""));
                json_t *arr = json_array();
                for (size_t i = 0; i < plan.nargv; i++)
                    json_append(arr, json_string(plan.argv_out[i] ? plan.argv_out[i] : ""));
                json_set(out, "argv", arr);
                w5_venventry_82a592_plan_free(&plan);
            } else {
                json_set(out, "error", json_string("OOM"));
            }
        } else {
            json_set(out, "error", json_string("UNKNOWN_FUNC"));
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
