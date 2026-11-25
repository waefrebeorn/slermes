/*
 * noc_w34_pluginsins_608012.h — C surface for upstream
 * hermes_cli/plugins_install_scan.py (gate w34_pluginsins_608012).
 *
 * The pure decision core of the plugin-guard scanner: how a path classifies
 * (code / python / js / doc prose / CI workflow / locale catalog / test tree /
 * changelog / excluded / bytecode), how a raw ``Finding`` steps down through
 * the exemption and severity-remap tables, the defensive-documentation cap, the
 * main-guard demotion, the structural size limits, and the verdict -> install
 * decision with its exact reason strings.
 *
 * Seams (deliberately not ported; every caller owns them):
 *   - the filesystem: ``_walk``, ``scan_file``, ``_file_lines``, ``stat``,
 *     ``_tracked_bytecode`` (git), symlink resolution;
 *   - the Python AST: ``_is_main_guard``, ``_docstring_lines``,
 *     ``_main_guard_body_lines``, ``_declares_non_utf8_source`` — the line sets
 *     they produce are passed IN to ``filter_findings`` as values;
 *   - the regex zoo in ``plugins_install_scan_context`` used by
 *     ``_inert_context_cap`` / ``is_json_prose_value`` /
 *     ``is_regex_alternation_token`` / ``is_inert_fixture_line`` on code lines /
 *     ``is_quoted_injection_phrase`` / ``is_self_uninstall_doc``.  Those are
 *     pattern-table lookups against ``tools.skills_guard`` regexes; the C side
 *     passes such findings through unchanged instead of guessing.
 */
#ifndef NOC_W34_PLUGINSINS_608012_H
#define NOC_W34_PLUGINSINS_608012_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* rel_path -> every derived classification the scanner decides on. */
json_t *w34_pluginsins_608012_path_class(const char *rel);

/* ScanResult verdict + findings -> {"allowed": true|false|null, "reason": str}. */
json_t *w34_pluginsins_608012_should_allow(const char *verdict, const json_t *findings, int force);

/* "N critical of M findings (id, id)" for a dangerous verdict. */
json_t *w34_pluginsins_608012_dangerous_summary(const json_t *findings);

/* "<name>: <verdict> — N finding(s) in a, b" / "<name>: clean scan, no threats detected". */
json_t *w34_pluginsins_608012_scan_summary(const char *name, const char *verdict, const json_t *findings);

/* {"scanner_version", "verdict", "source"} provenance block. */
json_t *w34_pluginsins_608012_provenance(const char *verdict, const char *source);

/* Severity algebra: lower sev to cap, never raise it. */
json_t *w34_pluginsins_608012_severity_at_most(const char *severity, const char *cap);

/* Whole-line comment / changelog cap (one step, two for non agent-facing). */
json_t *w34_pluginsins_608012_comment_severity(const char *pattern_id, const char *category,
                                              const char *severity);

/* Docstring cap: the comment cap floored at high for a critical. */
json_t *w34_pluginsins_608012_docstring_cap(const char *pattern_id, const char *category,
                                           const char *severity);

/* Documentation prose cap, or null when the shape keeps full severity. */
json_t *w34_pluginsins_608012_prose_cap(const char *pattern_id, const char *category,
                                       const char *severity);

/* Locale-catalog cap (agent-config family -> medium, else the prose cap). */
json_t *w34_pluginsins_608012_catalog_cap(const char *pattern_id, const char *category,
                                         const char *severity);

/* A shape whose prose IS the payload (injection, agent-config edit, key leak). */
int w34_pluginsins_608012_is_agent_facing(const char *pattern_id, const char *category);

/* Whole-line comment / docstring / changelog detection for one finding. */
int w34_pluginsins_608012_is_defensive_documentation(const char *match, const char *rel, int line,
                                                     const json_t *docstring_lines);

/* The per-finding policy pipeline over a whole file:
 * findings + rel + its source lines + the AST line sets -> findings with the
 * post-policy severity (exempt pattern ids dropped, order preserved). */
json_t *w34_pluginsins_608012_filter_findings(const json_t *findings, const char *rel,
                                              const json_t *lines, const json_t *docstring_lines,
                                              const json_t *main_guard_lines);

/* Python's `logical_line`: index plus its trailing-`\` continuations (max 8). */
json_t *w34_pluginsins_608012_logical_line(const json_t *lines, int index);

/* Every module-level constant the policy tables are built from. */
json_t *w34_pluginsins_608012_limits(void);

/* Structural findings, as pure functions of a walked manifest entry. */
json_t *w34_pluginsins_608012_size_finding(const char *rel, double size);
json_t *w34_pluginsins_608012_binary_finding(const char *rel);
json_t *w34_pluginsins_608012_bytecode_finding(const char *rel);
json_t *w34_pluginsins_608012_manifest_findings(double file_count, double total_size);

#ifdef __cplusplus
}
#endif
#endif
