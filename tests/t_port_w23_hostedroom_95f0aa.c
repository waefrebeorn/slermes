/* JSON-lines harness for the w23_hostedroom_95f0aa gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_hostedroom_95f0aa.h"

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
        json_t *in, *out;
        const char *func;
        const json_t *req;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        req = json_obj_get(in, "req");
        if (!strcmp(func, "constants")) {
            out = w23_hostedroom_95f0aa_constants();
        } else if (!strcmp(func, "grant_digest")) {
            char *s = w23_hostedroom_95f0aa_grant_digest(
                          req && req->type == JSON_OBJECT ? json_get_str(req, "grant", NULL) : NULL);
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "hard_expiry")) {
            out = w23_hostedroom_95f0aa_hard_expiry(req);
        } else if (!strcmp(func, "task_identifier")) {
            out = w23_hostedroom_95f0aa_task_identifier(req);
        } else if (!strcmp(func, "task_identity")) {
            out = w23_hostedroom_95f0aa_task_identity(req);
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
