/* Differential harness for the ported Fernet implementation.
 *
 * Cases: [ {"name":..., "op":"enc", "secret":..., "plain":..., "iv":[16 ints], "ts":N} ]
 *        [ {"name":..., "op":"dec", "secret":..., "token":..., "now":N, "ttl":N} ]
 * Out:   [ {"name":..., "token":...} | {"name":..., "plain":...} | {"name":..., "error":...} ]
 */
#include "fernet.h"
#include "hermes_json.h"
#include "base64.h"

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
        const char *op = json_get_str(c, "op", "");
        const char *secret = json_get_str(c, "secret", "");
        json_t *entry = json_object();
        json_set(entry, "name", json_string(json_get_str(c, "name", "")));

        char *err = NULL;
        if (strcmp(op, "enc") == 0) {
            unsigned char iv[FERNET_IV_LEN];
            const json_t *ivj = json_obj_get(c, "iv");
            for (int k = 0; k < FERNET_IV_LEN; k++) {
                json_t *e = json_get(ivj, (size_t)k);
                iv[k] = (unsigned char)(e && e->type == JSON_NUMBER ? (int)json_number_value(e) : 0);
            }
            /* Bytes travel as base64: JSON strings cannot carry 0x80..0xFF intact. */
            size_t plen = 0;
            unsigned char *plain = base64_decode(json_get_str(c, "plain_b64", ""), &plen);
            if (!plain) { plain = (unsigned char *)calloc(1, 1); plen = 0; }
            json_t *tsj = json_obj_get(c, "ts");
            long long ts = tsj && tsj->type == JSON_NUMBER ? (long long)json_number_value(tsj) : 0;
            char *tok = fernet_encrypt(secret, plain, plen, iv, ts, &err);
            free(plain);
            if (tok) { json_set(entry, "token", json_string(tok)); free(tok); }
            else json_set(entry, "error", json_string(err ? err : "encrypt failed"));
        } else if (strcmp(op, "dec") == 0) {
            json_t *nowj = json_obj_get(c, "now");
            json_t *ttlj = json_obj_get(c, "ttl");
            long long now = nowj && nowj->type == JSON_NUMBER ? (long long)json_number_value(nowj) : 0;
            long long ttl = ttlj && ttlj->type == JSON_NUMBER ? (long long)json_number_value(ttlj) : -1;
            size_t len = 0;
            unsigned char *plain = fernet_decrypt(secret, json_get_str(c, "token", ""),
                                                  now, ttl, &len, &err);
            if (plain) {
                char *b64 = base64_encode_padded(plain, len);
                json_set(entry, "plain_b64", json_string(b64 ? b64 : ""));
                json_set(entry, "len", json_number((double)len));
                free(b64);
                free(plain);
            } else {
                json_set(entry, "error", json_string(err ? err : "invalid token"));
            }
        } else {
            json_set(entry, "error", json_string("unknown op"));
        }
        free(err);
        err = NULL;
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
