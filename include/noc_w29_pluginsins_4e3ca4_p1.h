/*
 * noc_w29_pluginsins_4e3ca4_p1.h - C surface for upstream
 * hermes_cli/plugins_install_scan_context.py (gate w29_pluginsins_4e3ca4_p1).
 *
 * Upstream demotes install-scan findings that live in INERT context (docs,
 * CI workflows, locale catalogs, test trees, JSON prose, build-script repo-root
 * climbs, `pip install` sentences, `\xHH` inside a char class).  Every helper
 * is a pure predicate on (finding, line, path).
 *
 * SEAMS
 *  - The filesystem is not read: ``rel_path`` is a posix-style relative path
 *    handed in by the caller (fixture).
 *  - A ``Finding`` is reduced to the three fields the predicates read:
 *    ``pattern_id``, ``severity``, ``category``.  The compiled threat regex
 *    table (``tools.skills_guard._COMPILED_THREAT_PATTERNS``) is the one
 *    external dependency; the port carries the matching subset of those
 *    pattern texts (see the PATTERN TABLE section of the .c) and locates a
 *    finding's token with the same leftmost-first semantics.
 *
 * Gate p1 partition: path/doc classification, prose caps, the documented
 * install one-liner, test-tree naming, JSON prose values, repo-root climbs,
 * logical lines.
 */
#ifndef NOC_W29_PLUGINSINS_4E3CA4_P1_H
#define NOC_W29_PLUGINSINS_4E3CA4_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The three Finding fields these predicates consult. Strings are borrowed. */
typedef struct {
    const char *pattern_id;
    const char *severity;
    const char *category;
} noc_finding_t;

/* is_doc_prose(rel_path) - a .md/.txt/.rst/.html doc outside a skill tree. */
int noc_pis_p1_is_doc_prose(const char *rel_path);

/* is_ci_workflow(rel_path) - a .github/workflows yml/yaml or .gitlab-ci.yml. */
int noc_pis_p1_is_ci_workflow(const char *rel_path);

/* is_agent_facing(finding) - a shape whose prose IS the payload. */
int noc_pis_p1_is_agent_facing(const noc_finding_t *f);

/* prose_cap(finding) - stepped-down severity, or NULL for "no cap". */
const char *noc_pis_p1_prose_cap(const noc_finding_t *f);

/* is_trusted_installer_pipe(finding, line) - curl|sh to a named installer. */
int noc_pis_p1_is_trusted_installer_pipe(const noc_finding_t *f, const char *line);

/* is_locale_catalog(rel_path) - locales/<name>.yaml at the plugin root. */
int noc_pis_p1_is_locale_catalog(const char *rel_path);

/* catalog_cap(finding) - catalog severity cap, or NULL for "no cap". */
const char *noc_pis_p1_catalog_cap(const noc_finding_t *f);

/* is_self_uninstall_doc(finding, line) - `rm` of the plugin's own ~/.hermes dir. */
int noc_pis_p1_is_self_uninstall_doc(const noc_finding_t *f, const char *line);

/* is_test_tree(rel_path) - a test/fixture tree, dunder dir or test-file name. */
int noc_pis_p1_is_test_tree(const char *rel_path);

/* is_disclosed_config_note(finding, rel_path) - prose config write in install docs. */
int noc_pis_p1_is_disclosed_config_note(const noc_finding_t *f, const char *rel_path);

/* is_json_prose_value(finding, rel_path, line) - mid-sentence string in a .json data file. */
int noc_pis_p1_is_json_prose_value(const noc_finding_t *f, const char *rel_path, const char *line);

/* is_repo_root_traversal(finding, line) - ../../../ climb with no sensitive leaf. */
int noc_pis_p1_is_repo_root_traversal(const noc_finding_t *f, const char *line);

/* is_pip_install_in_prose_literal(finding, line) - "pip install" mid-sentence in a literal. */
int noc_pis_p1_is_pip_install_in_prose_literal(const noc_finding_t *f, const char *line);

/* is_hex_in_char_class(finding, line) - every \xHH inside a [...] class. */
int noc_pis_p1_is_hex_in_char_class(const noc_finding_t *f, const char *line);

/* logical_line(lines, index) - the line joined with its backslash continuations. */
json_t *noc_pis_p1_logical_line(const json_t *lines, int index);

#ifdef __cplusplus
}
#endif
#endif
