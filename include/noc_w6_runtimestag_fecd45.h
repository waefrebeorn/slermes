#ifndef NOC_W6_RUNTIMESTAG_FECD45_H
#define NOC_W6_RUNTIMESTAG_FECD45_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_runtimestag_plan @ pm/runtime_stage.py:stage_runtime
 *
 * Reproduces the deterministic PLANNING/decision trace of stage_runtime when
 * every effectful collaborator is injected (project, uv_cache_dir, the
 * PythonEnvironment builder, TemporaryDirectory, shutil.copyfile and
 * subprocess.run are all seams supplied as JSON fields of `in`).  Returns:
 *
 *   {"project": str, "destination": str, "cache": str,
 *    "cache_source": "default"|"explicit", "offline": bool,
 *    "copy_files": ["pyproject.toml","uv.lock"], "created": bool,
 *    "sync_mode": "locked"|"wheelhouse",
 *    "check_argv": [...], "error": [<InstallError composed message>]|null,
 *    "result": str}
 *
 * All path strings are emitted with '/' separators for stable comparison.
 * Caller frees. */
struct json_t *w6_runtimestag_plan(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_RUNTIMESTAG_FECD45_H */
