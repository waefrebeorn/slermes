/* skill_linter.h — structural + convention linter for SKILL.md files.
 * Cohesive C11 port of tools/skill_linter.py. See AGENTS.md for the
 * advisory-only contract. Oracle-verified against live Python.
 */
#ifndef SLERMES_SKILL_LINTER_H
#define SLERMES_SKILL_LINTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINTER_MAX_RULE 64
#define LINTER_MAX_MSG  448

typedef struct {
    const char *severity;   /* "error" or "warning" */
    char       rule[LINTER_MAX_RULE];
    char       message[LINTER_MAX_MSG];
} lint_finding_t;

/* Render a finding as "badge [rule] message" (badge = ✗ for error, ⚠ for
 * warning — matching Python's LintFinding.format). */
void lint_finding_format(const lint_finding_t *f, char *out, size_t out_sz);

/* True if any finding is ERROR severity. */
int lint_has_errors(const lint_finding_t *findings, size_t n);

/* Lint raw SKILL.md *content*. Pass skill_dir (absolute) to enable on-disk
 * checks (name/dir match, dangling refs, POSIX gating, forbidden files).
 * Appends findings to *out (up to *cap). Returns the count appended. */
size_t lint_content(const char *content, const char *skill_dir,
                    lint_finding_t *out, size_t cap);

/* Like lint_content, but reads the file at skill_md_path and lints with its
 * parent directory as skill_dir. Returns count, or 0 on read failure. */
/* PoP: lint_skill @ tools/skill_linter.py:lint_skill */
size_t lint_skill(const char *skill_md_path, lint_finding_t *out, size_t cap);

/* Render findings as a newline-joined human-readable block. Returns the
 * number of bytes written into *out* (truncated to out_sz). */
/* PoP: format_findings @ tools/skill_linter.py:format_findings */
size_t lint_format_findings(const lint_finding_t *findings, size_t n,
                            char *out, size_t out_sz);

/* CLI entry: lint SKILL.md files/dirs passed as argv. Mirrors
 * ``python -m tools.skill_linter``. Returns 0 if no errors, 1 if any
 * finding is ERROR severity, 2 on usage error. */
/* PoP: _main @ tools/skill_linter.py:_main */
int skill_linter_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_SKILL_LINTER_H */
