/* AUTO-GENERATED integration oracle harness for port_kanban_db_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_kanban_db_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int kdbport_u_assert_not_delegated_child_mutation(const char *);
extern int kdbport_scoped_current_board(const char *);
extern int kdbport_from_row(const char *);
extern int kdbport_from_row_2(const char *);
extern int kdbport_u_sqlite_connect(const char *);
extern int kdbport_u_maybe_checkpoint_wal(const char *);
extern int kdbport_u_prune_corrupt_backups(const char *);
extern int kdbport_u_integrity_messages_ok(const char *);
extern int kdbport_u_run_integrity_check(const char *);
extern int kdbport_u_repairable_index_names(const char *);
extern int kdbport_u_attempt_index_reindex_repair(const char *);
extern int kdbport_repair_db(const char *);
extern int kdbport_u_migrate_add_optional_columns(const char *);
extern int kdbport_set_model_override(const char *);
extern int kdbport_u_safe_attachment_name(const char *);
extern int kdbport_u_collision_free_path(const char *);
extern int kdbport_store_attachment_bytes(const char *);
extern int kdbport_u_merge_completion_prose_artifacts(const char *);
extern int kdbport_u_persist_scratch_completion_artifacts(const char *);
extern int kdbport_u_insert_completion_attachment(const char *);
extern int kdbport_u_unique_attachment_path(const char *);
extern int kdbport_u_managed_scratch_path_info(const char *);
extern int kdbport_decompose_triage_task(const char *);
extern int kdbport_u_protocol_violation_streak(const char *);
extern int kdbport_list_runs(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_kdbport_u_assert_not_delegated_child_mutation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_assert_not_delegated_child_mutation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_assert_not_delegated_child_mutation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_scoped_current_board(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_scoped_current_board(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_scoped_current_board"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_from_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_from_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_from_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_from_row_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_from_row_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_from_row_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_sqlite_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_sqlite_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_sqlite_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_maybe_checkpoint_wal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_maybe_checkpoint_wal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_maybe_checkpoint_wal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_prune_corrupt_backups(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_prune_corrupt_backups(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_prune_corrupt_backups"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_integrity_messages_ok(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_integrity_messages_ok(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_integrity_messages_ok"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_run_integrity_check(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_run_integrity_check(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_run_integrity_check"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_repairable_index_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_repairable_index_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_repairable_index_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_attempt_index_reindex_repair(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_attempt_index_reindex_repair(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_attempt_index_reindex_repair"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_repair_db(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_repair_db(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_repair_db"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_migrate_add_optional_columns(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_migrate_add_optional_columns(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_migrate_add_optional_columns"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_set_model_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_set_model_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_set_model_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_safe_attachment_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_safe_attachment_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_safe_attachment_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_collision_free_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_collision_free_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_collision_free_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_store_attachment_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_store_attachment_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_store_attachment_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_merge_completion_prose_artifacts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_merge_completion_prose_artifacts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_merge_completion_prose_artifacts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_persist_scratch_completion_artifacts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_persist_scratch_completion_artifacts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_persist_scratch_completion_artifacts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_insert_completion_attachment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_insert_completion_attachment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_insert_completion_attachment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_unique_attachment_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_unique_attachment_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_unique_attachment_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_managed_scratch_path_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_managed_scratch_path_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_managed_scratch_path_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_decompose_triage_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_decompose_triage_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_decompose_triage_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_u_protocol_violation_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_u_protocol_violation_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_u_protocol_violation_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_kdbport_list_runs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)kdbport_list_runs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("kdbport_list_runs"));
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
        if (strcmp(op, "kdbport_u_assert_not_delegated_child_mutation") == 0) o = emit_kdbport_u_assert_not_delegated_child_mutation(c);
        if (strcmp(op, "kdbport_scoped_current_board") == 0) o = emit_kdbport_scoped_current_board(c);
        if (strcmp(op, "kdbport_from_row") == 0) o = emit_kdbport_from_row(c);
        if (strcmp(op, "kdbport_from_row_2") == 0) o = emit_kdbport_from_row_2(c);
        if (strcmp(op, "kdbport_u_sqlite_connect") == 0) o = emit_kdbport_u_sqlite_connect(c);
        if (strcmp(op, "kdbport_u_maybe_checkpoint_wal") == 0) o = emit_kdbport_u_maybe_checkpoint_wal(c);
        if (strcmp(op, "kdbport_u_prune_corrupt_backups") == 0) o = emit_kdbport_u_prune_corrupt_backups(c);
        if (strcmp(op, "kdbport_u_integrity_messages_ok") == 0) o = emit_kdbport_u_integrity_messages_ok(c);
        if (strcmp(op, "kdbport_u_run_integrity_check") == 0) o = emit_kdbport_u_run_integrity_check(c);
        if (strcmp(op, "kdbport_u_repairable_index_names") == 0) o = emit_kdbport_u_repairable_index_names(c);
        if (strcmp(op, "kdbport_u_attempt_index_reindex_repair") == 0) o = emit_kdbport_u_attempt_index_reindex_repair(c);
        if (strcmp(op, "kdbport_repair_db") == 0) o = emit_kdbport_repair_db(c);
        if (strcmp(op, "kdbport_u_migrate_add_optional_columns") == 0) o = emit_kdbport_u_migrate_add_optional_columns(c);
        if (strcmp(op, "kdbport_set_model_override") == 0) o = emit_kdbport_set_model_override(c);
        if (strcmp(op, "kdbport_u_safe_attachment_name") == 0) o = emit_kdbport_u_safe_attachment_name(c);
        if (strcmp(op, "kdbport_u_collision_free_path") == 0) o = emit_kdbport_u_collision_free_path(c);
        if (strcmp(op, "kdbport_store_attachment_bytes") == 0) o = emit_kdbport_store_attachment_bytes(c);
        if (strcmp(op, "kdbport_u_merge_completion_prose_artifacts") == 0) o = emit_kdbport_u_merge_completion_prose_artifacts(c);
        if (strcmp(op, "kdbport_u_persist_scratch_completion_artifacts") == 0) o = emit_kdbport_u_persist_scratch_completion_artifacts(c);
        if (strcmp(op, "kdbport_u_insert_completion_attachment") == 0) o = emit_kdbport_u_insert_completion_attachment(c);
        if (strcmp(op, "kdbport_u_unique_attachment_path") == 0) o = emit_kdbport_u_unique_attachment_path(c);
        if (strcmp(op, "kdbport_u_managed_scratch_path_info") == 0) o = emit_kdbport_u_managed_scratch_path_info(c);
        if (strcmp(op, "kdbport_decompose_triage_task") == 0) o = emit_kdbport_decompose_triage_task(c);
        if (strcmp(op, "kdbport_u_protocol_violation_streak") == 0) o = emit_kdbport_u_protocol_violation_streak(c);
        if (strcmp(op, "kdbport_list_runs") == 0) o = emit_kdbport_list_runs(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
