#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_memorycall_6310dc.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jbool(json_t *v){ return (v && v->type==JSON_BOOL) ? v->bool_val : 0; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func,"refused_tool_message")) {
            tmp = w11_memorycall_6310dc_refused_tool_message(jstr(json_obj_get(in,"tool_name")));
            out = json_string(tmp);
        } else if (!strcmp(func,"bound_scope")) {
            out = w11_memorycall_6310dc_bound_scope(json_obj_get(in,"home"),
                    json_obj_get(in,"identity"), json_obj_get(in,"home_key"));
        } else if (!strcmp(func,"caller_scope")) {
            out = w11_memorycall_6310dc_caller_scope(
                    json_obj_get(in,"bound"),
                    jstr(json_obj_get(in,"caller_home")),
                    jstr(json_obj_get(in,"process_home")),
                    jbool(json_obj_get(in,"multiplexed")),
                    json_obj_get(in,"keys"));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
