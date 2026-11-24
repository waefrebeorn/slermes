#ifndef NOC_W21_SKILLSGUAR_8A9CFB_H
#define NOC_W21_SKILLSGUAR_8A9CFB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/skills_guard_credentials.py (gate w21_skillsguar_8a9cfb).
 *
 * The module is a leaf: two regex-source builders, the compiled dotenv pattern
 * (`SKILL_ONLY_PATTERNS[0]`), the env-read name pattern, and the finding
 * reclassification that turns a provider-key read into a high finding. The
 * provider-name set (`model_provider_key_names` -> PROVIDER_REGISTRY, config,
 * the child-env blocklist) is the config seam and arrives as an argument.
 *
 * Text is matched on the same view the scanner hands the regex: for a .py file
 * that is `code_views`' `view` line (comments blanked, parens in string
 * literals blanked), plus its `in_text` mask as a string of '0'/'1' bytes.
 */

/* PoP: w21_skillsguar_8a9cfb_pattern_source @ tools/skills_guard_credentials.py:_closed
 * {"closed": _closed(depth), "open_prefix": _open_prefix(depth)} -- the exact
 * regex text the module builds for that depth (1..4 are the live depths). */
json_t *w21_skillsguar_8a9cfb_pattern_source(int depth);

/* PoP: w21_skillsguar_8a9cfb_dotenv_match @ tools/skills_guard_credentials.py:SKILL_ONLY_PATTERNS
 * 1 when `SKILL_ONLY_PATTERNS[0][0].search(view)` finds a Hermes .env load:
 * a `load_dotenv(` / `dotenv_values(` call whose own parenthesised argument
 * span (at most 4 levels deep) names HERMES_HOME, get_hermes_home, or a
 * `~/.hermes` / `/...` / `\...` / quoted `.hermes` path segment. */
int w21_skillsguar_8a9cfb_dotenv_match(const char *view);

/* PoP: w21_skillsguar_8a9cfb_env_read_names @ tools/skills_guard_credentials.py:_ENV_READ_NAME_RE
 * The first argument of every `getenv(...)`, `environ.get(...)` and
 * `environ[...]` read in the line, in non-overlapping match order. */
json_t *w21_skillsguar_8a9cfb_env_read_names(const char *line);

/* PoP: w21_skillsguar_8a9cfb_raise_reads @ tools/skills_guard_credentials.py:raise_provider_key_reads
 * findings: [{"pattern_id","line","severity","description"}]; lines: the file's
 * lines; providers: the upper-cased provider key names; in_text: mask lines or
 * null (a non-.py file: the whole line counts). Returns the findings in order,
 * with each provider-key read reclassified to
 * provider_key_env_read/high and re-described. */
json_t *w21_skillsguar_8a9cfb_raise_reads(const json_t *findings, const json_t *lines,
                                          const json_t *providers, const json_t *in_text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_SKILLSGUAR_8A9CFB_H */
