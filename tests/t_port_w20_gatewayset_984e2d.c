/* JSON-lines harness for the w20_gatewayset_984e2d gate.
 * One request object per stdin line; one response per line with "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_gatewayset_984e2d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        w20_gatewayset_984e2d_env e;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");

        memset(&e, 0, sizeof(e));
        e.config = json_obj_get(in, "config");
        e.stored = json_obj_get(in, "stored");
        e.gw = json_obj_get(in, "gw");
        e.runtime = json_obj_get(in, "runtime");
        e.interactive = json_get_bool(in, "interactive", 0) ? 1 : 0;
        e.install = json_get_bool(in, "install", 0) ? 1 : 0;
        e.isatty = json_get_bool(in, "isatty", 0) ? 1 : 0;
        e.prompt_answer = json_get_bool(in, "prompt_answer", 0) ? 1 : 0;
        e.start_now_answer = json_get_bool(in, "start_now_answer", 0) ? 1 : 0;

        if (!strcmp(func, "record_service_choice")) {
            out = w20_gatewayset_984e2d_record_service_choice(
                json_get_str(in, "choice", ""), &e);
        } else if (!strcmp(func, "wants_service_install")) {
            out = w20_gatewayset_984e2d_wants_service_install(&e);
        } else if (!strcmp(func, "ensure_gateway_service")) {
            out = w20_gatewayset_984e2d_ensure_gateway_service(
                json_get_str(in, "context", "setup"), &e);
        } else if (!strcmp(func, "wizard_install_service")) {
            out = w20_gatewayset_984e2d_wizard_install_service(
                json_get_str(in, "backend", "windows"), &e);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
