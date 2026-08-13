#ifndef HERMES_VERIFY_ENVIRONMENT_H
#define HERMES_VERIFY_ENVIRONMENT_H

/* Faithful C11 port of agent/verify/environment.py (manifest I/O). */

#include "verify_recipes.h"

/* Path of the verify manifest for the project at `root`:
 * <root>/.hermes/environment.json. Returns NULL on allocation failure. */
char *verify_manifest_path(const char *root);

/* Load the saved recipe from the manifest, tolerating malformed files.
 * Mirrors load_manifest: any read/parse/shape problem → NULL. Caller
 * frees the recipe with recipe_free. */
recipe_t *verify_load_manifest(const char *root);

/* Persist `recipe` as the project's verify manifest (versioned wrapper).
 * Returns the manifest path on success, NULL on failure. */
char *verify_save_manifest(const char *root, const recipe_t *recipe);

/* Return (recipe, source) where source is "manifest" or "detected".
 * A saved manifest wins over fresh detection. Writes source into
 * `source_out` (caller-allocated, >=16 bytes), or "detected" when NULL.
 * Returns the recipe (caller frees) or NULL. */
recipe_t *verify_load_or_detect(const char *root, char *source_out);

#endif /* HERMES_VERIFY_ENVIRONMENT_H */
