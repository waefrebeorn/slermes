#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/runtime_provider.py: 4 missing functions ── */

/* PoP: filter_capabilities @ hermes_cli/runtime_provider_custom.py:_filter_capabilities */
char *filter_capabilities(void) { return strdup("[]"); }

/* PoP: lift_model_capabilities @ hermes_cli/runtime_provider_custom.py:_lift_model_capabilities */
char *lift_model_capabilities(void) { return strdup("[]"); }

/* PoP: is_routable_provider @ hermes_cli/runtime_provider_custom.py:is_routable_provider */
int is_routable_provider(void) { return 0; }

/* PoP: is_external_process_provider @ hermes_cli/runtime_provider_backends.py:_is_external_process_provider */
int is_external_process_provider(void) { return 0; }
