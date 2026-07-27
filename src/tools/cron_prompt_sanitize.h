#ifndef SLERMES_CRON_PROMPT_SANITIZE_H
#define SLERMES_CRON_PROMPT_SANITIZE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * cron_prompt_sanitize.h — cron prompt threat-scanning + unicode surgery.
 *
 * Self-contained module extracted from port_cronjob_tools.c (v551 refactor).
 * No god headers, no void* passthrough. Pure functions over a UTF-8 prompt
 * string + the threat-pattern tables; the opaque context holds those tables.
 *
 * Every public fn has a /* PoP: c @ tools/cronjob_tools.py:_py *\/ annotation
 * in the .c. All serialization goes through the project's json_t (hermes_json).
 */

typedef struct json_t json_t;

/* Opaque context: owns the threat/skill pattern tables + invisible-char set. */
typedef struct cron_prompt_sanitize cron_prompt_sanitize_t;

/* Create a sanitizer context. Returns NULL on allocation failure. */
cron_prompt_sanitize_t *cron_prompt_sanitize_init(void);

/* Free a sanitizer context (safe with NULL). */
void cron_prompt_sanitize_free(cron_prompt_sanitize_t *ctx);

/* Port of tools/cronjob_tools.py:_check_invisible_unicode.
 * Scans prompt (after stripping legitimate emoji ZWJs) for invisible-unicode
 * injection marks. Returns a malloc'd error string (caller frees) on block,
 * or an empty string ("") on pass. Never returns NULL. */
char *cron_prompt_sanitize_check_invisible(const char *prompt);

/* Port of tools/cronjob_tools.py:_strip_invisible_unicode.
 * Strips invisible-unicode codepoints (preserving legitimate emoji ZWJs).
 * Returns a json_t* {cleaned:str, removed:["U+XXXX",...]} (caller frees with
 * json_free). Returns NULL only on allocation failure. */
json_t *cron_prompt_sanitize_strip_invisible(const char *prompt);

/* Port of tools/cronjob_tools.py:_scan_cron_skill_assembled.
 * Looser scan for already-vetted skill content: invisible unicode is
 * SANITIZED (not blocked); only unambiguous injection directives are blocked.
 * Returns a json_t* {cleaned:str, error:str} (caller frees). Returns NULL only
 * on allocation failure. */
json_t *cron_prompt_sanitize_scan_skill_assembled(const char *assembled);

/* Port of tools/cronjob_tools.py:_scan_cron_prompt.
 * STRICT scan of the raw user-supplied cron prompt (create/update time).
 * Returns malloc'd error string when blocked, malloc'd "" when clean. */
char *cron_prompt_sanitize_scan_prompt(const char *prompt);

#endif /* SLERMES_CRON_PROMPT_SANITIZE_H */
