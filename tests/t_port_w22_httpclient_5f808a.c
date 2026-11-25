/* JSON-lines harness for the w22_httpclient_5f808a gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_httpclient_5f808a.h"
#include "gateway/platforms/_http_client_limits.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "pos_int")) {
            long long def = (long long)json_get_num(in, "default", 0.0);
            long long got = w22_httpclient_5f808a_positive_env_int(
                json_get_str(in, "raw", NULL), def, json_get_bool(in, "allow_zero", false));
            out = json_number((double)got);
        } else if (!strcmp(func, "pos_float")) {
            double def = json_get_num(in, "default", 0.0);
            double got = w22_httpclient_5f808a_positive_env_float(
                json_get_str(in, "raw", NULL), def);
            out = json_number(got);
        } else if (!strcmp(func, "limits")) {
            out = w22_httpclient_5f808a_limits(json_get_str(in, "max_keepalive", NULL),
                                               json_get_str(in, "expiry", NULL),
                                               json_get_bool(in, "have_httpx", true));
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
