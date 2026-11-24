#ifndef NOC_W22_FINDER_213FAA_H
#define NOC_W22_FINDER_213FAA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_agent/compat/finder.py (gate w22_finder_213faa).
 *
 * The portable half of the module is the name algebra: which current name an
 * old name stands for, and which derived child names "from <old package>
 * import <child>" asks for. The finder machinery itself (the meta-path finder,
 * the alias loader/spec, importlib.PathFinder probes) is the live import system
 * and stays in Python.
 */

/* PoP: w22_finder_213faa_current_name @ hermes_agent/compat/finder.py:current_name
 * The current name for `name` by its longest dotted prefix that is a key of
 * `moves`, as moves[prefix] + name[len(prefix):]. NULL when no prefix matches,
 * and NULL when the matched value is not a string (upstream raises TypeError
 * on `value + suffix`). Caller frees. */
char *w22_finder_213faa_current_name(const char *name, const json_t *moves);

/* PoP: w22_finder_213faa_child_names @ hermes_agent/compat/finder.py:_child_names
 * {"<moved_parent>.<child>": new} for every move whose old parent itself moved
 * apart from its child, in `moves` insertion order, skipping entries whose
 * derived name equals the move's own new name or is already a key of `moves`.
 * {} for a non-object `moves`. */
json_t *w22_finder_213faa_child_names(const json_t *moves);

/* PoP: w22_finder_213faa_strict_error @ hermes_agent/compat/finder.py:Finder.find_spec
 * The strict finder's ImportError text, "<fullname> has moved: import <new>",
 * or NULL when the finder would not claim the name. The PathFinder existence
 * guard that precedes both the derived-name branch and the answer is the
 * filesystem seam and stays upstream, so this answer assumes no real module of
 * the looked-up name exists (the same thing the pure name algebra decides).
 * Caller frees. */
char *w22_finder_213faa_strict_error(const char *name, const json_t *moves);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_FINDER_213FAA_H */
