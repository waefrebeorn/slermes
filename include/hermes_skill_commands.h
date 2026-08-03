/*
 * hermes_skill_commands.h — Skill slash-command support for Hermes C.
 *
 * Port of Python agent/skill_commands.py: scan skills dir, build /slug
 * mapping, resolve user input, build formatted invocation messages.
 */
#ifndef HERMES_SKILL_COMMANDS_H
#define HERMES_SKILL_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Types
 * ================================================================ */

#define SKILL_CMD_NAME_MAX    64
#define SKILL_CMD_DESC_MAX    512
#define SKILL_CMD_PATH_MAX    4096
#define SKILL_CMD_SLUG_MAX    128
#define MAX_SKILL_COMMANDS    64   /* max /slug commands (was 256: 256×17KB skill_cmd_entry_t ≈ 4.3MB .bss) */

/* A single skill command entry (/slug → info) */
typedef struct {
    char slug[SKILL_CMD_SLUG_MAX];       /* "/slug-name" */
    char name[SKILL_CMD_NAME_MAX];        /* original display name from frontmatter */
    char description[SKILL_CMD_DESC_MAX]; /* frontmatter description */
    char skill_path[SKILL_CMD_PATH_MAX];  /* absolute path to skill dir */
    /* SK08: Setup notes from frontmatter */
    bool setup_skipped;                   /* "setup_skipped: true" */
    char gateway_setup_hint[SKILL_CMD_DESC_MAX]; /* "gateway_setup_hint" value */
    bool setup_needed;                    /* "setup_needed: true" */
    char setup_note[SKILL_CMD_DESC_MAX];  /* "setup_note" value */
    /* SK07: Config variable injection from config.yaml */
    int  config_var_count;                /* number of config vars declared */
    struct {
        char key[128];                    /* logical key, e.g. "wiki.path" */
        char description[256];            /* human-readable description */
        char default_val[512];            /* default value */
        char resolved[512];               /* resolved value from config.yaml */
    } config_vars[8];                     /* max 8 config vars per skill */
} skill_cmd_entry_t;

/* Skill payload returned by load_skill_payload() */
typedef struct {
    char *skill_name;     /* display name */
    char *skill_dir;      /* absolute path to skill dir */
    char *frontmatter;    /* raw frontmatter text */
    char *body;           /* SKILL.md body without frontmatter */
} skill_cmd_payload_t;

/* Free a skill payload returned by load_skill_payload() */
void skill_payload_free(skill_cmd_payload_t *p);

/* Load a skill by name/path and return payload (NULL on failure). */
skill_cmd_payload_t *load_skill_payload(const char *skill_identifier);

/* ================================================================
 *  Public API
 * ================================================================ */

/* Scan ~/.hermes/skills/ and rebuild the /slug cache.
 * Returns the number of skill commands found (0 if none or error). */
int skill_cmd_scan(void);

/* Get a skill command entry by /slug key (including leading slash).
 * Returns pointer to internal entry, or NULL if not found. */
const skill_cmd_entry_t *skill_cmd_get(const char *slug);

/* Get all cached skill commands. Sets *count to number of entries.
 * Returns a heap array of pointers to live entries (caller frees with
 * free()); NULL + count 0 when empty. Entries stay valid until the next
 * scan invalidates the cache. */
const skill_cmd_entry_t **skill_cmd_get_all(int *count);

/* Resolve a user-typed /command to its canonical /slug key.
 * Normalizes underscores to hyphens, case-insensitive.
 * Returns pointer to internal slug string, or NULL if no match. */
const char *skill_cmd_resolve(const char *command);

/* Build a formatted skill invocation message for the agent.
 * Loads the SKILL.md, strips frontmatter, formats with activation
 * note, skill directory, and supporting file references.
 *
 * Returns malloc'd string (caller must free) or NULL on error. */
char *skill_cmd_build_message(const char *slug, const char *user_args);

/* Re-scan skills dir and return number of changes (additions + removals).
 * Sets *added to number of new skills, *removed to number of removed.
 * Pass NULL for added/removed if not needed. */
int skill_cmd_rescan(int *added, int *removed);

/* Check if a skill slug is in a comma-separated disabled list.
 * Matches against both slug (/slug-name) and bare name.
 * Returns true if the skill is disabled. */
bool skill_cmd_is_disabled(const char *slug, const char *disabled_csv);

/* Scan skills dir, filtering out entries that match the disabled CSV list.
 * Returns the count of active (non-disabled) skills found.
 * Caller is responsible for passing the correct disabled list
 * (platform-aware filtering can be implemented at the call site). */
int skill_cmd_scan_filtered(const char *disabled_csv);

/* SK06: Platform-scoped skill cache invalidation.
 * Set the current platform scope; invalidates cache if platform changed.
 * Called by gateway when session platform changes. */
void skill_cmd_set_platform(const char *platform);

/* SK06: Explicitly invalidate the platform-scoped skill cache.
 * Forces a full re-scan on next skill_cmd_scan(). */
void skill_cmd_invalidate_platform_cache(void);

/* Load one or more skills for session-wide CLI preloading.
 * Accepts comma/space-separated list of skill names or slugs.
 * out_prompt: malloc'd combined prompt text (caller free, optional)
 * out_loaded: malloc'd comma-separated loaded skill names (caller free, optional)
 * out_missing: malloc'd comma-separated missing identifiers (caller free, optional)
 * Returns number of skills successfully loaded.
 * Port of Python skill_commands.py:build_preloaded_skills_prompt(). */
int build_preloaded_skills_prompt(const char *skill_identifiers,
                                     char **out_prompt,
                                     char **out_loaded,
                                     char **out_missing);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SKILL_COMMANDS_H */
