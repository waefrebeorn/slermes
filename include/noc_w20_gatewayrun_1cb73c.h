/*
 * noc_w20_gatewayrun_1cb73c.h -- C11 port of the adaptive startup deadline of
 * upstream hermes_cli/gateway_runtime_progress.py.
 */
#ifndef NOC_W20_GATEWAYRUN_1CB73C_H
#define NOC_W20_GATEWAYRUN_1CB73C_H

#include "hermes_json.h"

/* The module's tuned constants. */
json_t *w20_gatewayrun_1cb73c_constants(void);

/*
 * One whole ensure loop: construct the runway, then replay ``steps`` through
 * ``extend``.  Each step carries the injected I/O seams the Python class reads
 * from the process table:
 *   {"now": <monotonic>, "homes": [...]|null, "child": {"pid": n}|null,
 *    "owner_pid": n|null, "cpu": <seconds>|null}
 * ``owner_pid``/``cpu`` are what _starting_owner_pid / _cpu_seconds return.
 *
 * -> {"status":"ok","deadlines":[...]} or
 *    {"status":"raised","type":"ValueError","message":...}
 */
json_t *w20_gatewayrun_1cb73c_run(const json_t *timeout, double default_s,
                                  const json_t *steps);

#endif /* NOC_W20_GATEWAYRUN_1CB73C_H */
