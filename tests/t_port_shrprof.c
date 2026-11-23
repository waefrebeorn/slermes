/* t_port_shrprof.c — shrprof gate harness (shared_profile_warning.py). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_shrprof.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or_null(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "shared_profile_warning")) {
            json_t *ledger = json_obj_get(in, "ledger");
            size_t n = (ledger && ledger->type == JSON_ARRAY) ? json_len(ledger) : 0;
            shrprof_ledger_entry_t *entries = NULL;
            if (n) entries = (shrprof_ledger_entry_t *)calloc(n, sizeof(*entries));
            for (size_t i = 0; i < n; i++) {
                json_t *e = json_get(ledger, i);
                if (!e || e->type != JSON_OBJECT) continue;
                json_t *iv = json_obj_get(e, "install");
                json_t *hv = json_obj_get(e, "hermes_home");
                entries[i].has_install = iv != NULL;
                entries[i].install = str_or_null(iv);
                entries[i].has_home = hv != NULL;
                entries[i].hermes_home = str_or_null(hv);
            }
            const char *r = shrprof_shared_profile_warning(
                json_get_str(in, "own_install", ""),
                json_get_str(in, "home_key", ""),
                entries, n);
            free(entries);
            out = json_string(r ? r : "");
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
