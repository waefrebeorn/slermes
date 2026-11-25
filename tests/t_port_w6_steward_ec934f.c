/* t_port_w6_steward_ec934f.c — JSON-lines harness for hermes_cli/steward.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_steward_ec934f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *opt_str(json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *steward = json_get_str(in, "steward", "");
        const char *platform = opt_str(in, "platform");
        const char *content = opt_str(in, "content");
        const json_t *jg = json_obj_get(in, "has_git");
        int has_git = jg && jg->type == JSON_BOOL && jg->bool_val;
        json_t *out = NULL;
        char *r = NULL;

        if (!strcmp(func, "steward_update_message")) {
            r = w6_steward_ec934f_steward_update_message(steward);
            out = json_string(r ? r : "");
        } else if (!strcmp(func, "steward_uninstall_message")) {
            r = w6_steward_ec934f_steward_uninstall_message(steward, platform);
            out = json_string(r ? r : "");
        } else if (!strcmp(func, "read_install_stamp")) {
            out = w6_steward_ec934f_read_install_stamp(content);
        } else if (!strcmp(func, "sealed_steward")) {
            r = w6_steward_ec934f_sealed_steward(has_git, content);
            out = r ? json_string(r) : json_null();
        } else if (!strcmp(func, "is_bundled_payload")) {
            out = json_bool(w6_steward_ec934f_is_bundled_payload(content));
        } else if (!strcmp(func, "classify_install")) {
            out = w6_steward_ec934f_classify_install(has_git, content);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(r);

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
