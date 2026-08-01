/* AUTO-GENERATED integration oracle harness for port_skill_mgr_tool_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_skill_mgr_tool_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int smt_mark_background_review_skill_read(const char *);
extern int smt_u_background_review_has_read(const char *);
extern int smt_u_reset_background_review_read_marks(const char *);
extern int smt_u_guard_agent_created_enabled(const char *);
extern int smt_u_security_scan_skill(const char *);
extern int smt_u_pinned_guard(const char *);
extern int smt_u_background_review_write_guard(const char *);
extern int smt_u_background_review_read_before_write_guard(const char *);
extern int smt_u_background_review_preflight(const char *);
extern int smt_u_curator_consolidation_delete_guard(const char *);
extern int smt_u_validate_category(const char *);
extern int smt_u_validate_frontmatter(const char *);
extern int smt_u_resolve_skill_dir(const char *);
extern int smt_u_find_skill_in_other_profiles(const char *);
extern int smt_u_skill_not_found_error(const char *);
extern int smt_u_atomic_write_text(const char *);
extern int smt_u_add_description_prompt_preview(const char *);
extern int smt_u_create_skill(const char *);
extern int smt_u_edit_skill(const char *);
extern int smt_u_patch_skill(const char *);
extern int smt_u_delete_skill(const char *);
extern int smt_u_remove_file(const char *);
extern int smt_u_apply_skill_write_gate(const char *);
extern int smt_apply_skill_pending(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_smt_mark_background_review_skill_read(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_mark_background_review_skill_read(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_mark_background_review_skill_read"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_background_review_has_read(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_background_review_has_read(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_background_review_has_read"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_reset_background_review_read_marks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_reset_background_review_read_marks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_reset_background_review_read_marks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_guard_agent_created_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_guard_agent_created_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_guard_agent_created_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_security_scan_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_security_scan_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_security_scan_skill"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_pinned_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_pinned_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_pinned_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_background_review_write_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_background_review_write_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_background_review_write_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_background_review_read_before_write_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_background_review_read_before_write_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_background_review_read_before_write_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_background_review_preflight(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_background_review_preflight(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_background_review_preflight"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_curator_consolidation_delete_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_curator_consolidation_delete_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_curator_consolidation_delete_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_validate_category(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_validate_category(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_validate_category"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_validate_frontmatter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_validate_frontmatter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_validate_frontmatter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_resolve_skill_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_resolve_skill_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_resolve_skill_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_find_skill_in_other_profiles(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_find_skill_in_other_profiles(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_find_skill_in_other_profiles"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_skill_not_found_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_skill_not_found_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_skill_not_found_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_atomic_write_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_atomic_write_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_atomic_write_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_add_description_prompt_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_add_description_prompt_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_add_description_prompt_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_create_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_create_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_create_skill"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_edit_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_edit_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_edit_skill"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_patch_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_patch_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_patch_skill"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_delete_skill(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_delete_skill(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_delete_skill"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_remove_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_remove_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_remove_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_u_apply_skill_write_gate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_u_apply_skill_write_gate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_u_apply_skill_write_gate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_smt_apply_skill_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)smt_apply_skill_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("smt_apply_skill_pending"));
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
        if (strcmp(op, "smt_mark_background_review_skill_read") == 0) o = emit_smt_mark_background_review_skill_read(c);
        if (strcmp(op, "smt_u_background_review_has_read") == 0) o = emit_smt_u_background_review_has_read(c);
        if (strcmp(op, "smt_u_reset_background_review_read_marks") == 0) o = emit_smt_u_reset_background_review_read_marks(c);
        if (strcmp(op, "smt_u_guard_agent_created_enabled") == 0) o = emit_smt_u_guard_agent_created_enabled(c);
        if (strcmp(op, "smt_u_security_scan_skill") == 0) o = emit_smt_u_security_scan_skill(c);
        if (strcmp(op, "smt_u_pinned_guard") == 0) o = emit_smt_u_pinned_guard(c);
        if (strcmp(op, "smt_u_background_review_write_guard") == 0) o = emit_smt_u_background_review_write_guard(c);
        if (strcmp(op, "smt_u_background_review_read_before_write_guard") == 0) o = emit_smt_u_background_review_read_before_write_guard(c);
        if (strcmp(op, "smt_u_background_review_preflight") == 0) o = emit_smt_u_background_review_preflight(c);
        if (strcmp(op, "smt_u_curator_consolidation_delete_guard") == 0) o = emit_smt_u_curator_consolidation_delete_guard(c);
        if (strcmp(op, "smt_u_validate_category") == 0) o = emit_smt_u_validate_category(c);
        if (strcmp(op, "smt_u_validate_frontmatter") == 0) o = emit_smt_u_validate_frontmatter(c);
        if (strcmp(op, "smt_u_resolve_skill_dir") == 0) o = emit_smt_u_resolve_skill_dir(c);
        if (strcmp(op, "smt_u_find_skill_in_other_profiles") == 0) o = emit_smt_u_find_skill_in_other_profiles(c);
        if (strcmp(op, "smt_u_skill_not_found_error") == 0) o = emit_smt_u_skill_not_found_error(c);
        if (strcmp(op, "smt_u_atomic_write_text") == 0) o = emit_smt_u_atomic_write_text(c);
        if (strcmp(op, "smt_u_add_description_prompt_preview") == 0) o = emit_smt_u_add_description_prompt_preview(c);
        if (strcmp(op, "smt_u_create_skill") == 0) o = emit_smt_u_create_skill(c);
        if (strcmp(op, "smt_u_edit_skill") == 0) o = emit_smt_u_edit_skill(c);
        if (strcmp(op, "smt_u_patch_skill") == 0) o = emit_smt_u_patch_skill(c);
        if (strcmp(op, "smt_u_delete_skill") == 0) o = emit_smt_u_delete_skill(c);
        if (strcmp(op, "smt_u_remove_file") == 0) o = emit_smt_u_remove_file(c);
        if (strcmp(op, "smt_u_apply_skill_write_gate") == 0) o = emit_smt_u_apply_skill_write_gate(c);
        if (strcmp(op, "smt_apply_skill_pending") == 0) o = emit_smt_apply_skill_pending(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
