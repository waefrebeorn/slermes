/* t_port_w19_contextcom_da88c1.c - harness for src/agent/context_compressor_telemetry.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_contextcom_da88c1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* The estimator seam: fixture `rough` is what estimate_messages_tokens_rough answered, in order. */
static json_t *rough_at(json_t *in, size_t i) {
    json_t *r = json_obj_get(in, "rough");
    if (!r || r->type != JSON_ARRAY || json_len(r) == 0) return json_number(0);
    return i < json_len(r) ? json_get(r, i) : json_get(r, json_len(r) - 1);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *tel = json_obj_get(in, "telemetry");
        json_t *out = NULL;
        if (!strcmp(func, "safe_int")) {
            out = w19_contextcom_da88c1_safe_int(json_obj_get(in, "value"));
        } else if (!strcmp(func, "compression_method")) {
            out = json_string(w19_contextcom_da88c1_compression_method(
                    json_get_bool(in, "fallback_used", false) ? 1 : 0,
                    json_get_str(in, "failure_error_prev", "")[0] != 0));
        } else if (!strcmp(func, "note_aux_fallback")) {
            out = w19_contextcom_da88c1_note_aux_fallback(tel, json_obj_get(in, "route"));
        } else if (!strcmp(func, "record_aux_call")) {
            out = w19_contextcom_da88c1_record_aux_call(tel, rough_at(in, 0),
                    json_obj_get(in, "max_tokens"), json_obj_get(in, "duration_ms"),
                    json_obj_get(in, "aux_provider"), json_obj_get(in, "aux_model"),
                    json_obj_get(in, "effective_aux_context"), json_obj_get(in, "phase_timings"));
        } else if (!strcmp(func, "record_effect")) {
            out = w19_contextcom_da88c1_record_effect(tel,
                    json_get_bool(in, "fallback_used", false) ? 1 : 0,
                    json_get_str(in, "failure_error_prev", "")[0] != 0,
                    json_obj_get(in, "messages_before"), json_obj_get(in, "compressed_len"),
                    json_obj_get(in, "tokens_before"), json_obj_get(in, "tool_results_pruned"),
                    json_obj_get(in, "reasoning_items_pruned"), json_obj_get(in, "last_dropped_count"),
                    rough_at(in, 0));
        } else if (!strcmp(func, "record_committed_effect")) {
            out = w19_contextcom_da88c1_record_committed_effect(tel,
                    json_obj_get(in, "before_len"), json_obj_get(in, "after_len"),
                    rough_at(in, 0), rough_at(in, 1));
        } else if (!strcmp(func, "record_regions")) {
            out = w19_contextcom_da88c1_record_regions(tel, rough_at(in, 0), rough_at(in, 1),
                                                       rough_at(in, 2));
        } else if (!strcmp(func, "begin_telemetry")) {
            out = w19_contextcom_da88c1_begin_telemetry(
                    json_obj_get(in, "attempt_id"), json_obj_get(in, "session_id"),
                    json_obj_get(in, "trigger_source"), json_obj_get(in, "seed"),
                    json_obj_get(in, "provider"), json_obj_get(in, "model"),
                    json_obj_get(in, "context_length"), json_obj_get(in, "current_tokens"),
                    json_obj_get(in, "threshold_tokens"));
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
