#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_backend_308e86.h"

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

static int flag(json_t *in, const char *key) { return json_get_bool(in, key, 0) ? 1 : 0; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "store_error")) {
            out = w19_backend_308e86_store_error(opt_str(in, "message"),
                                                flag(in, "is_disk_full"), flag(in, "is_lock"),
                                                flag(in, "no_more_rows"), flag(in, "fts_corrupt"),
                                                flag(in, "malformed"), opt_str(in, "classify"));
        } else if (!strcmp(func, "translate")) {
            out = w19_backend_308e86_translate((int)json_get_num(in, "exc_kind", 0),
                                              opt_str(in, "exc_name"), opt_str(in, "message"),
                                              flag(in, "is_disk_full"), flag(in, "is_lock"),
                                              flag(in, "no_more_rows"), flag(in, "fts_corrupt"),
                                              flag(in, "malformed"), opt_str(in, "classify"));
        } else if (!strcmp(func, "register_repository")) {
            out = w19_backend_308e86_register_repository(json_obj_get(in, "already_registered"),
                                                        opt_str(in, "attribute"),
                                                        json_obj_get(in, "capabilities"));
        } else if (!strcmp(func, "open_plan")) {
            out = w19_backend_308e86_open_plan(json_obj_get(in, "store_keys"),
                                              opt_str(in, "scope_key"),
                                              opt_str(in, "profile_dir"), flag(in, "closed"));
        } else if (!strcmp(func, "usable")) {
            out = json_bool(w19_backend_308e86_usable(flag(in, "has_holder"),
                                                     flag(in, "read_only"),
                                                     flag(in, "registry_owned")));
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
