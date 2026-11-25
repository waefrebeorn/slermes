#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_webapplaun_8e9a00.h"

static void emit_key(const char *key, struct json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "older_gateway")) {
            out = w14_webapplaun_8e9a00_older_gateway(in);
        } else if (!strcmp(func, "refusal")) {
            out = w14_webapplaun_8e9a00_refusal(in);
        } else if (!strcmp(func, "plain_http_note")) {
            out = w14_webapplaun_8e9a00_plain_http_note(in);
        } else if (!strcmp(func, "signed_out_summary")) {
            out = w14_webapplaun_8e9a00_signed_out_summary(in);
        } else if (!strcmp(func, "restart_applies")) {
            out = w14_webapplaun_8e9a00_restart_applies(in);
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
