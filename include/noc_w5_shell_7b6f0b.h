#ifndef NOC_W5_SHELL_7B6F0B_H
#define NOC_W5_SHELL_7B6F0B_H
#include <stddef.h>

/* C home for upstream Hermes pm/shell.py (gate w5_shell_7b6f0b).
 * Only the pure, env-driven candidate builder is ported; the filesystem and
 * subprocess probes in _staged_bash/_bash_starts/bash stay in Python. */

typedef struct {
    const char *key;
    const char *value;
} w5_shell_7b6f0b_env_t;

/* PoP: w5_shell_7b6f0b_windows_bash_candidates @ pm/shell.py:windows_bash_candidates
 * Returns a malloc'd NULL-terminated array of malloc'd candidate strings in
 * the Python order (dict.fromkeys dedup, first occurrence wins). on_path may
 * be NULL/empty. *out_n receives the count. Caller frees with
 * w5_shell_7b6f0b_strings_free(). */
char **w5_shell_7b6f0b_windows_bash_candidates(
    const char *on_path, const w5_shell_7b6f0b_env_t *env, size_t env_count,
    size_t *out_n);

/* PoP: w5_shell_7b6f0b_strings_free @ pm/shell.py:<module> */
void w5_shell_7b6f0b_strings_free(char **arr, size_t n);

#endif /* NOC_W5_SHELL_7B6F0B_H */
