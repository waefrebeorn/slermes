/* Differential harness for the ported vault_store helpers.
 *
 * Cases: [ {"name":..., "op":"kinds"|"idtypes"|"origin"|"otp"|"scrub", "value":..., "secret": {...}} ]
 * Out:   [ {"name":..., "result": ...} ]
 */
#include "hermes_json.h"
#include "noc_w6_vaultstore_6d2c23.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]); return 2; }
    char *text = read_all(argv[1]);
    if (!text) return 2;
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) return 2;

    json_t *out = json_array();
    for (size_t i = 0; i < json_len(cases); i++) {
        const json_t *c = json_get(cases, i);
        const char *name = json_get_str(c, "name", "");
        const char *op = json_get_str(c, "op", "");
        const char *value = json_get_str(c, "value", "");
        const json_t *secret = json_obj_get(c, "secret");

        json_t *res = NULL;
        if (strcmp(op, "kinds") == 0) res = w6_vaultstore_6d2c23_vault_kinds();
        else if (strcmp(op, "idtypes") == 0) res = w6_vaultstore_6d2c23_login_identifier_types();
        else if (strcmp(op, "origin") == 0) {
            char *v = w6_vaultstore_6d2c23_normalize_origin(value);
            res = v ? json_string(v) : json_null();      /* the port may refuse an input */
            free(v);
        } else if (strcmp(op, "otp") == 0) {
            char *v = w6_vaultstore_6d2c23_normalize_otp_secret(value);
            res = v ? json_string(v) : json_null();
            free(v);
        }
        else if (strcmp(op, "fields") == 0) res = w6_vaultstore_6d2c23_fields(value);
        else if (strcmp(op, "required") == 0) res = w6_vaultstore_6d2c23_required_fields(value);
        else if (strcmp(op, "scrub") == 0) {
            char *s = w6_vaultstore_6d2c23_scrub_secret_from_text(value, secret);
            res = s ? json_string(s) : json_null();
            free(s);
        }

        json_t *entry = json_object();
        json_set(entry, "name", json_string(name ? name : ""));
        json_set(entry, "result", res ? res : json_null());   /* ownership moves into entry */
        json_append(out, entry);
    }
    char *ser = json_serialize(out);
    json_free(out);
    json_free(cases);
    if (!ser) return 2;
    FILE *f = fopen(argv[2], "wb");
    if (!f) { free(ser); return 2; }
    fputs(ser, f);
    fclose(f);
    free(ser);
    return 0;
}
