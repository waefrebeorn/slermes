/*
 * noc_w26_gatewayrun_1eda26.h - C surface for upstream
 * hermes_cli/gateway_runtime_tickets.py (gate w26_gatewayrun_1eda26).
 *
 * ``hermes gateway tickets --serve``: the one-channel ticket protocol.  The
 * line reader (including the 64 KiB refusal), the whole request validator, the
 * reply shape and the error codes are pure functions of the bytes on the wire
 * plus what the profile registry and the mint say.  Both of those are seams.
 */
#ifndef NOC_W26_GATEWAYRUN_1EDA26_H
#define NOC_W26_GATEWAYRUN_1EDA26_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W26_GATEWAYRUN_1EDA26_PROTOCOL 1
#define W26_GATEWAYRUN_1EDA26_MAX_REQUEST_LINE_BYTES (64 * 1024)
#define W26_GATEWAYRUN_1EDA26_PURPOSE_COUNT 4

/* {"wire": str} -> [str|null, ...]   (gateway_runtime_tickets._request_lines;
 * a null element is the one over-long line, dropped unread) */
json_t *w26_gatewayrun_1eda26_request_lines(const json_t *in);

/* {"line": str|null, "mint": {...}, "profiles": {...}} -> the one reply dict
 * (gateway_runtime_tickets._reply) */
json_t *w26_gatewayrun_1eda26_reply(const json_t *in);

/* {"wire": str, "mint": {...}, "profiles": {...}} -> [reply, ...]
 * (the --serve loop over _request_lines + _reply) */
json_t *w26_gatewayrun_1eda26_serve(const json_t *in);

/* The module's literal constants. */
json_t *w26_gatewayrun_1eda26_constants(void);

#ifdef __cplusplus
}
#endif
#endif
