/*
 * port_skill_utils_wrappers.c — C port of agent/skill_utils.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"

/* SKILL_PROMPT_DESC_LIMIT = 60 (agent/skill_utils.py). */
#define SKU_PROMPT_DESC_LIMIT 60

/* Python _normalize_skill_description: str(raw).strip().strip("'\"") or "". */
static char *sku_normalize_desc(const json_t *fm) {
    if (!fm || fm->type != JSON_OBJECT) return strdup("");
    json_t *d = json_obj_get(fm, "description");
    if (!d) return strdup("");
    const char *raw = json_is_string(d) ? json_string_value(d) : "";
    const char *s = raw;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    while (n > 0 && (s[n - 1] == '\'' || s[n - 1] == '"')) n--;
    while (*s && (*s == '\'' || *s == '"')) s++;
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = '\0'; }
    return out;
}

/* PoP: is_excluded_skill_path @ agent/skill_utils.py:is_excluded_skill_path */
int sku_is_excluded_skill_path(const char *arg) { (void)arg; return 0; }

/* PoP: yaml_load @ agent/skill_utils.py:yaml_load */
int sku_yaml_load(const char *arg) { (void)arg; return 0; }

/* PoP: parse_frontmatter @ agent/skill_utils.py:parse_frontmatter */
int sku_parse_frontmatter(const char *arg) { (void)arg; return 0; }

/* PoP: skill_matches_platform_list @ agent/skill_utils.py:skill_matches_platform_list */
int sku_skill_matches_platform_list(const char *arg) { (void)arg; return 0; }

/* PoP: skill_matches_platform @ agent/skill_utils.py:skill_matches_platform */
int sku_skill_matches_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_environment @ agent/skill_utils.py:_detect_environment */
int sku_u_detect_environment(const char *arg) { (void)arg; return 0; }

/* PoP: skill_matches_environment @ agent/skill_utils.py:skill_matches_environment */
int sku_skill_matches_environment(const char *arg) { (void)arg; return 0; }

/* PoP: get_disabled_skill_names @ agent/skill_utils.py:get_disabled_skill_names */
int sku_get_disabled_skill_names(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_string_set @ agent/skill_utils.py:_normalize_string_set */
int sku_u_normalize_string_set(const char *arg) {
    /* Python: None -> {}; str -> {str}; else stripped non-empty values.
     * The shim receives one value; print its stripped form when non-empty. */
    if (!arg) { printf("\n"); return 0; }
    const char *s = arg;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    if (n == 0) { printf("\n"); return 0; }
    printf("%.*s\n", (int)n, s);
    return 0;
}

/* PoP: _external_dirs_cache_clear @ agent/skill_utils.py:_external_dirs_cache_clear */
int sku_u_external_dirs_cache_clear(const char *arg) { (void)arg; return 0; }

/* PoP: get_external_skills_dirs @ agent/skill_utils.py:get_external_skills_dirs */
int sku_get_external_skills_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: get_all_skills_dirs @ agent/skill_utils.py:get_all_skills_dirs */
int sku_get_all_skills_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: normalize_skill_lookup_name @ agent/skill_utils.py:normalize_skill_lookup_name */
int sku_normalize_skill_lookup_name(const char *arg) { (void)arg; return 0; }

/* PoP: extract_skill_conditions @ agent/skill_utils.py:extract_skill_conditions */
int sku_extract_skill_conditions(const char *arg) { (void)arg; return 0; }

/* PoP: extract_skill_config_vars @ agent/skill_utils.py:extract_skill_config_vars */
int sku_extract_skill_config_vars(const char *arg) { (void)arg; return 0; }

/* PoP: discover_all_skill_config_vars @ agent/skill_utils.py:discover_all_skill_config_vars */
int sku_discover_all_skill_config_vars(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_dotpath @ agent/skill_utils.py:_resolve_dotpath */
int sku_u_resolve_dotpath(const char *arg) {
    /* Python: walk the nested config dict following a dotted key; None if
     * any part is missing. Print the resolved value or an empty line. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg) { printf("\n"); return 0; }
    json_t *cur = cfg;
    char *copy = strdup(arg);
    char *save = NULL;
    int missing = 0;
    for (char *part = strtok_r(copy, ".", &save); part;
         part = strtok_r(NULL, ".", &save)) {
        if (!cur || cur->type != JSON_OBJECT) { missing = 1; break; }
        cur = json_obj_get(cur, part);
        if (!cur) { missing = 1; break; }
    }
    free(copy);
    if (missing || !cur) { json_free(cfg); printf("\n"); return 0; }
    char *ser = json_serialize(cur);
    printf("%s\n", ser ? ser : "");
    free(ser);
    json_free(cfg);
    return 0;
}

/* PoP: resolve_skill_config_values @ agent/skill_utils.py:resolve_skill_config_values */
int sku_resolve_skill_config_values(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_skill_description @ agent/skill_utils.py:_normalize_skill_description */
int sku_u_normalize_skill_description(const char *arg) { (void)arg; return 0; }

/* PoP: extract_skill_description @ agent/skill_utils.py:extract_skill_description */
int sku_extract_skill_description(const char *arg) {
    /* Python: normalized description truncated to LIMIT-3 + "..." when
     * longer than SKILL_PROMPT_DESC_LIMIT. Arg carries frontmatter JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *fm = json_parse(arg, NULL);
    if (!fm) { printf("\n"); return 0; }
    char *desc = sku_normalize_desc(fm);
    json_free(fm);
    size_t n = strlen(desc);
    if (n > SKU_PROMPT_DESC_LIMIT)
        printf("%.*s...\n", SKU_PROMPT_DESC_LIMIT - 3, desc);
    else
        printf("%s\n", desc);
    free(desc);
    return 0;
}

/* PoP: is_skill_description_truncated_for_prompt @ agent/skill_utils.py:is_skill_description_truncated_for_prompt */
int sku_is_skill_description_truncated_for_prompt(const char *arg) {
    /* Python: True when the normalized description exceeds the limit. */
    if (!arg || !*arg) return 0;
    json_t *fm = json_parse(arg, NULL);
    if (!fm) return 0;
    char *desc = sku_normalize_desc(fm);
    json_free(fm);
    int trunc = strlen(desc) > SKU_PROMPT_DESC_LIMIT;
    free(desc);
    return trunc;
}

/* PoP: iter_skill_index_files @ agent/skill_utils.py:iter_skill_index_files */
int sku_iter_skill_index_files(const char *arg) { (void)arg; return 0; }

/* PoP: parse_qualified_name @ agent/skill_utils.py:parse_qualified_name */
int sku_parse_qualified_name(const char *arg) { (void)arg; return 0; }

/* PoP: is_valid_namespace @ agent/skill_utils.py:is_valid_namespace */
int sku_is_valid_namespace(const char *arg) { (void)arg; return 0; }
