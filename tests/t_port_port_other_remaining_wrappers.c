/* AUTO-GENERATED integration oracle harness for port_other_remaining_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_other_remaining_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int cron_executions_u_connect(const char *);
extern int cron_executions_u_initialize_schema(const char *);
extern int cron_executions_u_transaction(const char *);
extern int cron_executions_u_process_start_time(const char *);
extern int cron_executions_u_owner_is_live(const char *);
extern int cron_executions_u_prune_unlocked(const char *);
extern int cron_executions_create_execution(const char *);
extern int cron_executions_mark_execution_running(const char *);
extern int cron_executions_finish_execution(const char *);
extern int cron_executions_recover_interrupted_executions(const char *);
extern int cron_executions_list_executions(const char *);
extern int cron_executions_latest_execution(const char *);
extern int cron_executions_latest_executions(const char *);
extern int cron_jobs_u_current_cron_store(const char *);
extern int cron_jobs_use_cron_store(const char *);
extern int cron_jobs_get_cron_output_dir(const char *);
extern int cron_jobs_u_oneshot_run_claim_ttl_seconds(const char *);
extern int cron_jobs_u_job_running_in_this_process(const char *);
extern int cron_jobs_u_preserve_file_ownership(const char *);
extern int cron_jobs_record_ticker_error(const char *);
extern int cron_jobs_clear_ticker_error(const char *);
extern int cron_jobs_get_ticker_last_error(const char *);
extern int cron_scheduler_u_windows_cron_python_invocation(const char *);
extern int cron_scheduler_u_teardown_cron_agent(const char *);
extern int cron_scheduler_provider_recover_interrupted(const char *);
extern int cron_scheduler_provider_u_start_multiplex(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_cron_executions_u_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_u_initialize_schema(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_initialize_schema(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_initialize_schema"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_u_transaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_transaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_transaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_u_process_start_time(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_process_start_time(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_process_start_time"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_u_owner_is_live(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_owner_is_live(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_owner_is_live"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_u_prune_unlocked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_u_prune_unlocked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_u_prune_unlocked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_create_execution(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_create_execution(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_create_execution"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_mark_execution_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_mark_execution_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_mark_execution_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_finish_execution(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_finish_execution(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_finish_execution"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_recover_interrupted_executions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_recover_interrupted_executions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_recover_interrupted_executions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_list_executions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_list_executions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_list_executions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_latest_execution(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_latest_execution(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_latest_execution"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_executions_latest_executions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_executions_latest_executions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_executions_latest_executions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_u_current_cron_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_u_current_cron_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_u_current_cron_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_use_cron_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_use_cron_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_use_cron_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_get_cron_output_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_get_cron_output_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_get_cron_output_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_u_oneshot_run_claim_ttl_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_u_oneshot_run_claim_ttl_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_u_oneshot_run_claim_ttl_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_u_job_running_in_this_process(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_u_job_running_in_this_process(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_u_job_running_in_this_process"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_u_preserve_file_ownership(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_u_preserve_file_ownership(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_u_preserve_file_ownership"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_record_ticker_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_record_ticker_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_record_ticker_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_clear_ticker_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_clear_ticker_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_clear_ticker_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_jobs_get_ticker_last_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_jobs_get_ticker_last_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_jobs_get_ticker_last_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_scheduler_u_windows_cron_python_invocation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_scheduler_u_windows_cron_python_invocation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_scheduler_u_windows_cron_python_invocation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_scheduler_u_teardown_cron_agent(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_scheduler_u_teardown_cron_agent(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_scheduler_u_teardown_cron_agent"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_scheduler_provider_recover_interrupted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_scheduler_provider_recover_interrupted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_scheduler_provider_recover_interrupted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cron_scheduler_provider_u_start_multiplex(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cron_scheduler_provider_u_start_multiplex(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cron_scheduler_provider_u_start_multiplex"));
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
        if (strcmp(op, "cron_executions_u_connect") == 0) o = emit_cron_executions_u_connect(c);
        if (strcmp(op, "cron_executions_u_initialize_schema") == 0) o = emit_cron_executions_u_initialize_schema(c);
        if (strcmp(op, "cron_executions_u_transaction") == 0) o = emit_cron_executions_u_transaction(c);
        if (strcmp(op, "cron_executions_u_process_start_time") == 0) o = emit_cron_executions_u_process_start_time(c);
        if (strcmp(op, "cron_executions_u_owner_is_live") == 0) o = emit_cron_executions_u_owner_is_live(c);
        if (strcmp(op, "cron_executions_u_prune_unlocked") == 0) o = emit_cron_executions_u_prune_unlocked(c);
        if (strcmp(op, "cron_executions_create_execution") == 0) o = emit_cron_executions_create_execution(c);
        if (strcmp(op, "cron_executions_mark_execution_running") == 0) o = emit_cron_executions_mark_execution_running(c);
        if (strcmp(op, "cron_executions_finish_execution") == 0) o = emit_cron_executions_finish_execution(c);
        if (strcmp(op, "cron_executions_recover_interrupted_executions") == 0) o = emit_cron_executions_recover_interrupted_executions(c);
        if (strcmp(op, "cron_executions_list_executions") == 0) o = emit_cron_executions_list_executions(c);
        if (strcmp(op, "cron_executions_latest_execution") == 0) o = emit_cron_executions_latest_execution(c);
        if (strcmp(op, "cron_executions_latest_executions") == 0) o = emit_cron_executions_latest_executions(c);
        if (strcmp(op, "cron_jobs_u_current_cron_store") == 0) o = emit_cron_jobs_u_current_cron_store(c);
        if (strcmp(op, "cron_jobs_use_cron_store") == 0) o = emit_cron_jobs_use_cron_store(c);
        if (strcmp(op, "cron_jobs_get_cron_output_dir") == 0) o = emit_cron_jobs_get_cron_output_dir(c);
        if (strcmp(op, "cron_jobs_u_oneshot_run_claim_ttl_seconds") == 0) o = emit_cron_jobs_u_oneshot_run_claim_ttl_seconds(c);
        if (strcmp(op, "cron_jobs_u_job_running_in_this_process") == 0) o = emit_cron_jobs_u_job_running_in_this_process(c);
        if (strcmp(op, "cron_jobs_u_preserve_file_ownership") == 0) o = emit_cron_jobs_u_preserve_file_ownership(c);
        if (strcmp(op, "cron_jobs_record_ticker_error") == 0) o = emit_cron_jobs_record_ticker_error(c);
        if (strcmp(op, "cron_jobs_clear_ticker_error") == 0) o = emit_cron_jobs_clear_ticker_error(c);
        if (strcmp(op, "cron_jobs_get_ticker_last_error") == 0) o = emit_cron_jobs_get_ticker_last_error(c);
        if (strcmp(op, "cron_scheduler_u_windows_cron_python_invocation") == 0) o = emit_cron_scheduler_u_windows_cron_python_invocation(c);
        if (strcmp(op, "cron_scheduler_u_teardown_cron_agent") == 0) o = emit_cron_scheduler_u_teardown_cron_agent(c);
        if (strcmp(op, "cron_scheduler_provider_recover_interrupted") == 0) o = emit_cron_scheduler_provider_recover_interrupted(c);
        if (strcmp(op, "cron_scheduler_provider_u_start_multiplex") == 0) o = emit_cron_scheduler_provider_u_start_multiplex(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
