#ifndef NOC_W15_MAINRESUME_0C8E53_H
#define NOC_W15_MAINRESUME_0C8E53_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/main_resume_args.py (gate w15_mainresume_0c8e53).
 * All results are freshly allocated json_t values freed by the caller.
 * These exports port the decision logic (guards) of the three CLI steps; the
 * filesystem/chdir/session-import side effects stay in Python. The oracle
 * injects sentinel-raising fakes for the dependency imports so the live
 * functions can be exercised without performing those side effects. */

/* PoP: w15_mainresume_0c8e53_apply_in_dir_guard @ hermes_cli/main_resume_args.py:apply_in_dir */
struct json_t *w15_mainresume_0c8e53_apply_in_dir_guard(const struct json_t *in_dir);

/* PoP: w15_mainresume_0c8e53_import_foreign_resume_guard @ hermes_cli/main_resume_args.py:import_foreign_resume */
struct json_t *w15_mainresume_0c8e53_import_foreign_resume_guard(const struct json_t *resume);

/* PoP: w15_mainresume_0c8e53_recreate_tui_worktree_guard @ hermes_cli/main_resume_args.py:recreate_resumed_tui_worktree */
struct json_t *w15_mainresume_0c8e53_recreate_tui_worktree_guard(const struct json_t *resume,
                                                                const struct json_t *worktree);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_MAINRESUME_0C8E53_H */
