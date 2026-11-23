#ifndef NOC_W15_WORKTREEAR_5029D2_H
#define NOC_W15_WORKTREEAR_5029D2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_worktreear_5029d2_ref_safe @ hermes_cli/worktree_archive.py:ref_safe
 * Returns a JSON string. */
struct json_t *w15_worktreear_5029d2_ref_safe(const char *name);

/* PoP: w15_worktreear_5029d2_parse_meta @ hermes_cli/worktree_archive.py:_parse_meta
 * Returns the JSON object parsed from the first '{' onward, or {}. */
struct json_t *w15_worktreear_5029d2_parse_meta(const char *body);

/* PoP: w15_worktreear_5029d2_split_oversized @ hermes_cli/worktree_archive.py:_split_oversized
 * untracked is a JSON array of paths, sizes maps each path to a decimal string,
 * max_mb is the cap in MiB. Returns [{"path":..,"bytes":"<decimal>"}]. */
struct json_t *w15_worktreear_5029d2_split_oversized(const struct json_t *untracked,
                                                    const struct json_t *sizes, double max_mb);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_WORKTREEAR_5029D2_H */
