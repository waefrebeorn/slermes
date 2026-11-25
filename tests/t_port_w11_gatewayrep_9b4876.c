/* t_port_w11_gatewayrep_9b4876.c - harness for acp_adapter/gateway_reply.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_gatewayrep_9b4876.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? (v->str_val ? v->str_val : "") : "";
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *text = NULL;

        if (!strcmp(func, "text")) {
            text = w11_gatewayrep_9b4876_text(json_obj_get(in, "payload"));
            out = json_string(text ? text : "");
        } else if (!strcmp(func, "close_segment")) {
            out = w11_gatewayrep_9b4876_close_segment(
                json_get_str(in, "streamed", ""), json_obj_get(in, "shown"));
        } else if (!strcmp(func, "final_remainder")) {
            text = w11_gatewayrep_9b4876_final_remainder(
                json_get_str(in, "text", ""), json_get_str(in, "streamed", ""),
                json_obj_get(in, "shown"), opt_str(in, "cut"), opt_str(in, "outcome"),
                json_get_bool(in, "response_reused", false),
                json_get_bool(in, "has_receipt", false));
            out = json_string(text ? text : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(text);

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
