#ifndef NOC_W19_PRUNING_C083BD_H
#define NOC_W19_PRUNING_C083BD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_pruning_c083bd_store_lock_path @ agent/checkpoints/pruning.py:store_lock_path
 * "<dir>/<name>" -> "<dir>/.<name>.lock" (Path.resolve() + Path.with_name). Caller frees. */
char *w19_pruning_c083bd_store_lock_path(const char *base);

/* PoP: w19_pruning_c083bd_parse_metadata @ agent/checkpoints/pruning.py:Pruner._rewrite
 * Split the `git log -1 --format=%T%x00%aI%x00%cI%x00%B` payload on `sep` with maxsplit=3.
 * sep is the fixture's stand-in byte for git's %x00 (a C string cannot carry an embedded NUL).
 * {"ok":true,"tree","author_date","committer_date","message"} or
 * {"ok":false,"error":"invalid checkpoint metadata: <sha>"}. */
struct json_t *w19_pruning_c083bd_parse_metadata(const char *text, const char *sep, const char *sha);

/* PoP: w19_pruning_c083bd_commits @ agent/checkpoints/pruning.py:Pruner._commits
 * splitlines() of `git rev-list --reverse <ref>`; empty output is a PruneError.
 * git_error non-NULL models the run_git failure path. */
struct json_t *w19_pruning_c083bd_commits(const char *output, const char *ref, const char *git_error);

/* PoP: w19_pruning_c083bd_trim @ agent/checkpoints/pruning.py:Pruner.trim
 * {"ok":true,"action":"rewrite"|"none","keep":[sha...]} or {"ok":false,"error":...} when the
 * ref has no readable history. */
struct json_t *w19_pruning_c083bd_trim(const struct json_t *commits, long long keep_count,
                                       const char *ref);

/* PoP: w19_pruning_c083bd_drop_one_round @ agent/checkpoints/pruning.py:Pruner.drop_one_round
 * refs = [[ref, [commits...]], ...]; {"changed":bool,"dropped":[ref...]}. */
struct json_t *w19_pruning_c083bd_drop_one_round(const struct json_t *cap_bytes,
                                                 const struct json_t *size,
                                                 const struct json_t *refs);

/* PoP: w19_pruning_c083bd_enforce_size @ agent/checkpoints/pruning.py:Pruner.enforce_size
 * sizes = the dir_size() readings after the reclaim and after each rewrite (the measurement
 * script that makes the loop deterministic). {"result":bool,"rewrites":n,"kept":[sha...]}. */
struct json_t *w19_pruning_c083bd_enforce_size(const struct json_t *cap_bytes,
                                               const struct json_t *size,
                                               const struct json_t *refs,
                                               const struct json_t *sizes);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PRUNING_C083BD_H */
