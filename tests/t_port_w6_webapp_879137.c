#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_webapp_879137.h"

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
        if (!strcmp(func, "dist_dir")) {
            const char *root = json_get_str(in, "project_root", "");
            json_t *e = json_obj_get(in, "dist_env");
            const char *env = (e && e->type == JSON_STRING) ? e->str_val : NULL;
            char *d = w6_webapp_879137_dist_dir(root, env);
            out = json_string(d ? d : "");
            w6_webapp_879137_free_str(d);
        } else if (!strcmp(func, "mark_stale")) {
            out = json_bool(w6_webapp_879137_mark_stale(
                json_get_bool(in, "packaged", 0) ? 1 : 0,
                json_get_bool(in, "index_exists", 0) ? 1 : 0) ? 1 : 0);
        } else if (!strcmp(func, "bundle_stale")) {
            out = json_bool(w6_webapp_879137_bundle_stale(
                json_get_bool(in, "marker_exists", 0) ? 1 : 0) ? 1 : 0);
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
