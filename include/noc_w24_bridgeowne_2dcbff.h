/*
 * noc_w24_bridgeowne_2dcbff.h - C surface for upstream
 * plugins/platforms/whatsapp/bridge_ownership.py (gate w24_bridgeowne_2dcbff).
 *
 * Port allocation for the multiplexed WhatsApp bridge, decided on values: the
 * explicit-vs-record-vs-default precedence, the 1..65535 gate, the scan for the
 * first unclaimed port in 3001..3999, and the port-record parse.  The real
 * module stats a profile directory, globs sibling profiles and binds a socket
 * to probe freeness; those are the seams -- the caller supplies
 * record_exists/record_text, the claimed and busy port sets -- so nothing here
 * opens a file or a socket.
 */
#ifndef W24_BRIDGEOWNE_2DCBFF_H
#define W24_BRIDGEOWNE_2DCBFF_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The secondary allocation window and the historical default profile port. */
#define W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_FIRST 3001
#define W24_BRIDGEOWNE_2DCBFF_SECONDARY_PORT_LAST 3999
#define W24_BRIDGEOWNE_2DCBFF_DEFAULT_PORT 3000

/* req: {"explicit": <any>, "record_exists": <bool>, "record_text": <str>} ->
 * the port the launch profile must target: explicit (int() of it) wins, then a
 * persisted record, then 3000.  {"error": "..."} carries the ValueError text. */
json_t *w24_bridgeowne_2dcbff_standalone_port(const json_t *req);

/* req: the same keys plus {"claimed": [ports], "busy": [ports]} (sibling
 * records and the bind probe) -> the port a secondary must target: explicit,
 * else its record, else the first 3001..3999 that is neither claimed nor busy
 * ({"error": "No free bridge port in 3001..3999"} when the window is full).
 * Such an allocation is written to the record by the caller. */
json_t *w24_bridgeowne_2dcbff_secondary_port(const json_t *req);

/* req: {"text": <str>} -> int(record text), or {"error": "not_a_port"} when the
 * record does not hold a port number.  The operator sentence upstream raises
 * for that case embeds the profile path, so the path-naming stays with the
 * caller; the classification is what is compared. */
json_t *w24_bridgeowne_2dcbff_read_port_record(const json_t *req);

/* The allocation window and the default port, as an object. */
json_t *w24_bridgeowne_2dcbff_constants(void);

#ifdef __cplusplus
}
#endif
#endif
