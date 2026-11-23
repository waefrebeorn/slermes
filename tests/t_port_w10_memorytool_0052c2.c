#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_memorytool_0052c2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jint(json_t *v){ return (int)(v && v->type==JSON_NUMBER ? v->num_val : 0); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func, "normalize_for_match")) {
            tmp = w10_memorytool_0052c2_normalize_for_match(jstr(json_obj_get(in,"text")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "span_occurrences")) {
            out = json_int(w10_memorytool_0052c2_span_occurrences(
                jstr(json_obj_get(in,"text")), jstr(json_obj_get(in,"span"))));
        } else if (!strcmp(func, "repeated_span_message")) {
            tmp = w10_memorytool_0052c2_repeated_span_message(
                jstr(json_obj_get(in,"old_text")), jint(json_obj_get(in,"count")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "over_limit")) {
            tmp = w10_memorytool_0052c2_over_limit(
                jstr(json_obj_get(in,"new_total")), jstr(json_obj_get(in,"limit")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "stale_entry_message")) {
            tmp = w10_memorytool_0052c2_stale_entry_message(jstr(json_obj_get(in,"entry")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "find_unique_match")) {
            out = w10_memorytool_0052c2_find_unique_match(
                json_obj_get(in,"entries"), jstr(json_obj_get(in,"old_text")));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
