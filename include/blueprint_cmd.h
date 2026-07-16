/*
 * blueprint_cmd.h — /blueprint command resolution helpers (PoP port).
 *
 * Self-contained port of hermes_cli/blueprint_cmd.py: the forgiving
 * blueprint-name resolution used by the CLI / TUI / gateway /blueprint
 * command. It operates on a caller-supplied catalog (JSON array) so the
 * module stays free of cron/blueprint_catalog internals and of any job
 * engine — the caller owns job creation.
 *
 * Reuses libdifflib (difflib_ratio) for the fuzzy typo-tolerance pass that
 * mirrors Python's difflib.get_close_matches.
 *
 * Minimal includes: <stddef.h> only. The catalog is opaque.
 */
#ifndef HERMES_BLUEPRINT_CMD_H
#define HERMES_BLUEPRINT_CMD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque catalog of blueprints loaded from a JSON array. */
typedef struct blueprint_catalog blueprint_catalog_t;

/* Load a catalog from a JSON array string:
 *   [{"key":..,"title":..,"description":..,
 *     "slots":[{"name":..,"label":..,"type":..,"default":..,
 *               "options":[..],"optional":bool,"help":..}], ...}, ...]
 * Returns NULL on parse failure. Caller frees with blueprint_catalog_free(). */
blueprint_catalog_t *blueprint_catalog_load_json(const char *catalog_json);
void blueprint_catalog_free(blueprint_catalog_t *cat);

int  blueprint_catalog_count(const blueprint_catalog_t *cat);
/* Fill key/title/description (malloc'd, caller frees) for entry i.
 * Any of key/title/description may be NULL to skip. Returns 0 if i in range. */
int  blueprint_catalog_at(const blueprint_catalog_t *cat, int i,
                          char **key, char **title, char **description);

/* Quote-aware split of "slot=value …" tokens (shlex-like). Returns 0 on ok.
 * out_keys/out_vals are malloc'd arrays of malloc'd strings (n entries);
 * out_leftovers are bare tokens (also malloc'd strings). Caller frees each
 * string and the arrays. Any out_* may be NULL to skip; *out_n / *out_leftover_n
 * are set to 0 when NULL is passed. */
int blueprint_cmd_parse_kv(const char *args,
                           char ***out_keys, char ***out_vals, int *out_n,
                           char ***out_leftovers, int *out_leftover_n);

/* Resolve a free-typed query to a blueprint (exact -> prefix -> substring ->
 * fuzzy, mirroring hermes_cli/blueprint_cmd.py:match_blueprint).
 *   out_matched_key     malloc'd key of the matched blueprint, or NULL
 *   out_candidates      malloc'd array of malloc'd candidate keys, or NULL
 *   out_ncand           number of candidates
 * Returns 1 if exactly matched, 0 otherwise (candidates may still be set for
 * the ambiguous case; empty candidates => no plausible match). */
int blueprint_cmd_match(const blueprint_catalog_t *cat, const char *query,
                        char **out_matched_key,
                        char ***out_candidates, int *out_ncand);

/* Formatters (malloc'd strings, caller frees). */
char *blueprint_cmd_format_catalog(const blueprint_catalog_t *cat);
char *blueprint_cmd_format_candidates(const blueprint_catalog_t *cat, const char *query);
char *blueprint_cmd_format_no_match(const blueprint_catalog_t *cat, const char *query);

/* Build the agent-seed prompt for a matched blueprint (by key). Returns a
 * malloc'd string, or NULL if the key is not found. */
char *blueprint_cmd_build_seed(const blueprint_catalog_t *cat, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_BLUEPRINT_CMD_H */
