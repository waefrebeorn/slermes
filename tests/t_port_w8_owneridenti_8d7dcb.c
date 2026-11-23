/* Port gate harness for gateway/owner_identity.py (gate w8_owneridenti_8d7dcb). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_owneridenti_8d7dcb.h"

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
        if (!strcmp(func, "login_provider")) {
            out = owneridenti_login_provider(json_get_str(in, "provider", NULL),
                                             json_obj_get(in, "extra"));
        } else if (!strcmp(func, "principal")) {
            out = owneridenti_principal_subject(json_obj_get(in, "identity"),
                    json_get_str(in, "host", ""), json_obj_get(in, "extra"));
        } else if (!strcmp(func, "same_owner")) {
            out = owneridenti_same_owner(json_get_str(in, "stored", NULL),
                    json_get_str(in, "subject", NULL), json_obj_get(in, "rule"));
        } else if (!strcmp(func, "owners_agree")) {
            out = owneridenti_owners_agree(json_get_str(in, "first", NULL),
                    json_get_str(in, "second", NULL), json_obj_get(in, "rule"));
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
