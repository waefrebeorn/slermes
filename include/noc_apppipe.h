#ifndef NOC_APPPIPE_H
#define NOC_APPPIPE_H
/* C port of tools/approval_pipe_launchers.py (gate: apppipe).
 * String builders return malloc'd strings (caller frees). */
#include <stddef.h>

char *apppipe_pipe_launcher_re(const char *name, const char *short_opts,
                               const char *const *long_opts, size_t n_long,
                               const char *ws);
const char *apppipe_path_prefix_re(void);  /* static constant */
char *apppipe_wrappers_re(void);           /* malloc'd */

#endif /* NOC_APPPIPE_H */
