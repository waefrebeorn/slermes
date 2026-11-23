#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_webbuildl_3b4d8f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int cgroup_present(const json_t *in) {
    json_t *v = json_obj_get(in, "cgroup_mb");
    return v && v->type != JSON_NULL;
}
static long long cgroup_value(const json_t *in) {
    json_t *v = json_obj_get(in, "cgroup_mb");
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : 0;
}
static int cores_value(const json_t *in) {
    return (int)json_get_num(in, "cores", 1);
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
        int cp = cgroup_present(in);
        long long cm = cgroup_value(in);
        int cores = cores_value(in);
        if (!strcmp(func, "light")) {
            out = json_bool(w15_webbuildl_3b4d8f_light_build_requested(json_obj_get(in, "env")) ? true : false);
        } else if (!strcmp(func, "threads")) {
            out = json_number((double)w15_webbuildl_3b4d8f_bounded_threads(cores));
        } else if (!strcmp(func, "heap")) {
            out = json_number((double)w15_webbuildl_3b4d8f_bounded_heap_mb(cp, cm));
        } else if (!strcmp(func, "limits") || !strcmp(func, "apply")) {
            out = !strcmp(func, "apply")
                ? w15_webbuildl_3b4d8f_apply_web_build_limits(json_obj_get(in, "env"), cp, cm, cores)
                : w15_webbuildl_3b4d8f_web_build_limits(json_obj_get(in, "env"), cp, cm, cores);
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
