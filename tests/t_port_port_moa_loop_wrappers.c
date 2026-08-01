/* AUTO-GENERATED integration oracle harness for port_moa_loop_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_moa_loop_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int moa_u_redact_reference_text(const char *);
extern int moa_u_moa_privacy_mode(const char *);
extern int moa_u_redact_reference_outputs(const char *);
extern int moa_u_redact_trace_messages(const char *);
extern int moa_u_redact_trace_accounting(const char *);
extern int moa_u_slot_label(const char *);
extern int moa_u_slot_reasoning_config(const char *);
extern int moa_u_aggregator_reasoning_config(const char *);
extern int moa_u_slot_runtime(const char *);
extern int moa_u_merge_slot_extra_body(const char *);
extern int moa_u_maybe_apply_moa_cache_control(const char *);
extern int moa_u_run_reference(const char *);
extern int moa_u_trim_messages_for_reference(const char *);
extern int moa_u_run_references_parallel(const char *);
extern int moa_u_truncate_tool_result(const char *);
extern int moa_u_render_tool_calls(const char *);
extern int moa_u_reference_messages(const char *);
extern int moa_u_preset_temperature(const char *);
extern int moa_u_is_failed_reference(const char *);
extern int moa_u_successful_references(const char *);
extern int moa_u_failed_reference_labels(const char *);
extern int moa_u_degraded_notice(const char *);
extern int moa_aggregate_moa_context(const char *);
extern int moa_u_attach_reference_guidance(const char *);
extern int moa_consume_reference_usage(const char *);
extern int moa_u_record_late_reference_accounting(const char *);
extern int moa_consume_and_save_trace(const char *);
extern int moa_prepare(const char *);
extern int moa_rebase_prepared_request(const char *);
extern int moa_u_call_prepared_aggregator(const char *);
extern int moa_consume_reference_usage_2(const char *);
extern int moa_last_aggregator_slot(const char *);
extern int moa_consume_and_save_trace_2(const char *);
extern int moa_build_moa_facade(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_moa_u_redact_reference_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_redact_reference_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_redact_reference_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_moa_privacy_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_moa_privacy_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_moa_privacy_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_redact_reference_outputs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_redact_reference_outputs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_redact_reference_outputs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_redact_trace_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_redact_trace_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_redact_trace_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_redact_trace_accounting(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_redact_trace_accounting(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_redact_trace_accounting"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_slot_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_slot_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_slot_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_slot_reasoning_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_slot_reasoning_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_slot_reasoning_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_aggregator_reasoning_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_aggregator_reasoning_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_aggregator_reasoning_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_slot_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_slot_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_slot_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_merge_slot_extra_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_merge_slot_extra_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_merge_slot_extra_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_maybe_apply_moa_cache_control(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_maybe_apply_moa_cache_control(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_maybe_apply_moa_cache_control"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_run_reference(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_run_reference(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_run_reference"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_trim_messages_for_reference(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_trim_messages_for_reference(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_trim_messages_for_reference"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_run_references_parallel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_run_references_parallel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_run_references_parallel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_truncate_tool_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_truncate_tool_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_truncate_tool_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_render_tool_calls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_render_tool_calls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_render_tool_calls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_reference_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_reference_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_reference_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_preset_temperature(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_preset_temperature(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_preset_temperature"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_is_failed_reference(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_is_failed_reference(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_is_failed_reference"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_successful_references(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_successful_references(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_successful_references"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_failed_reference_labels(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_failed_reference_labels(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_failed_reference_labels"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_degraded_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_degraded_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_degraded_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_aggregate_moa_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_aggregate_moa_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_aggregate_moa_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_attach_reference_guidance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_attach_reference_guidance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_attach_reference_guidance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_consume_reference_usage(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_consume_reference_usage(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_consume_reference_usage"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_record_late_reference_accounting(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_record_late_reference_accounting(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_record_late_reference_accounting"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_consume_and_save_trace(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_consume_and_save_trace(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_consume_and_save_trace"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_prepare(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_prepare(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_prepare"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_rebase_prepared_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_rebase_prepared_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_rebase_prepared_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_u_call_prepared_aggregator(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_u_call_prepared_aggregator(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_u_call_prepared_aggregator"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_consume_reference_usage_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_consume_reference_usage_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_consume_reference_usage_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_last_aggregator_slot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_last_aggregator_slot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_last_aggregator_slot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_consume_and_save_trace_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_consume_and_save_trace_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_consume_and_save_trace_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_moa_build_moa_facade(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)moa_build_moa_facade(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("moa_build_moa_facade"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "moa_u_redact_reference_text") == 0) o = emit_moa_u_redact_reference_text(c);
        if (strcmp(op, "moa_u_moa_privacy_mode") == 0) o = emit_moa_u_moa_privacy_mode(c);
        if (strcmp(op, "moa_u_redact_reference_outputs") == 0) o = emit_moa_u_redact_reference_outputs(c);
        if (strcmp(op, "moa_u_redact_trace_messages") == 0) o = emit_moa_u_redact_trace_messages(c);
        if (strcmp(op, "moa_u_redact_trace_accounting") == 0) o = emit_moa_u_redact_trace_accounting(c);
        if (strcmp(op, "moa_u_slot_label") == 0) o = emit_moa_u_slot_label(c);
        if (strcmp(op, "moa_u_slot_reasoning_config") == 0) o = emit_moa_u_slot_reasoning_config(c);
        if (strcmp(op, "moa_u_aggregator_reasoning_config") == 0) o = emit_moa_u_aggregator_reasoning_config(c);
        if (strcmp(op, "moa_u_slot_runtime") == 0) o = emit_moa_u_slot_runtime(c);
        if (strcmp(op, "moa_u_merge_slot_extra_body") == 0) o = emit_moa_u_merge_slot_extra_body(c);
        if (strcmp(op, "moa_u_maybe_apply_moa_cache_control") == 0) o = emit_moa_u_maybe_apply_moa_cache_control(c);
        if (strcmp(op, "moa_u_run_reference") == 0) o = emit_moa_u_run_reference(c);
        if (strcmp(op, "moa_u_trim_messages_for_reference") == 0) o = emit_moa_u_trim_messages_for_reference(c);
        if (strcmp(op, "moa_u_run_references_parallel") == 0) o = emit_moa_u_run_references_parallel(c);
        if (strcmp(op, "moa_u_truncate_tool_result") == 0) o = emit_moa_u_truncate_tool_result(c);
        if (strcmp(op, "moa_u_render_tool_calls") == 0) o = emit_moa_u_render_tool_calls(c);
        if (strcmp(op, "moa_u_reference_messages") == 0) o = emit_moa_u_reference_messages(c);
        if (strcmp(op, "moa_u_preset_temperature") == 0) o = emit_moa_u_preset_temperature(c);
        if (strcmp(op, "moa_u_is_failed_reference") == 0) o = emit_moa_u_is_failed_reference(c);
        if (strcmp(op, "moa_u_successful_references") == 0) o = emit_moa_u_successful_references(c);
        if (strcmp(op, "moa_u_failed_reference_labels") == 0) o = emit_moa_u_failed_reference_labels(c);
        if (strcmp(op, "moa_u_degraded_notice") == 0) o = emit_moa_u_degraded_notice(c);
        if (strcmp(op, "moa_aggregate_moa_context") == 0) o = emit_moa_aggregate_moa_context(c);
        if (strcmp(op, "moa_u_attach_reference_guidance") == 0) o = emit_moa_u_attach_reference_guidance(c);
        if (strcmp(op, "moa_consume_reference_usage") == 0) o = emit_moa_consume_reference_usage(c);
        if (strcmp(op, "moa_u_record_late_reference_accounting") == 0) o = emit_moa_u_record_late_reference_accounting(c);
        if (strcmp(op, "moa_consume_and_save_trace") == 0) o = emit_moa_consume_and_save_trace(c);
        if (strcmp(op, "moa_prepare") == 0) o = emit_moa_prepare(c);
        if (strcmp(op, "moa_rebase_prepared_request") == 0) o = emit_moa_rebase_prepared_request(c);
        if (strcmp(op, "moa_u_call_prepared_aggregator") == 0) o = emit_moa_u_call_prepared_aggregator(c);
        if (strcmp(op, "moa_consume_reference_usage_2") == 0) o = emit_moa_consume_reference_usage_2(c);
        if (strcmp(op, "moa_last_aggregator_slot") == 0) o = emit_moa_last_aggregator_slot(c);
        if (strcmp(op, "moa_consume_and_save_trace_2") == 0) o = emit_moa_consume_and_save_trace_2(c);
        if (strcmp(op, "moa_build_moa_facade") == 0) o = emit_moa_build_moa_facade(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
