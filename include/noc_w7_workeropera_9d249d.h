#ifndef NOC_W7_WORKEROPERA_9D249D_H
#define NOC_W7_WORKEROPERA_9D249D_H

#include "hermes_json.h"

/* Public C API for the pure routing table of pm/worker_operations.py
 * (gate w7_workeropera_9d249d). wop_ prefix avoids collisions with the other
 * worker/operation symbols in this repo. */

/* Sorted list of operation names (mirrors sorted(OPERATIONS)). */
struct json_t *wop_names(void);
/* OPERATIONS.get(name) rendered as {module, packages|null, bootstrap}, or null. */
struct json_t *wop_lookup(const char *name);

#endif /* NOC_W7_WORKEROPERA_9D249D_H */
