/*
 * port_hermes_cli_timefmt.h — C11 port of hermes_cli/timefmt.py
 */
#ifndef PORT_HERMES_CLI_TIMEFMT_H
#define PORT_HERMES_CLI_TIMEFMT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: relative_time @ hermes_cli/timefmt.py:relative_time */
/* Format a timestamp as relative time (e.g., '2h ago', 'yesterday'). */
char *tf_relative_time(double ts);

#ifdef __cplusplus
}
#endif

#endif /* PORT_HERMES_CLI_TIMEFMT_H */
