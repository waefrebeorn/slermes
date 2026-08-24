/*
 * agent_skill_utils_ports.h — public surface for the C11 ports of the
 * pure helpers in Python agent/skill_utils.py (project-skill tier).
 */
#ifndef SLERMES_AGENT_SKILL_UTILS_PORTS_H
#define SLERMES_AGENT_SKILL_UTILS_PORTS_H

#include <stdbool.h>
#include "hermes_json.h"

/* PoP: parse_config_string_list @ agent/skill_utils.py:parse_config_string_list */
json_t *parse_config_string_list(const json_t *value);
/* PoP: find_project_root @ agent/skill_utils.py:find_project_root */
char *find_project_root(const char *start);   /* caller frees; NULL if none */
/* PoP: is_project_root_trusted @ agent/skill_utils.py:is_project_root_trusted */
bool is_project_root_trusted(const char *root);
/* PoP: get_project_skills_dirs @ agent/skill_utils.py:get_project_skills_dirs */
json_t *get_project_skills_dirs(void);
/* PoP: get_untrusted_project_skills_root @ agent/skill_utils.py:get_untrusted_project_skills_root */
bool get_untrusted_project_skills_root(char **out_root, int *out_count);
/* PoP: get_scan_ordered_skills_dirs @ agent/skill_utils.py:get_scan_ordered_skills_dirs */
json_t *get_scan_ordered_skills_dirs(void);
/* PoP: iter_skill_index_files @ agent/skill_utils.py:iter_skill_index_files */
json_t *sku_iter_skill_index_files(const char *skills_dir, const char *filename);
/* PoP: is_quarantined_project_skill @ agent/skill_utils.py:is_quarantined_project_skill */
bool is_quarantined_project_skill(const char *skill_md);
/* PoP: iter_project_skill_files @ agent/skill_utils.py:iter_project_skill_files */
json_t *iter_project_skill_files(const char *project_dir);

#endif /* SLERMES_AGENT_SKILL_UTILS_PORTS_H */
