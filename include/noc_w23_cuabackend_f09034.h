/*
 * noc_w23_cuabackend_f09034.h - C surface for upstream
 * tools/computer_use/cua_backend_session.py (gate w23_cuabackend_f09034).
 *
 * The session's transport (asyncio bridge, stdio MCP client, subprocess CLI
 * fallback, the wall clock) is a process seam: the caller supplies what the
 * transport reported and this surface owns only the decisions taken from it --
 * the fail-closed "outcome unknown" envelopes, the error classifiers, the
 * per-tool capability lookups and the MCP field reads.
 */
#ifndef NOC_W23_CUABACKEND_F09034_H
#define NOC_W23_CUABACKEND_F09034_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _CLI_ATTEMPTS: CLI fallback transport retries (backoff 0.5s doubling). */
#define W23_CUABACKEND_F09034_CLI_ATTEMPTS 4

int w23_cuabackend_f09034_cli_attempts(void);

/* req keys: name (str), detail (str, the exception text), code (one of
 * "transport_outcome_unknown" / "timeout_outcome_unknown").
 * Returns the fail-closed tool envelope exactly as the MCP/CLI transports emit
 * it: {data, images, image_mime_types, structuredContent{ok,code,message,
 * operation,next_step,detail}, isError}.  An unknown code has no message
 * template and yields JSON null. */
json_t *w23_cuabackend_f09034_outcome_unknown(const json_t *req);

/* error keys: data, structuredContent.  Flattens a logical MCP error into the
 * text cli/classification reads: json.dumps(value, sort_keys=True) for
 * non-strings, "\n"-joined, None skipped.  Caller frees. */
char *w23_cuabackend_f09034_logical_error_text(const json_t *error);

/* result keys: isError (identity true), data, structuredContent.  Recognises
 * cua-driver's explicit recoverable ended-session result. */
int w23_cuabackend_f09034_is_ended_session_result(const json_t *result);

/* exc keys: name (class __name__), module (class __module__), base
 * ("" | "broken_pipe" | "eof" -- the isinstance seam).  True for MCP/stdio
 * failures that a reconnect can recover. */
int w23_cuabackend_f09034_is_closed_session_error(const json_t *exc);

/* True when str(exc) carries a daemon-proxy congestion needle. */
int w23_cuabackend_f09034_is_transient_daemon_error(const char *message);

/* req keys: capability, capabilities{tool: [tokens]}, optional tool.
 * tool absent/null -> any tool; a non-string tool matches nothing. */
int w23_cuabackend_f09034_supports_capability(const json_t *req);

/* req keys: tool, property, schemas{tool: schema}.  Live tools/list schema
 * accepts *property* (fails closed: no schema, no properties, no answer). */
int w23_cuabackend_f09034_supports_input_property(const json_t *req);

/* mcp 1.x -> 2.x field read: obj[snake] if present else obj[camel] else null.
 * Returns an owned node (never NULL). */
json_t *w23_cuabackend_f09034_mcp_field(const json_t *obj, const char *snake,
                                        const char *camel);

/* _mcp_field plus the model_extra fallback Pydantic v2 forwards custom fields
 * through.  Returns an owned node (never NULL). */
json_t *w23_cuabackend_f09034_tool_field(const json_t *obj, const char *snake,
                                         const char *camel);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W23_CUABACKEND_F09034_H */
