#ifndef NOC_W21_CONTEXT_1BF504_H
#define NOC_W21_CONTEXT_1BF504_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/core/context.py (gate w21_context_1bf504).
 *
 * `ProfileScope` names the one profile a store handle is bound to. Its
 * `profile_dir` is resolved once, when the scope is built, by the
 * `hermes_constants.hermes_home_key` rule: `Path(path).expanduser()` then
 * `resolve(strict=True)` with a lenient `resolve(strict=False)` fallback for a
 * folder not on disk yet. `hermes_home_key` is that folder's `normcase`, and
 * equality uses that key only. `name` is a label: it never selects storage and
 * is not part of equality.
 *
 * The three path steps are ported in full (expanduser, resolve, normcase); no
 * seam is stubbed, because none of them reaches a database, network, config or
 * clock surface. */

/* PoP: w21_context_1bf504_expand_user @ persistence/core/context.py:_canonical_dir
 * `Path(path).expanduser()`: a leading `~` (alone, or before a separator) is
 * replaced by HOME. A `~user` form is left untouched, as pathlib does on
 * Windows. Caller frees. */
char *w21_context_1bf504_expand_user(const char *path);

/* PoP: w21_context_1bf504_canonical_dir @ persistence/core/context.py:_canonical_dir
 * `expanduser()` then `resolve(strict=True)`, falling back to
 * `resolve(strict=False)` on OSError: the absolute path with `.`/`..` folded
 * and the on-disk spelling (case, symlink target) of every existing component.
 * A trailing separator is dropped except at a drive root. Caller frees. */
char *w21_context_1bf504_canonical_dir(const char *path);

/* PoP: w21_context_1bf504_canonical_home_key @ persistence/core/context.py:canonical_home_key
 * `os.path.normcase(str(_canonical_dir(path)))`: lowercase ASCII and forward
 * slashes folded to backslashes on this host. Caller frees. */
char *w21_context_1bf504_canonical_home_key(const char *path);

/* PoP: w21_context_1bf504_scope @ persistence/core/context.py:ProfileScope.__post_init__
 * Build a scope from its two given fields:
 *   {"status":"ok","profile_dir":<resolved>,"name":<label>,
 *    "hermes_home_key":<normcase>}
 * or {"status":"error","type":"ValueError","message":...} for an empty
 * profile_dir ("ProfileScope.profile_dir must be non-empty") or an empty name
 * ("ProfileScope.name must be non-empty"). */
json_t *w21_context_1bf504_scope(const char *profile_dir, const char *name);

/* PoP: w21_context_1bf504_scope_equal @ persistence/core/context.py:ProfileScope.__eq__
 * Equality of two scopes built from the given fields: the `hermes_home_key`
 * alone, never the label.
 *   {"status":"ok","equal":true|false} | {"status":"error",...} */
json_t *w21_context_1bf504_scope_equal(const char *dir_a, const char *name_a,
                                        const char *dir_b, const char *name_b);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_CONTEXT_1BF504_H */
