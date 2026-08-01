/* AUTO-GENERATED integration oracle harness for port_skill_utils_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_skill_utils_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int sku_is_excluded_skill_path(const char *);
extern int sku_yaml_load(const char *);
extern int sku_parse_frontmatter(const char *);
extern int sku_skill_matches_platform_list(const char *);
extern int sku_skill_matches_platform(const char *);
extern int sku_u_detect_environment(const char *);
extern int sku_skill_matches_environment(const char *);
extern int sku_get_disabled_skill_names(const char *);
extern int sku_u_normalize_string_set(const char *);
extern int sku_u_external_dirs_cache_clear(const char *);
extern int sku_get_external_skills_dirs(const char *);
extern int sku_get_all_skills_dirs(const char *);
extern int sku_normalize_skill_lookup_name(const char *);
extern int sku_extract_skill_conditions(const char *);
extern int sku_extract_skill_config_vars(const char *);
extern int sku_discover_all_skill_config_vars(const char *);
extern int sku_u_resolve_dotpath(const char *);
extern int sku_resolve_skill_config_values(const char *);
extern int sku_u_normalize_skill_description(const char *);
extern int sku_extract_skill_description(const char *);
extern int sku_is_skill_description_truncated_for_prompt(const char *);
extern int sku_iter_skill_index_files(const char *);
extern int sku_parse_qualified_name(const char *);
extern int sku_is_valid_namespace(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_sku_is_excluded_skill_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_is_excluded_skill_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_is_excluded_skill_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_yaml_load(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_yaml_load(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_yaml_load"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_parse_frontmatter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_parse_frontmatter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_parse_frontmatter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_skill_matches_platform_list(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_skill_matches_platform_list(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_skill_matches_platform_list"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_skill_matches_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_skill_matches_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_skill_matches_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_u_detect_environment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_u_detect_environment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_u_detect_environment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_skill_matches_environment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_skill_matches_environment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_skill_matches_environment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_get_disabled_skill_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_get_disabled_skill_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_get_disabled_skill_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_u_normalize_string_set(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_u_normalize_string_set(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_u_normalize_string_set"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_u_external_dirs_cache_clear(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_u_external_dirs_cache_clear(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_u_external_dirs_cache_clear"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_get_external_skills_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_get_external_skills_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_get_external_skills_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_get_all_skills_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_get_all_skills_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_get_all_skills_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_normalize_skill_lookup_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_normalize_skill_lookup_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_normalize_skill_lookup_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_extract_skill_conditions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_extract_skill_conditions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_extract_skill_conditions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_extract_skill_config_vars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_extract_skill_config_vars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_extract_skill_config_vars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_discover_all_skill_config_vars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_discover_all_skill_config_vars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_discover_all_skill_config_vars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_u_resolve_dotpath(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_u_resolve_dotpath(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_u_resolve_dotpath"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_resolve_skill_config_values(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_resolve_skill_config_values(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_resolve_skill_config_values"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_u_normalize_skill_description(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_u_normalize_skill_description(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_u_normalize_skill_description"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_extract_skill_description(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_extract_skill_description(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_extract_skill_description"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_is_skill_description_truncated_for_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_is_skill_description_truncated_for_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_is_skill_description_truncated_for_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_iter_skill_index_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_iter_skill_index_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_iter_skill_index_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_parse_qualified_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_parse_qualified_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_parse_qualified_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sku_is_valid_namespace(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sku_is_valid_namespace(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sku_is_valid_namespace"));
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
        if (strcmp(op, "sku_is_excluded_skill_path") == 0) o = emit_sku_is_excluded_skill_path(c);
        if (strcmp(op, "sku_yaml_load") == 0) o = emit_sku_yaml_load(c);
        if (strcmp(op, "sku_parse_frontmatter") == 0) o = emit_sku_parse_frontmatter(c);
        if (strcmp(op, "sku_skill_matches_platform_list") == 0) o = emit_sku_skill_matches_platform_list(c);
        if (strcmp(op, "sku_skill_matches_platform") == 0) o = emit_sku_skill_matches_platform(c);
        if (strcmp(op, "sku_u_detect_environment") == 0) o = emit_sku_u_detect_environment(c);
        if (strcmp(op, "sku_skill_matches_environment") == 0) o = emit_sku_skill_matches_environment(c);
        if (strcmp(op, "sku_get_disabled_skill_names") == 0) o = emit_sku_get_disabled_skill_names(c);
        if (strcmp(op, "sku_u_normalize_string_set") == 0) o = emit_sku_u_normalize_string_set(c);
        if (strcmp(op, "sku_u_external_dirs_cache_clear") == 0) o = emit_sku_u_external_dirs_cache_clear(c);
        if (strcmp(op, "sku_get_external_skills_dirs") == 0) o = emit_sku_get_external_skills_dirs(c);
        if (strcmp(op, "sku_get_all_skills_dirs") == 0) o = emit_sku_get_all_skills_dirs(c);
        if (strcmp(op, "sku_normalize_skill_lookup_name") == 0) o = emit_sku_normalize_skill_lookup_name(c);
        if (strcmp(op, "sku_extract_skill_conditions") == 0) o = emit_sku_extract_skill_conditions(c);
        if (strcmp(op, "sku_extract_skill_config_vars") == 0) o = emit_sku_extract_skill_config_vars(c);
        if (strcmp(op, "sku_discover_all_skill_config_vars") == 0) o = emit_sku_discover_all_skill_config_vars(c);
        if (strcmp(op, "sku_u_resolve_dotpath") == 0) o = emit_sku_u_resolve_dotpath(c);
        if (strcmp(op, "sku_resolve_skill_config_values") == 0) o = emit_sku_resolve_skill_config_values(c);
        if (strcmp(op, "sku_u_normalize_skill_description") == 0) o = emit_sku_u_normalize_skill_description(c);
        if (strcmp(op, "sku_extract_skill_description") == 0) o = emit_sku_extract_skill_description(c);
        if (strcmp(op, "sku_is_skill_description_truncated_for_prompt") == 0) o = emit_sku_is_skill_description_truncated_for_prompt(c);
        if (strcmp(op, "sku_iter_skill_index_files") == 0) o = emit_sku_iter_skill_index_files(c);
        if (strcmp(op, "sku_parse_qualified_name") == 0) o = emit_sku_parse_qualified_name(c);
        if (strcmp(op, "sku_is_valid_namespace") == 0) o = emit_sku_is_valid_namespace(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
