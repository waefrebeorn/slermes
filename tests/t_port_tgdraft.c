#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_tgdraft.h"
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
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
        if (!strcmp(func, "claim_ai_action")) {
            double now = json_get_num(in, "now", 0.0);
            json_t *arr = json_obj_get(in, "calls");
            size_t n = (arr && arr->type == JSON_ARRAY) ? json_len(arr) : 0;
            double *calls = n ? (double *)malloc(n * sizeof(double)) : NULL;
            for (size_t i = 0; i < n; i++) { json_t *v = json_get(arr, i); calls[i] = (v && v->type == JSON_NUMBER) ? v->num_val : 0.0; }
            json_t *newcalls = NULL;
            double delay = tgdraft_claim_ai_action_ex(now, calls, n, &newcalls);
            out = json_object();
            json_set(out, "delay", json_number(delay));
            json_set(out, "calls", newcalls ? newcalls : json_array());
            free(calls);
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
