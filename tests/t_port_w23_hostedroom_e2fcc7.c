/* JSON-lines harness for the w23_hostedroom_e2fcc7 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_hostedroom_e2fcc7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "identifier")) {
            out = w23_hostedroom_e2fcc7_identifier(in);
        } else if (!strcmp(func, "sniff_mime")) {
            out = w23_hostedroom_e2fcc7_sniff_mime(in);
        } else if (!strcmp(func, "validate_mime_kind")) {
            out = w23_hostedroom_e2fcc7_validate_mime_kind(in);
        } else if (!strcmp(func, "decode_b64")) {
            out = w23_hostedroom_e2fcc7_decode_b64(in);
        } else if (!strcmp(func, "encode_b64")) {
            out = w23_hostedroom_e2fcc7_encode_b64(in);
        } else if (!strcmp(func, "consts")) {
            out = w23_hostedroom_e2fcc7_consts();
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
