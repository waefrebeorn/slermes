#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_legacyloca_18b83d.h"
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
        if (!strcmp(func,"parse")) out = w14_legacyloca_18b83d_parse_authenticated_subject(json_obj_get(in,"subject"));
        else if (!strcmp(func,"login_label")) out = w14_legacyloca_18b83d_login_label(json_obj_get(in,"subject"));
        else if (!strcmp(func,"same_owner")) out = w14_legacyloca_18b83d_same_owner(json_obj_get(in,"stored"),json_obj_get(in,"subject"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"owners_agree")) out = w14_legacyloca_18b83d_owners_agree(json_obj_get(in,"first"),json_obj_get(in,"second"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"binding_names")) out = w14_legacyloca_18b83d_binding_names(json_obj_get(in,"saved"),json_obj_get(in,"subject"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"bindings_agree")) out = w14_legacyloca_18b83d_bindings_agree(json_obj_get(in,"first"),json_obj_get(in,"second"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"client_label")) out = w14_legacyloca_18b83d_client_label(json_obj_get(in,"source"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"legacy_history_row")) out = w14_legacyloca_18b83d_legacy_history_row(json_obj_get(in,"row"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"legacy_creation_source")) out = w14_legacyloca_18b83d_legacy_creation_source(json_obj_get(in,"row"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"legacy_account_label")) out = w14_legacyloca_18b83d_legacy_account_label(json_obj_get(in,"row"));
        else if (!strcmp(func,"login_owned_by")) out = w14_legacyloca_18b83d_login_owned_by(json_obj_get(in,"row"),json_obj_get(in,"subject"),json_obj_get(in,"rule"));
        else if (!strcmp(func,"validate_legacy_row")) out = w14_legacyloca_18b83d_validate_legacy_row(json_obj_get(in,"own_profile"),json_obj_get(in,"row"),json_obj_get(in,"subject"),json_obj_get(in,"rule"));
        else out = json_string("UNKNOWN_FUNC");
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
