/*
 * noc_w25_apiserverf_ce83ea.h - C surface for upstream
 * gateway/platforms/api_server_fire_startup.py (gate w25_apiserverf_ce83ea).
 *
 * The Chronos fire webhook's startup gate: a POST /api/cron/fire that arrives
 * before the gateway has published its adapters must be refused with the
 * dashboard-readable retryable 503 instead of running its job against no live
 * transport.  The aiohttp request, the runner object, the event loop and the
 * sleep are seams; what is ported is the polling decision they decide.
 */
#ifndef NOC_W25_APISERVERF_CE83EA_H
#define NOC_W25_APISERVERF_CE83EA_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_APISERVERF_CE83EA_FIRE_STARTUP_WAIT_SECONDS 7.0
#define W25_APISERVERF_CE83EA_POLL_SECONDS 0.1
#define W25_APISERVERF_CE83EA_RETRY_AFTER_SECONDS 60
#define W25_APISERVERF_CE83EA_ERROR_TEXT "gateway unreachable; retry"

/* runner attributes -> bool(_draining or _external_drain_active). */
json_t *w25_apiserverf_ce83ea_is_draining(const json_t *state);

/* {"has_runner","draining","external_drain","running","received_at","now",
 *  "job_id"} -> {"result":"allow"|"refuse","polls":N[,"status","retry_after","body"]}.
 * A missing "running" is getattr's default True; a present null is falsy. */
json_t *w25_apiserverf_ce83ea_startup_gate(const json_t *state);

/* live_adapters_once_started: {"refusal": <gate result>, "adapters": [...]|null}. */
json_t *w25_apiserverf_ce83ea_live_adapters(const json_t *state);

/* The module constants, as an object. */
json_t *w25_apiserverf_ce83ea_constants(void);

#ifdef __cplusplus
}
#endif
#endif
