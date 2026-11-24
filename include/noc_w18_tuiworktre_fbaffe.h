#ifndef NOC_W18_TUIWORKTRE_FBAAFE_H
#define NOC_W18_TUIWORKTRE_FBAAFE_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C port of hermes_cli/tui_worktree_resume.py path/record decision logic.
 * PoP: w18_tuiworktre_fbaffe_key @ hermes_cli/tui_worktree_resume.py:_key
 * PoP: w18_tuiworktre_fbaffe_recorded @ hermes_cli/tui_worktree_resume.py:_recorded_tree_for
 */

/* -> os.path.normcase(os.path.realpath(str(path))) as a JSON string. */
const char *w18_tuiworktre_fbaffe_key(const char *path);

/* The recorded missing tree *cwd* is (or is inside), as a JSON object
 * {path,branch,repo_root} or JSON null. `trees` maps _key(path) -> entry dict;
 * `existing` is the set of _key() values that exist on disk. The `_read()` I/O
 * seam and Path.exists() are injected as arguments, everything else is verbatim. */
json_t *w18_tuiworktre_fbaffe_recorded(const char *cwd, const json_t *trees,
                                       const json_t *existing);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_TUIWORKTRE_FBAAFE_H */
