/* AUTO-GENERATED integration oracle harness for port_async_delegation_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_async_delegation_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int adel_u_db_path(const char *);
extern int adel_u_connect(const char *);
extern int adel_u_initialize_schema(const char *);
extern int adel_u_transaction(const char *);
extern int adel_u_persist_dispatch(const char *);
extern int adel_u_delete_durable_delegation(const char *);
extern int adel_u_prune_durable_records(const char *);
extern int adel_u_persist_completion(const char *);
extern int adel_u_note_delivery_attempt(const char *);
extern int adel_recover_abandoned_delegations(const char *);
extern int adel_restore_undelivered_completions(const char *);
extern int adel_mark_completion_delivered(const char *);
extern int adel_claim_completion_delivery(const char *);
extern int adel_claim_event_delivery(const char *);
extern int adel_release_completion_delivery(const char *);
extern int adel_drop_completion_delivery(const char *);
extern int adel_complete_completion_delivery(const char *);
extern int adel_complete_event_delivery(const char *);
extern int adel_release_event_delivery(const char *);
extern int adel_get_durable_delegation(const char *);
extern int adel_u_get_executor(const char *);
extern int adel_u_new_delegation_id(const char *);
extern int adel_u_current_origin_session_id(const char *);
extern int adel_dispatch_async_delegation(const char *);
extern int adel_u_push_completion_event(const char *);
extern int adel_dispatch_async_delegation_batch(const char *);
extern int adel_u_finalize_batch(const char *);
extern int adel_list_async_delegations(const char *);
extern int adel_interrupt_for_session(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_adel_u_db_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_db_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_db_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_initialize_schema(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_initialize_schema(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_initialize_schema"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_transaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_transaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_transaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_persist_dispatch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_persist_dispatch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_persist_dispatch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_delete_durable_delegation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_delete_durable_delegation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_delete_durable_delegation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_prune_durable_records(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_prune_durable_records(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_prune_durable_records"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_persist_completion(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_persist_completion(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_persist_completion"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_note_delivery_attempt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_note_delivery_attempt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_note_delivery_attempt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_recover_abandoned_delegations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_recover_abandoned_delegations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_recover_abandoned_delegations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_restore_undelivered_completions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_restore_undelivered_completions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_restore_undelivered_completions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_mark_completion_delivered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_mark_completion_delivered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_mark_completion_delivered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_claim_completion_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_claim_completion_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_claim_completion_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_claim_event_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_claim_event_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_claim_event_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_release_completion_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_release_completion_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_release_completion_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_drop_completion_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_drop_completion_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_drop_completion_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_complete_completion_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_complete_completion_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_complete_completion_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_complete_event_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_complete_event_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_complete_event_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_release_event_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_release_event_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_release_event_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_get_durable_delegation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_get_durable_delegation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_get_durable_delegation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_get_executor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_get_executor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_get_executor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_new_delegation_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_new_delegation_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_new_delegation_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_current_origin_session_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_current_origin_session_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_current_origin_session_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_dispatch_async_delegation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_dispatch_async_delegation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_dispatch_async_delegation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_push_completion_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_push_completion_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_push_completion_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_dispatch_async_delegation_batch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_dispatch_async_delegation_batch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_dispatch_async_delegation_batch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_u_finalize_batch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_u_finalize_batch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_u_finalize_batch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_list_async_delegations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_list_async_delegations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_list_async_delegations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_adel_interrupt_for_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)adel_interrupt_for_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("adel_interrupt_for_session"));
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
        if (strcmp(op, "adel_u_db_path") == 0) o = emit_adel_u_db_path(c);
        if (strcmp(op, "adel_u_connect") == 0) o = emit_adel_u_connect(c);
        if (strcmp(op, "adel_u_initialize_schema") == 0) o = emit_adel_u_initialize_schema(c);
        if (strcmp(op, "adel_u_transaction") == 0) o = emit_adel_u_transaction(c);
        if (strcmp(op, "adel_u_persist_dispatch") == 0) o = emit_adel_u_persist_dispatch(c);
        if (strcmp(op, "adel_u_delete_durable_delegation") == 0) o = emit_adel_u_delete_durable_delegation(c);
        if (strcmp(op, "adel_u_prune_durable_records") == 0) o = emit_adel_u_prune_durable_records(c);
        if (strcmp(op, "adel_u_persist_completion") == 0) o = emit_adel_u_persist_completion(c);
        if (strcmp(op, "adel_u_note_delivery_attempt") == 0) o = emit_adel_u_note_delivery_attempt(c);
        if (strcmp(op, "adel_recover_abandoned_delegations") == 0) o = emit_adel_recover_abandoned_delegations(c);
        if (strcmp(op, "adel_restore_undelivered_completions") == 0) o = emit_adel_restore_undelivered_completions(c);
        if (strcmp(op, "adel_mark_completion_delivered") == 0) o = emit_adel_mark_completion_delivered(c);
        if (strcmp(op, "adel_claim_completion_delivery") == 0) o = emit_adel_claim_completion_delivery(c);
        if (strcmp(op, "adel_claim_event_delivery") == 0) o = emit_adel_claim_event_delivery(c);
        if (strcmp(op, "adel_release_completion_delivery") == 0) o = emit_adel_release_completion_delivery(c);
        if (strcmp(op, "adel_drop_completion_delivery") == 0) o = emit_adel_drop_completion_delivery(c);
        if (strcmp(op, "adel_complete_completion_delivery") == 0) o = emit_adel_complete_completion_delivery(c);
        if (strcmp(op, "adel_complete_event_delivery") == 0) o = emit_adel_complete_event_delivery(c);
        if (strcmp(op, "adel_release_event_delivery") == 0) o = emit_adel_release_event_delivery(c);
        if (strcmp(op, "adel_get_durable_delegation") == 0) o = emit_adel_get_durable_delegation(c);
        if (strcmp(op, "adel_u_get_executor") == 0) o = emit_adel_u_get_executor(c);
        if (strcmp(op, "adel_u_new_delegation_id") == 0) o = emit_adel_u_new_delegation_id(c);
        if (strcmp(op, "adel_u_current_origin_session_id") == 0) o = emit_adel_u_current_origin_session_id(c);
        if (strcmp(op, "adel_dispatch_async_delegation") == 0) o = emit_adel_dispatch_async_delegation(c);
        if (strcmp(op, "adel_u_push_completion_event") == 0) o = emit_adel_u_push_completion_event(c);
        if (strcmp(op, "adel_dispatch_async_delegation_batch") == 0) o = emit_adel_dispatch_async_delegation_batch(c);
        if (strcmp(op, "adel_u_finalize_batch") == 0) o = emit_adel_u_finalize_batch(c);
        if (strcmp(op, "adel_list_async_delegations") == 0) o = emit_adel_list_async_delegations(c);
        if (strcmp(op, "adel_interrupt_for_session") == 0) o = emit_adel_interrupt_for_session(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
