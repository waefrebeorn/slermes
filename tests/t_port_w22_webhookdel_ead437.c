/* JSON-lines harness for the w22_webhookdel_ead437 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_webhookdel_ead437.h"
#include "gateway/platforms/webhook_delivery.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "validate")) {
            out = w22_webhookdel_ead437_validate_destination(json_obj_get(in, "delivery"),
                                                             json_obj_get(in, "extra_platforms"));
        } else if (!strcmp(func, "snapshot")) {
            out = w22_webhookdel_ead437_snapshot_destination(json_obj_get(in, "delivery"),
                                                             json_obj_get(in, "extra_platforms"),
                                                             json_get_str(in, "home", NULL));
        } else if (!strcmp(func, "digest")) {
            char *hex = w22_webhookdel_ead437_route_digest(json_obj_get(in, "route"));
            out = hex ? json_string(hex) : json_null();
            free(hex);
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
