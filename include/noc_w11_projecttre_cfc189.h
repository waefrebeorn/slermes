/* noc_w11_projecttre_cfc189.h — public C API for faithful port of upstream
 * Hermes tui_gateway/project_tree.py (pure path/lane/tree helpers).
 *
 * PoP: w11_projecttre_cfc189_path_key @ tui_gateway/project_tree.py:_path_key
 * PoP: w11_projecttre_cfc189_build_helpers @ tui_gateway/project_tree.py:base_name
 * PoP: w11_projecttre_cfc189_project_node @ tui_gateway/project_tree.py:_project_node
 */
#ifndef NOC_W11_PROJECTTRE_CFC189_H
#define NOC_W11_PROJECTTRE_CFC189_H

#include "hermes_json.h"

json_t *w11_projecttre_cfc189_constants(void);
json_t *w11_projecttre_cfc189_segments(const char *path);
const char *w11_projecttre_cfc189_base_name(const char *path);
const char *w11_projecttre_cfc189_path_key(const char *path);
const char *w11_projecttre_cfc189_lane_key(const char *path_or_lane);
const char *w11_projecttre_cfc189_branch_lane_id(const char *repo_root, const char *branch);
const char *w11_projecttre_cfc189_kanban_lane_id(const char *repo_root);
bool w11_projecttre_cfc189_is_windows_path(const char *path);
json_t *w11_projecttre_cfc189_kanban_worktree_dir(const char *path);
const char *w11_projecttre_cfc189_with_base_name(const char *path, const char *name);
const char *w11_projecttre_cfc189_field(json_t *row, const char *key);
double w11_projecttre_cfc189_session_time(json_t *session);
double w11_projecttre_cfc189_last_active(json_t *sessions);
json_t *w11_projecttre_cfc189_placement(const char *repo_root, const char *lane_key,
                                         const char *lane_label, const char *lane_path,
                                         bool is_main, bool is_kanban, bool is_git);
json_t *w11_projecttre_cfc189_trunk_placement(const char *repo_root, const char *branch);
json_t *w11_projecttre_cfc189_kanban_placement(const char *repo_root, const char *kanban_dir);
json_t *w11_projecttre_cfc189_repo_node(const char *root, const char *label);
json_t *w11_projecttre_cfc189_project_node(const char *pid, const char *label, json_t *path,
                                            json_t *repos, double session_count,
                                            double last_active, json_t *previews,
                                            json_t *rows, json_t *flags);
json_t *w11_projecttre_cfc189_lane_sort_key(json_t *group);
json_t *w11_projecttre_cfc189_disambiguate_labels(json_t *items);
json_t *w11_projecttre_cfc189_stamp_profile(json_t *projects, const char *profile);
json_t *w11_projecttre_cfc189_place_by_heuristic(const char *path);

#endif /* NOC_W11_PROJECTTRE_CFC189_H */
