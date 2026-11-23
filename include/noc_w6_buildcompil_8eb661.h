#ifndef NOC_W6_BUILDCOMPIL_8EB661_H
#define NOC_W6_BUILDCOMPIL_8EB661_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C port of pm/build_compilers.py (gate w6_buildcompil_8eb661).
 * The external probes (sys.platform, _recorded_toolchain, shutil.which) are
 * explicit seams: `platform`, `recorded`, `env`, `available` come from the
 * caller/fixture. `available` is the list of executables on PATH. */

/* PoP: w6_buildcompil_8eb661_missing_compiler_remedy @ pm/build_compilers.py:missing_compiler_remedy */
char *w6_buildcompil_8eb661_missing_compiler_remedy(const char *output);

/* PoP: w6_buildcompil_8eb661_linux_defaults @ pm/build_compilers.py:_linux_defaults */
struct json_t *w6_buildcompil_8eb661_linux_defaults(const struct json_t *recorded, const struct json_t *env,
                                  const struct json_t *available);
/* PoP: w6_buildcompil_8eb661_pinned_build_defaults @ pm/build_compilers.py:pinned_build_defaults */
struct json_t *w6_buildcompil_8eb661_pinned_build_defaults(const char *platform, const struct json_t *recorded,
                                         const struct json_t *env, const struct json_t *available);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W6_BUILDCOMPIL_8EB661_H */
