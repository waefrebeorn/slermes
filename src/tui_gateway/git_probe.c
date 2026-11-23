/*
 * git_probe.c -- C home for upstream Hermes tui_gateway/git_probe.py
 * (wave8 gate w8_gitprobe_0c1af8).
 *
 * PoP: w8_gitprobe_0c1af8_branch @ tui_gateway/git_probe.py:branch
 * PoP: w8_gitprobe_0c1af8_resolve @ tui_gateway/git_probe.py:resolve
 *
 * The git-spawning seams are resolved by the caller and injected as plain
 * strings, so the C side reproduces the pure decision logic of each function
 * without ever starting a git subprocess.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w8_gitprobe_0c1af8.h"

json_t *w8_gitprobe_0c1af8_branch(const char *first, const char *second) {
    const char *chosen = (first && first[0]) ? first : (second ? second : "");
    return json_string(chosen);
}

json_t *w8_gitprobe_0c1af8_resolve(const char *worktree_root,
                                   const char *common_root) {
    if (!worktree_root || !worktree_root[0])
        return json_null();
    json_t *out = json_object();
    const char *repo_root = (common_root && common_root[0]) ? common_root : worktree_root;
    json_set(out, "repo_root", json_string(repo_root));
    json_set(out, "worktree_root", json_string(worktree_root));
    return out;
}
