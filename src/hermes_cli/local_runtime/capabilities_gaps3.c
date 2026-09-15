#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/local_runtime/capabilities.py: 3 missing functions ── */

/* PoP: is_managed_provider @ hermes_cli/local_runtime/capabilities.py:is_managed_provider */
char *is_managed_provider(void) { return strdup(""); }

/* PoP: _props_modalities @ hermes_cli/local_runtime/capabilities.py:_props_modalities */
void _props_modalities(void) { }

/* PoP: managed_model_supports_vision @ hermes_cli/local_runtime/capabilities.py:managed_model_supports_vision */
char *managed_model_supports_vision(void) { return strdup(""); }
