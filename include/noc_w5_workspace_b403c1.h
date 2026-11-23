#ifndef NOC_W5_WORKSPACE_B403C1_H
#define NOC_W5_WORKSPACE_B403C1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_workspace_b403c1_member_ignored @ pm/workspace.py:_member_ignored
 * args: (ignored directory, names); returns the names shutil.copytree would ignore. */
struct json_t *w5_workspace_b403c1_member_ignored(const struct json_t *names);

/* PoP: w5_workspace_b403c1_without_release_age @ pm/workspace.py:_without_release_age
 * re.sub(r"(?m)^exclude-newer\s*=.*\n", "", text) */
struct json_t *w5_workspace_b403c1_without_release_age(const char *text);

/* PoP: w5_workspace_b403c1_conjuncts @ pm/workspace.py:_conjuncts
 * frozenset of top-level `and` terms (or one term for top-level `or`), sorted. */
struct json_t *w5_workspace_b403c1_conjuncts(const char *marker);

/* PoP: w5_workspace_b403c1_member_key @ pm/workspace.py:_member_key
 * arg: already-resolved identity path string (the .resolve() seam); the
 * basename is derived from it exactly as pathlib.Path.name does. */
struct json_t *w5_workspace_b403c1_member_key(const char *resolved);

/* PoP: w5_workspace_b403c1_solve_only_for @ pm/workspace.py:_solve_only_for
 * args: environments array + target object with sys_platform/platform_machine.
 * Returns {"environments": [...]} mirroring the mutated tool.uv policy. */
struct json_t *w5_workspace_b403c1_solve_only_for(const struct json_t *environments,
                                                  const struct json_t *target);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_WORKSPACE_B403C1_H */
