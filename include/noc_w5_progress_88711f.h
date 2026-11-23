#ifndef NOC_W5_PROGRESS_88711F_H
#define NOC_W5_PROGRESS_88711F_H

/* C home for upstream Hermes pm/progress.py — gate w5_progress_88711f. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_progress_88711f_verbose_output @ pm/progress.py:verbose_output
 * Mirrors verbose_output(env): 1 when child output streams unchanged. */
int w5_progress_88711f_verbose_output(const json_t *env);

/* PoP: w5_progress_88711f_ansi_strip @ pm/progress.py:LiveTail._line
 * Mirrors _ANSI.sub("", line[-MAX_LINE:]).rstrip().
 * Returns a malloc'd NUL-terminated string; caller frees. */
char *w5_progress_88711f_ansi_strip(const char *line);

/* PoP: w5_progress_88711f_normalize_lines @ pm/progress.py:LiveTail.write
 * Mirrors (partial+text).replace("\r\n","\n").replace("\r","\n").split("\n")
 * with the final element popped as the (MAX_LINE-truncated) partial.
 * Returns {"lines":[...], "partial":"..."}; caller frees. */
json_t *w5_progress_88711f_normalize_lines(const char *partial, const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_PROGRESS_88711F_H */
