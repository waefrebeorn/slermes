#ifndef NOC_DASHPROC_H
#define NOC_DASHPROC_H
/*
 * Public C API for the port of persistence/sqlite/_dashboard_procs.py
 * (gate dashproc).  All exported symbols carry the dashproc_ prefix.
 * argv is a JSON array of argv tokens.
 */
#include "hermes_json.h"
/* Index of the first "serve"/"dashboard" token, or -1 for Python None. */
int dashproc_dashboard_subcommand_index(const json_t *argv);

/* PoP: dashproc_is_ephemeral_port_zero_backend @ persistence/sqlite/_dashboard_procs.py:_is_ephemeral_port_zero_backend */
int dashproc_is_ephemeral_port_zero_backend(const json_t *argv);

#endif /* NOC_DASHPROC_H */
