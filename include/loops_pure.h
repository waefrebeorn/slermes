#ifndef HERMES_LOOPS_PURE_H
#define HERMES_LOOPS_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/loops.py.
 * All return freshly malloc'd strings (free with free()) or NULL. */

int  loops_parse_interval_token(const char *token);
char *loops_format_interval(double seconds);
int  loops_response_signals_complete(const char *response);

int  loops_min_interval_seconds(const char *config_json);
int  loops_max_ticks_default(const char *config_json);
int  loops_self_paced_floor_seconds(const char *config_json);
int  loops_self_paced_ceiling_seconds(const char *config_json);

char *loops_parse_loop_args(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LOOPS_PURE_H */
