#ifndef NOC_W5_ENVIRONMENT_032CBD_H
#define NOC_W5_ENVIRONMENT_032CBD_H

/* Pure failure-classification primitives from pm/environment.py.
 * The subprocess/streaming engine (PythonEnvironment._run, _run_streaming,
 * managed_environment, ...) is process/IO-bound and out of scope for the gate. */

/* PoP: w5_environment_032cbd_is_bytecode_compile_failure @ pm/environment.py:_is_bytecode_compile_failure
 * Returncode 0 => false; else case-insensitive marker in (stderr or "")+"\n"+(stdout or ""). */
int w5_environment_032cbd_is_bytecode_compile_failure(int returncode,
        const char *stderr_text, const char *stdout_text);

typedef struct {
    const char *kind;    /* "InstallError" | "ResolutionConflict" | "BuildFailure" */
    char       *package; /* malloc'd */
    char       *cause;   /* malloc'd */
    char       *remedy;  /* malloc'd */
} w5_environment_032cbd_error_t;

/* PoP: w5_environment_032cbd_classify_uv_failure @ pm/environment.py:classify_uv_failure
 * 'build_remedy' injects pm.build_compilers.missing_compiler_remedy(output),
 * which is only consulted on the build-failure branch. Returns 0 on OOM. */
int w5_environment_032cbd_classify_uv_failure(const char *stage, int returncode,
        const char *output, const char *build_remedy,
        w5_environment_032cbd_error_t *out);
void w5_environment_032cbd_error_free(w5_environment_032cbd_error_t *e);

#endif /* NOC_W5_ENVIRONMENT_032CBD_H */
