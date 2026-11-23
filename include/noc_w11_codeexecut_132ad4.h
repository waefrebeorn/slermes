#ifndef NOC_W11_CODEEXECUT_132AD4_H
#define NOC_W11_CODEEXECUT_132AD4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_codeexecut_132ad4_is_secret @ tools/code_execution_env.py:is_secret_env_name */
int w11_codeexecut_132ad4_is_secret(const char *name);

/* PoP: w11_codeexecut_132ad4_cache_probe @ tools/code_execution_env.py:_cache_probe_result
 * Returns a NEW object: copy of `cache` with key inserted (FIFO-evicted at 32). */
struct json_t *w11_codeexecut_132ad4_cache_probe(const struct json_t *cache, const char *key,
                                                 struct json_t *value);

/* PoP: w11_codeexecut_132ad4_scrub @ tools/code_execution_env.py:_scrub_child_env
 * External collaborators are injected as data seams (Windows CRT cannot keep
 * Python's process env): passthrough/resolved/scoped_additions/toolchain arrays,
 * plus the delegated-child transform inputs (delegated, os_task, fence_root). */
struct json_t *w11_codeexecut_132ad4_scrub(const struct json_t *source_env,
                                           const struct json_t *passthrough,
                                           const struct json_t *resolved,
                                           const struct json_t *scoped_additions,
                                           const struct json_t *toolchain,
                                           int is_windows, int delegated,
                                           const char *os_task, const char *fence_root);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_CODEEXECUT_132AD4_H */
