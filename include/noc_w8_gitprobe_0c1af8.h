#ifndef NOC_W8_GITPROBE_0C1AF8_H
#define NOC_W8_GITPROBE_0C1AF8_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_gitprobe_0c1af8_branch @ tui_gateway/git_probe.py:branch
 * ``first or second`` over the two git outputs (first/second are the already
 * captured stdout strings; empty string is falsy). Returns a JSON string. */
struct json_t *w8_gitprobe_0c1af8_branch(const char *first, const char *second);

/* PoP: w8_gitprobe_0c1af8_resolve @ tui_gateway/git_probe.py:resolve
 * Injectable-root seam for resolve(): pass the already-resolved worktree root
 * and common root. Empty worktree root -> JSON null; otherwise an object with
 * repo_root = (common_root or worktree_root) and worktree_root = worktree_root. */
struct json_t *w8_gitprobe_0c1af8_resolve(const char *worktree_root,
                                          const char *common_root);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W8_GITPROBE_0C1AF8_H */
