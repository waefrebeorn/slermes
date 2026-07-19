/*
 * project_tree_helpers.h — public API for the pure tui_gateway/project_tree.py
 * path/lane-id helpers. Opaque, minimal includes.
 */

#ifndef PROJECT_TREE_HELPERS_H
#define PROJECT_TREE_HELPERS_H

#include <stddef.h>

/* Split a path on / or \, drop trailing separators + empties.
 * (PoP: project_tree._segments) */
char **project_tree_segments(const char *path, int *out_n);
void project_tree_free_segments(char **segs, int n);

/* Last path segment, or "" when empty. (PoP: project_tree.base_name) */
void project_tree_base_name(const char *path, char *out, size_t out_cap);

/* If path is a .../.worktrees/t_<hex> dir, return the "<repo>/.worktrees" dir
 * (malloc'd), else NULL. (PoP: project_tree.kanban_worktree_dir) */
char *project_tree_kanban_worktree_dir(const char *path);

/* True when target == folder or is nested under folder (segment-wise).
 * (PoP: project_tree._is_path_under) */
int project_tree_is_path_under(const char *folder, const char *target);

/* Replace the last path segment with `name`; returns malloc'd string.
 * (PoP: project_tree._with_base_name) */
char *project_tree_with_base_name(const char *path, const char *name);

/* f"{repo_root}::branch::{branch.strip()}" — returns malloc'd string.
 * (PoP: project_tree._branch_lane_id) */
char *project_tree_branch_lane_id(const char *repo_root, const char *branch);

/* f"{repo_root}::kanban" — returns malloc'd string.
 * (PoP: project_tree._kanban_lane_id) */
char *project_tree_kanban_lane_id(const char *repo_root);

#endif /* PROJECT_TREE_HELPERS_H */
