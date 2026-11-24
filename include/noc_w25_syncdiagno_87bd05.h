/*
 * noc_w25_syncdiagno_87bd05.h - C surface for upstream
 * agent/lsp/sync_diagnostics.py (gate w25_syncdiagno_87bd05).
 *
 * typescript-language-server publishes diagnostics asynchronously and stays
 * silent when a file's diagnostics are unchanged, so this asks the tsserver
 * through ``workspace/executeCommand`` instead: the two synchronous commands
 * answer for the text last synced.  Sending the request, the clock and the
 * logger are seams; this header owns the support gate, the tsserver -> LSP
 * mapping and the per-command failure ladder.
 */
#ifndef NOC_W25_SYNCDIAGNO_87BD05_H
#define NOC_W25_SYNCDIAGNO_87BD05_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_SYNCDIAGNO_87BD05_COMMANDS 2

/* supported(server_id, unsupported_flag) -> server is tsserver-backed and has
 * not yet rejected a synchronous request. */
json_t *w25_syncdiagno_87bd05_supported(const json_t *server_id, const json_t *unsupported);

/* _from_tsserver(item) -> an LSP diagnostic in the pushed shape, or null when
 * the item is not a diagnostic (not an object, or no start/end location). */
json_t *w25_syncdiagno_87bd05_from_tsserver(const json_t *item);

/* request(...) over fixed replies: {"server_id", "unsupported", "timeout",
 * "replies": [{"kind": "ok"|"timeout"|"request_error"|"protocol_error",
 *             "reply": <any>}, ...]} (one entry per command, in order) ->
 * {"status": "none"|"timeout"|"diagnostics",
 *  "diagnostics": [...],      (only for "diagnostics")
 *  "message": "..."           (only for "timeout")
 *  "unsupported": bool}       (the client flag after the call) */
json_t *w25_syncdiagno_87bd05_request(const json_t *req);

/* The command/severity tables, as an object. */
json_t *w25_syncdiagno_87bd05_constants(void);

#ifdef __cplusplus
}
#endif
#endif
