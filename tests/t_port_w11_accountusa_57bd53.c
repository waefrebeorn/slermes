/* t_port_w11_accountusa_57bd53.c - harness for agent/account_usage_cache.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_accountusa_57bd53.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        char *text = NULL;

        if (!strcmp(func, "key")) {
            out = w11_accountusa_57bd53_key(
                json_get_str(in, "home_key", ""), json_get_str(in, "provider", ""),
                json_get_str(in, "identity_id", ""), json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "identity")) {
            json_t *pr = json_obj_get(in, "principal");
            if (pr && pr->type == JSON_NULL) pr = NULL;
            text = w11_accountusa_57bd53_identity_id(
                json_get_str(in, "provider", ""), json_get_str(in, "runtime_api_key", ""),
                json_get_str(in, "entry_id", ""), pr);
            out = json_string(text ? text : "");
        } else if (!strcmp(func, "stale")) {
            out = json_bool(w11_accountusa_57bd53_snapshot_is_stale(
                json_get_bool(in, "valid", false), json_get_bool(in, "bad_timestamp", false),
                json_get_num(in, "fetched_at", 0.0), json_get_num(in, "now", 0.0)));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(text);

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
