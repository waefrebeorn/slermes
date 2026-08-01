/* AUTO-GENERATED integration oracle harness for port_context_compressor_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_context_compressor_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int ctxc_u_begin_compression_telemetry(const char *);
extern int ctxc_u_record_compression_regions(const char *);
extern int ctxc_u_record_aux_compression_call(const char *);
extern int ctxc_u_load_fallback_compression_streak(const char *);
extern int ctxc_u_persist_fallback_compression_streak(const char *);
extern int ctxc_u_load_ineffective_compression_count(const char *);
extern int ctxc_u_persist_ineffective_compression_count(const char *);
extern int ctxc_u_record_ineffective_compression_verdict(const char *);
extern int ctxc_record_completed_compaction(const char *);
extern int ctxc_snapshot_preflight_display_tokens(const char *);
extern int ctxc_rollback_interrupted_preflight_display_tokens(const char *);
extern int ctxc_should_compress_info(const char *);
extern int ctxc_u_compression_block_reason(const char *);
extern int ctxc_u_refresh_durable_guards(const char *);
extern int ctxc_u_automatic_compression_blocked(const char *);
extern int ctxc_u_automatic_compression_blocked_locally(const char *);
extern int ctxc_prune_tool_results_only(const char *);
extern int ctxc_u_bound_summary_input(const char *);
extern int ctxc_u_validate_summary_user_provenance(const char *);
extern int ctxc_u_latest_user_task_snapshot(const char *);
extern int ctxc_u_ground_historical_task_snapshot(const char *);
extern int ctxc_u_ensure_last_n_user_messages_in_tail(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_ctxc_u_begin_compression_telemetry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_begin_compression_telemetry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_begin_compression_telemetry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_record_compression_regions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_record_compression_regions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_record_compression_regions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_record_aux_compression_call(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_record_aux_compression_call(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_record_aux_compression_call"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_load_fallback_compression_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_load_fallback_compression_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_load_fallback_compression_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_persist_fallback_compression_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_persist_fallback_compression_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_persist_fallback_compression_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_load_ineffective_compression_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_load_ineffective_compression_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_load_ineffective_compression_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_persist_ineffective_compression_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_persist_ineffective_compression_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_persist_ineffective_compression_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_record_ineffective_compression_verdict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_record_ineffective_compression_verdict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_record_ineffective_compression_verdict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_record_completed_compaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_record_completed_compaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_record_completed_compaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_snapshot_preflight_display_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_snapshot_preflight_display_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_snapshot_preflight_display_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_rollback_interrupted_preflight_display_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_rollback_interrupted_preflight_display_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_rollback_interrupted_preflight_display_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_should_compress_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_should_compress_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_should_compress_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_compression_block_reason(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_compression_block_reason(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_compression_block_reason"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_refresh_durable_guards(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_refresh_durable_guards(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_refresh_durable_guards"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_automatic_compression_blocked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_automatic_compression_blocked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_automatic_compression_blocked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_automatic_compression_blocked_locally(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_automatic_compression_blocked_locally(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_automatic_compression_blocked_locally"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_prune_tool_results_only(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_prune_tool_results_only(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_prune_tool_results_only"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_bound_summary_input(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_bound_summary_input(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_bound_summary_input"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_validate_summary_user_provenance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_validate_summary_user_provenance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_validate_summary_user_provenance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_latest_user_task_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_latest_user_task_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_latest_user_task_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_ground_historical_task_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_ground_historical_task_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_ground_historical_task_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_ctxc_u_ensure_last_n_user_messages_in_tail(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)ctxc_u_ensure_last_n_user_messages_in_tail(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("ctxc_u_ensure_last_n_user_messages_in_tail"));
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
        if (strcmp(op, "ctxc_u_begin_compression_telemetry") == 0) o = emit_ctxc_u_begin_compression_telemetry(c);
        if (strcmp(op, "ctxc_u_record_compression_regions") == 0) o = emit_ctxc_u_record_compression_regions(c);
        if (strcmp(op, "ctxc_u_record_aux_compression_call") == 0) o = emit_ctxc_u_record_aux_compression_call(c);
        if (strcmp(op, "ctxc_u_load_fallback_compression_streak") == 0) o = emit_ctxc_u_load_fallback_compression_streak(c);
        if (strcmp(op, "ctxc_u_persist_fallback_compression_streak") == 0) o = emit_ctxc_u_persist_fallback_compression_streak(c);
        if (strcmp(op, "ctxc_u_load_ineffective_compression_count") == 0) o = emit_ctxc_u_load_ineffective_compression_count(c);
        if (strcmp(op, "ctxc_u_persist_ineffective_compression_count") == 0) o = emit_ctxc_u_persist_ineffective_compression_count(c);
        if (strcmp(op, "ctxc_u_record_ineffective_compression_verdict") == 0) o = emit_ctxc_u_record_ineffective_compression_verdict(c);
        if (strcmp(op, "ctxc_record_completed_compaction") == 0) o = emit_ctxc_record_completed_compaction(c);
        if (strcmp(op, "ctxc_snapshot_preflight_display_tokens") == 0) o = emit_ctxc_snapshot_preflight_display_tokens(c);
        if (strcmp(op, "ctxc_rollback_interrupted_preflight_display_tokens") == 0) o = emit_ctxc_rollback_interrupted_preflight_display_tokens(c);
        if (strcmp(op, "ctxc_should_compress_info") == 0) o = emit_ctxc_should_compress_info(c);
        if (strcmp(op, "ctxc_u_compression_block_reason") == 0) o = emit_ctxc_u_compression_block_reason(c);
        if (strcmp(op, "ctxc_u_refresh_durable_guards") == 0) o = emit_ctxc_u_refresh_durable_guards(c);
        if (strcmp(op, "ctxc_u_automatic_compression_blocked") == 0) o = emit_ctxc_u_automatic_compression_blocked(c);
        if (strcmp(op, "ctxc_u_automatic_compression_blocked_locally") == 0) o = emit_ctxc_u_automatic_compression_blocked_locally(c);
        if (strcmp(op, "ctxc_prune_tool_results_only") == 0) o = emit_ctxc_prune_tool_results_only(c);
        if (strcmp(op, "ctxc_u_bound_summary_input") == 0) o = emit_ctxc_u_bound_summary_input(c);
        if (strcmp(op, "ctxc_u_validate_summary_user_provenance") == 0) o = emit_ctxc_u_validate_summary_user_provenance(c);
        if (strcmp(op, "ctxc_u_latest_user_task_snapshot") == 0) o = emit_ctxc_u_latest_user_task_snapshot(c);
        if (strcmp(op, "ctxc_u_ground_historical_task_snapshot") == 0) o = emit_ctxc_u_ground_historical_task_snapshot(c);
        if (strcmp(op, "ctxc_u_ensure_last_n_user_messages_in_tail") == 0) o = emit_ctxc_u_ensure_last_n_user_messages_in_tail(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
