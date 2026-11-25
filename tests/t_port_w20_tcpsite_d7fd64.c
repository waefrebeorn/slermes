/* JSON-lines harness for the w20_tcpsite_d7fd64 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_tcpsite_d7fd64.h"
#include "gateway/platforms/tcp_site.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "start")) {
            json_t *hostv = json_obj_get(in, "host");
            out = w20_tcpsite_d7fd64_start(json_get_str(in, "platform", "linux"),
                                           (hostv && hostv->type == JSON_STRING) ? hostv->str_val : NULL,
                                           (int)json_get_num(in, "port", 0),
                                           json_get_str(in, "log_tag", ""),
                                           json_get_str(in, "first", "ok"),
                                           w20_tcpsite_d7fd64_has_live_listener(
                                               json_get_str(in, "probe", "refused")),
                                           json_get_str(in, "second", "ok"));
        } else if (!strcmp(func, "has_live_listener")) {
            out = json_bool(w20_tcpsite_d7fd64_has_live_listener(
                json_get_str(in, "probe", "ok")) ? 1 : 0);
        } else if (!strcmp(func, "is_wildcard_host")) {
            json_t *hostv = json_obj_get(in, "host");
            out = json_bool(w20_tcpsite_d7fd64_is_wildcard_host(
                (hostv && hostv->type == JSON_STRING) ? hostv->str_val : NULL) ? 1 : 0);
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
