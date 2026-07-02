/*
 * model_metadata.c — Port of Python agent/model_metadata.py
 *
 * Python API → C implementation mapping:
 *   model_metadata_lookup()         → model_metadata_lookup() in provider_metadata.c
 *   model_metadata_get_provider()   → get_model_provider() in provider_metadata.c
 *   model_metadata_count_tokens()   → hermes_token_count() in hermes_tokenizer.c
 *   model_metadata_get_pricing()    → model_estimate_cost() in provider_metadata.c
 *   model_metadata_validate_model() → N/A (C validates at config load time)
 *   model_metadata_list_models()    → N/A (CLI-level feature)
 *   model_metadata_get_context_window() → hermes_token_context_size() in provider_metadata.c
 *
 * Model metadata manager — implemented in provider_metadata.c + hermes_tokenizer.c.
 */

#include "provider_metadata.h"   /* model_metadata_lookup(), model_estimate_cost(), hermes_token_context_size() */
