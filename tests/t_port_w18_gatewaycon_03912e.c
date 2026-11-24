/* t_port_w18_gatewaycon_03912e.c — JSON-lines harness for gateway_connections.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_gatewaycon_03912e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return v && v->type == JSON_STRING ? v->str_val : NULL;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "revision")) {
            out = w18_gatewaycon_03912e_revision(json_obj_get(in, "doc"));
        } else if (!strcmp(func, "plain")) {
            out = w18_gatewaycon_03912e_plain(json_obj_get(in, "secret"));
        } else if (!strcmp(func, "envelope")) {
            out = w18_gatewaycon_03912e_envelope(opt_str(in, "value"));
        } else if (!strcmp(func, "token_preview")) {
            out = w18_gatewaycon_03912e_token_preview(opt_str(in, "value"));
        } else if (!strcmp(func, "kept_envelope")) {
            out = w18_gatewaycon_03912e_kept_envelope(json_obj_get(in, "token"),
                                                     json_obj_get(in, "existing"));
        } else if (!strcmp(func, "public_connection")) {
            out = w18_gatewaycon_03912e_public_connection(json_obj_get(in, "entry"));
        } else if (!strcmp(func, "public_registry")) {
            out = w18_gatewaycon_03912e_public_registry(json_obj_get(in, "reg"));
        } else if (!strcmp(func, "signed_in_ids_gone")) {
            out = w18_gatewaycon_03912e_signed_in_ids_gone(json_obj_get(in, "before"),
                                                          json_obj_get(in, "after"));
        } else if (!strcmp(func, "incoming_headers")) {
            out = w18_gatewaycon_03912e_incoming_headers(json_obj_get(in, "raw"),
                                                         json_obj_get(in, "existing"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
