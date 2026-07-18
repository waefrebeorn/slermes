/*
 * port_learning_graph.h — public API for the agent/learning_graph.py port.
 *
 * Two layers:
 *   - Pure data-transform helpers (JSON in/out) declared here and defined in
 *     port_learning_graph_helpers.c.
 *   - I/O + composition layer (filesystem scan, frontmatter parse, usage load,
 *     top-level build_learning_graph) defined in port_learning_graph.c.
 *
 * Opaque JSON strings are used at the module boundary (callers pass/recv
 * malloc'd JSON); no god-header — only what consumers need.
 */
#ifndef HERMES_PORT_LEARNING_GRAPH_H
#define HERMES_PORT_LEARNING_GRAPH_H

#include <stddef.h>

/* ---- pure helpers (port_learning_graph_helpers.c) ---- */
char *learning_graph_hermes_meta(const char *fm_json);
char *learning_graph_related(const char *fm_json);
char *learning_graph_category(const char *fm_json, const char *fallback);
long long learning_graph_to_int_ts(const char *value_json, int *found);
long long learning_graph_usage_timestamp(const char *rec_json, int *found);
char *learning_graph_tokenize(const char *text);
char *learning_graph_build_edges(const char *nodes_json);
char *learning_graph_density_stats(const char *nodes_json, const char *edges_json);
char *learning_graph_memory_skill_edges(const char *cards_json, const char *skills_json);

/* ---- I/O + composition layer (port_learning_graph.c) ---- */

/* Parse SKILL.md frontmatter text into a malloc'd JSON object (or "{}"). */
char *learning_graph_frontmatter(const char *skill_md_text);

/* List SKILL.md files under a root as malloc'd JSON:
 * [{"source":str,"path":str}, ...]. Skips archive/hub/node_modules/.git. */
char *learning_graph_iter_skill_files(const char *root_json);

/* Load the skill usage map as malloc'd JSON keyed by skill name, or "{}". */
char *learning_graph_load_usage(const char *hermes_home);

/* Build the skill-node JSON map: {"name":{node...}, ...}.
 * skill_roots_json: [{"source":str,"path":str}, ...] (from iter_skill_files
 * over each root). usage_json: map from load_usage. */
char *learning_graph_build_skill_nodes(const char *skill_roots_json,
                                       const char *usage_json);

/* Freeform memory cards from MEMORY.md / USER.md as malloc'd JSON array. */
char *learning_graph_memory_cards(const char *hermes_home);

/* Default skill roots: [{"source":"base","path":<repo>/skills},
 *                        {"source":"profile","path":<home>/skills}]. */
char *learning_graph_skill_roots(const char *hermes_home, const char *repo_skills_dir);

/* Full desktop learning-graph payload (nodes/edges/clusters/memory/stats). */
char *learning_graph_build(const char *hermes_home, const char *repo_skills_dir);

#endif /* HERMES_PORT_LEARNING_GRAPH_H */
