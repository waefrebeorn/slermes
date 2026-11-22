/* t_port_same_auth_store.c — harness for auth_gaps.c is_same_auth_store.
 * Reads {"left":<str>,"right":<str>,"samefile":0|1} (paths already resolved),
 * emits input + out. Gate: run_one_oracle.sh same_auth_store
 * oracle_same_auth_store.py "" "src/hermes_cli/auth_gaps.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_same_auth_store(const char *left_resolved, const char *right_resolved, int samefile);

static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static void emit(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (v) { char *s = json_dumps(v, 0); if (s) { fputs(s, stdout); free(s); } }
    else fputs("null", stdout);
}
int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        json_t *sf = json_obj_get(in, "samefile");
        int flag = (sf && sf->type == JSON_BOOL) ? sf->bool_val :
                   (sf && sf->type == JSON_NUMBER && sf->num_val != 0) ? 1 : 0;
        int out = is_same_auth_store(jstr(in, "left"), jstr(in, "right"), flag);
        printf("{\"left\":"); emit(in, "left");
        printf(",\"right\":"); emit(in, "right");
        printf(",\"samefile\":"); emit(in, "samefile");
        printf(",\"out\":%d}\n", out);
        json_free(in);
    }
    return 0;
}
