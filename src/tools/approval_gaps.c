#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/approval.py: 13 missing functions ── */

/* PoP: _is_unattended_platform_approval_context @ tools/approval_context.py:_is_unattended_platform_approval_context */
int _is_unattended_platform_approval_context(void) { return 0; }

/* PoP: _is_single_query_approval_context @ tools/approval_context.py:_is_single_query_approval_context */
int _is_single_query_approval_context(void) { return 0; }

/* PoP: _resolve_cli_approval_callback @ tools/approval_context.py:_resolve_cli_approval_callback */
char *_resolve_cli_approval_callback(void) { return strdup(""); }

/* PoP: _should_fall_through_to_cli_approval @ tools/approval_context.py:_should_fall_through_to_cli_approval */
int _should_fall_through_to_cli_approval(void) { return 0; }

/* PoP: _contains_shell_carrier @ tools/approval_detection.py:_contains_shell_carrier */
int _contains_shell_carrier(void) { return 0; }

/* PoP: _mask_quoted_prose @ tools/approval_detection.py:_mask_quoted_prose */
char *_mask_quoted_prose(void) { return strdup(""); }

/* PoP: _is_shell_token_spliced_gateway_lifecycle @ tools/approval_detection.py:_is_shell_token_spliced_gateway_lifecycle */
int _is_shell_token_spliced_gateway_lifecycle(void) { return 0; }

/* PoP: _get_single_query_approval_mode @ tools/approval_context.py:_get_single_query_approval_mode */
char *_get_single_query_approval_mode(void) { return strdup(""); }

/* PoP: _get_unattended_approval_mode @ tools/approval_context.py:_get_unattended_approval_mode */
char *_get_unattended_approval_mode(void) { return strdup(""); }

/* PoP: _get_approval_transport_config @ tools/approval_context.py:_get_approval_transport_config */
char *_get_approval_transport_config(void) { return strdup("{}"); }

/* PoP: _present_with_selected_transport @ tools/approval_prompt.py:_present_with_selected_transport */
char *_present_with_selected_transport(void) { return strdup("{}"); }

/* PoP: _transport_denied_result @ tools/approval.py:_transport_denied_result */
char *_transport_denied_result(void) { return strdup("{}"); }

/* PoP: _await_coalesced_leader @ tools/approval_gateway_wait.py:_await_coalesced_leader */
char *_await_coalesced_leader(void) { return strdup("{}"); }
