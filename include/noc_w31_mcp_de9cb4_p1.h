/*
 * noc_w31_mcp_de9cb4_p1.h - C surface for upstream tools/connectors/mcp.py
 * (gate w31_mcp_de9cb4_p1).
 *
 * The tool entry point for MCP connection work.  Everything with a side effect
 * -- the catalog file, the installer, the OAuth flow, the connector gateway's
 * account list, the tool registry and the redactor -- is a seam whose answer is
 * passed in on the request object; what this gate owns is the part that is a
 * pure function of those answers: which names an action may address and the
 * exact message it refuses the rest with, the text of a failure, the bounded
 * tool-error body, the card answer's credential coercion, a catalog entry's
 * post-install note, and the two refusals Try again answers with.
 *
 * Every entry point takes the whole request object (one stdin line of the
 * fixture) and returns the owned JSON value the upstream function returns.
 */
#ifndef NOC_W31_MCP_DE9CB4_P1_H
#define NOC_W31_MCP_DE9CB4_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cap on a tool error body, and what a longer one is cut with. */
#define W31_MCP_P1_MAX_TOOL_ERROR_CHARS 2048
#define W31_MCP_P1_TRUNCATION_MARKER "\xE2\x80\xA6 [truncated]"

/* req: {action, names[], catalog[], configured[], hosted[]|null,
 *       catalog_error?, configured_error?} -> the refusal message or null. */
json_t *w31_mcp_de9cb4_p1_validate_names(const json_t *req);

/* req: {action, name} -> the "is a hosted connector account" message. */
json_t *w31_mcp_de9cb4_p1_misrouted_error(const json_t *req);

/* req: {exc_class|null, exc_message} -> the message-less-error substitution. */
json_t *w31_mcp_de9cb4_p1_bare_error_text(const json_t *req);

/* req: {exc_class|null, exc_message, target, approved_env{target:{k:v}}} ->
 * the user-facing failure text: bare-error mapping, then every approved
 * credential of that target replaced.  The pattern redactor is the seam. */
json_t *w31_mcp_de9cb4_p1_detail(const json_t *req);

/* req: {message} -> {"error": <bounded message>}. */
json_t *w31_mcp_de9cb4_p1_tool_error_body(const json_t *req);

/* req: {env} -> the card answer's {str(k): str(v)} map ({} when not a dict). */
json_t *w31_mcp_de9cb4_p1_answer_env(const json_t *req);

/* req: {present, post_install} -> the catalog entry's note, or "". */
json_t *w31_mcp_de9cb4_p1_catalog_instructions(const json_t *req);

/* req: {has_flow, cancelled} -> the kept-authorization note, or "". */
json_t *w31_mcp_de9cb4_p1_cancel_note(const json_t *req);

/* req: {runner, settled} -> why Try again cannot run, or null. */
json_t *w31_mcp_de9cb4_p1_retry_error(const json_t *req);

/* The module's notes, the prepare wait and the bare-error table. */
json_t *w31_mcp_de9cb4_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
