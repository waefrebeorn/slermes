#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/clarify_gateway.py: 4 missing functions ── */

/* PoP: label_matches @ tools/clarify_gateway.py:label_matches */
int label_matches(void) { return 0; }

/* PoP: selection_attempt_tokens @ tools/clarify_gateway.py:_selection_attempt_tokens */
int selection_attempt_tokens(void) { return 0; }

/* PoP: coerce_text_response_detailed @ tools/clarify_gateway.py:_coerce_text_response_detailed */
char *coerce_text_response_detailed(void) { return strdup("{}"); }

/* PoP: attempt_text_response_for_session @ tools/clarify_gateway.py:attempt_text_response_for_session */
char *attempt_text_response_for_session(void) { return strdup("{}"); }
