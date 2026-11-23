#ifndef NOC_W5_RUNTIME_3F1397_H
#define NOC_W5_RUNTIME_3F1397_H
#include <stddef.h>

/* The single launch-program string used by the resident-payload branch of
 * runtime_command (mirrors pm/runtime.py:launcher exactly). */
const char *w5runtime_launcher(void);

/* PoP: w5runtime_validate_result @ pm/runtime.py:_validate
 * Reproduces: checked.stderr.strip() or f"exit {returncode}"
 *             if checked.returncode else ""
 * Returns a malloc'd string; free with free(). */
char *w5runtime_validate_result(int returncode, const char *stderr_text);

/* PoP: w5runtime_command_args @ pm/runtime.py:runtime_command
 * Builds the argv list for one launch contract. resident=0 ->
 * [python, -I, -B, script, *args]; resident=1 ->
 * [python, -I, -S, -B, -c, LAUNCHER, site, script, *args].
 * Returned list is malloc'd; free with w5runtime_free_list. */
int w5runtime_command_args(int resident, const char *python, const char *site,
                           const char *script, const char **args, size_t nargs,
                           char ***out, size_t *outn);
void w5runtime_free_list(char **arr, size_t n);

#endif /* NOC_W5_RUNTIME_3F1397_H */
