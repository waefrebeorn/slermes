#ifndef NOC_W5_WORKER_9C3898_H
#define NOC_W5_WORKER_9C3898_H
#include "hermes_json.h"

/* C home for upstream Hermes pm/worker.py (gate w5_worker_9c3898).
 * The control-channel framing / validation loop (_read_controls) is ported
 * as a pure function over the decoded message list: it enforces one request
 * id, handles cancel, and reports the ValueError/KeyError exactly as Python
 * does. Process/FD/thread orchestration in main() stays in Python.
 *
 * Returns {"messages": [...], "paused": bool, "error": str|null}. */
json_t *w5_worker_9c3898_read_controls(json_t *messages);

#endif /* NOC_W5_WORKER_9C3898_H */
