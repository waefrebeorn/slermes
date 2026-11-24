#ifndef NOC_W22_PATHS_5CE33E_H
#define NOC_W22_PATHS_5CE33E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_agent/compat/paths.py (gate w22_paths_5ce33e).
 *
 * Repo paths through the same MOVES table the import finder reads:
 *
 *   current_path      -- where a pre-move path lives now
 *   original_path     -- what a file's path was before any move
 *   current_module    -- the current dotted name of a moved module
 *   original_module   -- the oldest dotted name of a current module
 *   current_globs     -- a glob over old paths, plus the patterns that find
 *                        its moved matches
 *
 * `moves_from_source` (the AST read of a tree's moves.py) and `tree_moves`
 * (a cached read of another checkout) are the source/file seam: they stay
 * upstream, and every function here takes the resolved table as a JSON object
 * (old dotted name -> current dotted name). An empty or absent table is the
 * identity for paths, exactly the `MOVES`-empty case.
 */

/* PoP: w22_paths_5ce33e_directory @ hermes_agent/compat/paths.py:_directory
 * `_directory(path, table)`: a slash path whose every segment is non-empty and
 * dot-free is mapped as a dotted module name; anything else is left alone. */
json_t *w22_paths_5ce33e_directory(const json_t *table, const char *path);

/* PoP: w22_paths_5ce33e_current @ hermes_agent/compat/paths.py:current_path
 * `PathMoves.current` / `_map`: a trailing "/" maps the directory; a
 * `__init__.py` maps its package; a ".py" file maps its module; another dotted
 * last segment maps the head only; everything else maps as a directory. */
json_t *w22_paths_5ce33e_current(const json_t *table, const char *path);

/* PoP: w22_paths_5ce33e_original @ hermes_agent/compat/paths.py:original_path
 * `PathMoves.original`: the path stepped back through the inverse table until
 * it stops changing (at most 8 steps), so a module moved twice comes back to
 * its oldest name. */
json_t *w22_paths_5ce33e_original(const json_t *table, const char *path);

/* PoP: w22_paths_5ce33e_module @ hermes_agent/compat/paths.py:current_module
 * `current_name(name, table) or name`: the longest dotted prefix that is a key
 * of the table wins, with the rest of the name appended. */
json_t *w22_paths_5ce33e_module(const json_t *table, const char *name);

/* PoP: w22_paths_5ce33e_original_module @ hermes_agent/compat/paths.py:original_module
 * The same undo walk over dotted names with the inverse table. */
json_t *w22_paths_5ce33e_original_module(const json_t *table, const char *name);

/* PoP: w22_paths_5ce33e_current_globs @ hermes_agent/compat/paths.py:current_globs
 * A JSON array of patterns, the input pattern first and the rest sorted. With
 * no table, or a pattern without any of `*?[`, it is just `current(pattern)`.
 * Matching is fnmatch: case-insensitive on this platform, `*` matches across
 * "/", `?` one character, `[seq]` a class. */
json_t *w22_paths_5ce33e_current_globs(const json_t *table, const char *pattern);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_PATHS_5CE33E_H */
