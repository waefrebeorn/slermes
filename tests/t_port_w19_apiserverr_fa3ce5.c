#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_apiserverr_fa3ce5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
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
        if (!strcmp(func, "identity_bound")) {
            json_t *max_ids = json_obj_get(in, "max_identities");
            int has = (max_ids && max_ids->type == JSON_NUMBER) ? 1 : 0;
            double max_identities = has ? max_ids->num_val : 0.0;
            long long bound = w19_apiserverr_fa3ce5_identity_bound(has, max_identities,
                                                                 json_get_num(in, "max_size", 100));
            out = json_number((double)bound);
        } else if (!strcmp(func, "put_plan")) {
            out = w19_apiserverr_fa3ce5_put_plan(opt_str(in, "response_id"));
        } else if (!strcmp(func, "delete_result")) {
            out = json_bool(w19_apiserverr_fa3ce5_delete_result(
                (int)json_get_num(in, "row_deleted", 0), (int)json_get_num(in, "forgotten", 0)));
        } else if (!strcmp(func, "request_key_matches")) {
            out = json_bool(w19_apiserverr_fa3ce5_request_key_matches(
                json_get_bool(in, "has_row", 0) ? 1 : 0, opt_str(in, "stored_fp"),
                opt_str(in, "fingerprint")));
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
