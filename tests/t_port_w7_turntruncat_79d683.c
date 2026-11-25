/* t_port_w7_turntruncat_79d683.c — differential harness for agent/turn_truncation.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_turntruncat_79d683.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Derive decimal string + presence from an input field (None vs 0 matters). */
static const char *num_field(const json_t *in, const char *key, bool *has) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) { *has = false; return NULL; }
    *has = true;
    return v->type == JSON_STRING ? v->str_val : "";
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

        if (!strcmp(func, "repetition_copy")) {
            out = w7_turntruncat_repetition_copy(
                json_get_str(in, "stopping", ""),
                json_get_str(in, "outcome", ""),
                json_get_str(in, "refusal", ""));
        } else if (!strcmp(func, "partial_result")) {
            out = w7_turntruncat_partial_result(
                json_get_str(in, "final_response", ""),
                json_get_str(in, "api_calls", "0"),
                json_get_bool(in, "has_error", false),
                json_get_str(in, "error", ""),
                json_get_bool(in, "failed", false),
                json_get_bool(in, "compression_exhausted", false));
        } else if (!strcmp(func, "boosted_output_cap")) {
            bool hreq = false, hlim = false, hbase = false;
            const char *req = num_field(in, "requested_cap", &hreq);
            const char *lim = num_field(in, "model_limit", &hlim);
            const char *bas = num_field(in, "base", &hbase);
            long long r = w7_turntruncat_boosted_output_cap(
                req ? req : "", hreq,
                json_get_str(in, "n", "1"),
                json_get_str(in, "max_tokens", "0"),
                json_get_str(in, "api_mode", "chat_completions"),
                hlim, lim ? lim : "", hbase, bas ? bas : "");
            out = json_number((double)r);
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
