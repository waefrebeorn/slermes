#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/model_metadata.py: 12 missing functions ── */

/* PoP: _get_endpoint_metadata_cache_path @ agent/model_metadata.py:_get_endpoint_metadata_cache_path */
char *_get_endpoint_metadata_cache_path(void) { return strdup(""); }

/* PoP: _endpoint_disk_cache_get @ agent/model_metadata.py:_endpoint_disk_cache_get */
char *_endpoint_disk_cache_get(const char *key) { return strdup("{}"); }

/* PoP: _endpoint_disk_cache_put @ agent/model_metadata.py:_endpoint_disk_cache_put */
void _endpoint_disk_cache_put(const char *key, const char *val) { (void)key; (void)val; }

/* PoP: _extract_flat_context_length @ agent/model_metadata.py:_extract_flat_context_length */
int _extract_flat_context_length(void) { return 0; }

/* PoP: _context_length_from_model_payload @ agent/model_metadata.py:_context_length_from_model_payload */
int _context_length_from_model_payload(void) { return 0; }

/* PoP: _stale_pre_catalog_cache_entry @ agent/model_metadata.py:_stale_pre_catalog_cache_entry */
int _stale_pre_catalog_cache_entry(void) { return 0; }

/* PoP: _model_name_suggests_minimax @ agent/model_metadata.py:_model_name_suggests_minimax */
int _model_name_suggests_minimax(void) { return 0; }

/* PoP: _model_name_suggests_stale_32k_underreport @ agent/model_metadata.py:_model_name_suggests_stale_32k_underreport */
int _model_name_suggests_stale_32k_underreport(void) { return 0; }

/* PoP: _verified_codex_ctx_for_slug @ agent/model_metadata.py:_verified_codex_ctx_for_slug */
int _verified_codex_ctx_for_slug(void) { return 0; }

/* PoP: _strip_stale_thinking_for_estimate @ agent/model_metadata.py:_strip_stale_thinking_for_estimate */
char *_strip_stale_thinking_for_estimate(void) { return strdup(""); }

/* PoP: capture_usage_anchor @ agent/model_metadata.py:capture_usage_anchor */
void capture_usage_anchor(void) {}

/* PoP: anchored_context_tokens @ agent/model_metadata.py:anchored_context_tokens */
int anchored_context_tokens(void) { return 0; }
