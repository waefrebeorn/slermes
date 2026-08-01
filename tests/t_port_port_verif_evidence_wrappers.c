/* AUTO-GENERATED integration oracle harness for port_verif_evidence_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_verif_evidence_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int vev_u_retention_cutoff(const char *);
extern int vev_u_db_path(const char *);
extern int vev_u_connect(const char *);
extern int vev_u_transaction(const char *);
extern int vev_u_ensure_schema(const char *);
extern int vev_u_split_segment_tokens(const char *);
extern int vev_u_clean_token(const char *);
extern int vev_u_canonical_tokens(const char *);
extern int vev_u_strip_command_prefix(const char *);
extern int vev_u_equivalent_needles(const char *);
extern int vev_u_is_under_root(const char *);
extern int vev_u_ad_hoc_script_args(const char *);
extern int vev_u_summarize_output(const char *);
extern int vev_u_prune_old_events(const char *);
extern int vev_classify_verification_command(const char *);
extern int vev_record_terminal_result(const char *);
extern int vev_mark_workspace_edited(const char *);
extern int vev_verification_status(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_vev_u_retention_cutoff(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_retention_cutoff(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_retention_cutoff"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_db_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_db_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_db_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_transaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_transaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_transaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_ensure_schema(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_ensure_schema(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_ensure_schema"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_split_segment_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_split_segment_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_split_segment_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_clean_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_clean_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_clean_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_canonical_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_canonical_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_canonical_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_strip_command_prefix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_strip_command_prefix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_strip_command_prefix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_equivalent_needles(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_equivalent_needles(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_equivalent_needles"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_is_under_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_is_under_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_is_under_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_ad_hoc_script_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_ad_hoc_script_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_ad_hoc_script_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_summarize_output(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_summarize_output(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_summarize_output"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_u_prune_old_events(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_u_prune_old_events(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_u_prune_old_events"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_classify_verification_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_classify_verification_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_classify_verification_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_record_terminal_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_record_terminal_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_record_terminal_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_mark_workspace_edited(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_mark_workspace_edited(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_mark_workspace_edited"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_vev_verification_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)vev_verification_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("vev_verification_status"));
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
        if (strcmp(op, "vev_u_retention_cutoff") == 0) o = emit_vev_u_retention_cutoff(c);
        if (strcmp(op, "vev_u_db_path") == 0) o = emit_vev_u_db_path(c);
        if (strcmp(op, "vev_u_connect") == 0) o = emit_vev_u_connect(c);
        if (strcmp(op, "vev_u_transaction") == 0) o = emit_vev_u_transaction(c);
        if (strcmp(op, "vev_u_ensure_schema") == 0) o = emit_vev_u_ensure_schema(c);
        if (strcmp(op, "vev_u_split_segment_tokens") == 0) o = emit_vev_u_split_segment_tokens(c);
        if (strcmp(op, "vev_u_clean_token") == 0) o = emit_vev_u_clean_token(c);
        if (strcmp(op, "vev_u_canonical_tokens") == 0) o = emit_vev_u_canonical_tokens(c);
        if (strcmp(op, "vev_u_strip_command_prefix") == 0) o = emit_vev_u_strip_command_prefix(c);
        if (strcmp(op, "vev_u_equivalent_needles") == 0) o = emit_vev_u_equivalent_needles(c);
        if (strcmp(op, "vev_u_is_under_root") == 0) o = emit_vev_u_is_under_root(c);
        if (strcmp(op, "vev_u_ad_hoc_script_args") == 0) o = emit_vev_u_ad_hoc_script_args(c);
        if (strcmp(op, "vev_u_summarize_output") == 0) o = emit_vev_u_summarize_output(c);
        if (strcmp(op, "vev_u_prune_old_events") == 0) o = emit_vev_u_prune_old_events(c);
        if (strcmp(op, "vev_classify_verification_command") == 0) o = emit_vev_classify_verification_command(c);
        if (strcmp(op, "vev_record_terminal_result") == 0) o = emit_vev_record_terminal_result(c);
        if (strcmp(op, "vev_mark_workspace_edited") == 0) o = emit_vev_mark_workspace_edited(c);
        if (strcmp(op, "vev_verification_status") == 0) o = emit_vev_verification_status(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
