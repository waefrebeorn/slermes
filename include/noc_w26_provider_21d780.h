/*
 * noc_w26_provider_21d780.h - C surface for upstream
 * plugins/web/ddgs/provider.py (gate w26_provider_21d780).
 *
 * The DuckDuckGo provider's value layer: the worker stdout envelope decoder,
 * the search outcome mapping (availability, timeout, interrupt, vendor error)
 * and the `hermes tools` picker entry.  The disposable child process, its
 * pipes and the 30 s wall clock are seams; the envelope text and the exit code
 * arrive here as fixed inputs.
 */
#ifndef NOC_W26_PROVIDER_21D780_H
#define NOC_W26_PROVIDER_21D780_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Worker stdout + exit code -> {"ok": true, "results": [...]} or {"error": str}. */
json_t *w26_provider_21d780_parse_envelope(const char *raw, long long exit_code);

/* {available, seam, query, limit, hits} -> {"result": {...}, "safe_limit": N|null} */
json_t *w26_provider_21d780_search(const json_t *req);

/* The picker entry `get_setup_schema()` builds. */
json_t *w26_provider_21d780_setup_schema(void);

/* {"search_timeout_secs","poll_interval_secs","terminate_grace_secs","name","display_name"} */
json_t *w26_provider_21d780_constants(void);

#ifdef __cplusplus
}
#endif
#endif
