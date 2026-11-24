/*
 * noc_w25_gatewaycli_8b715b.h - C surface for upstream
 * hermes_cli/gateway_cli_commands.py (gate w25_gatewaycli_8b715b).
 *
 * The decisions of the classic CLI commands whose state lives in the gateway:
 * how /diff's arguments are read (session mode, --stat), where /handoff parses
 * its platform and which branch it takes, and how run() routes a command to the
 * gateway (or refuses to).  The connection, the session diff payload, the
 * handoff wait and the terminal printers are seams: every one is a fixed input.
 */
#ifndef NOC_W25_GATEWAYCLI_8B715B_H
#define NOC_W25_GATEWAYCLI_8B715B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {cmd, result:{enabled,...}|null} ->
 * {handled, branch:"none"|"not_enabled"|"session_diff", stat_only}. */
json_t *w25_gatewaycli_8b715b_diff(const json_t *req);

/* req: {cmd, session_id, agent_running, kept, home_name} ->
 * {result:"USAGE"|"BUSY"|"KEPT"|"EXIT", platform, lines}. */
json_t *w25_gatewaycli_8b715b_handoff(const json_t *req);

/* req: {canonical, cmd, conn, route, error, detail} ->
 * {result:"TRUE"|"FALSE"|"EXIT_SESSION", printed}. */
json_t *w25_gatewaycli_8b715b_run(const json_t *req);

/* The route table and the named refusals, as an object. */
json_t *w25_gatewaycli_8b715b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
