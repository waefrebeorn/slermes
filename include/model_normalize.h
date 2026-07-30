/*
 * model_normalize.h — per-provider model-name normalization (PoP port).
 *
 * Faithful C port of hermes_cli/model_normalize.py. All functions that
 * return a new string use malloc (caller frees); the deepseek mapper returns
 * a static string. Provider ids should be Hermes-canonical (the module
 * resolves aliases itself via model_normalize_provider from model_catalog).
 */
#ifndef HERMES_MODEL_NORMALIZE_H
#define HERMES_MODEL_NORMALIZE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DeepSeek: map a bare name to a DeepSeek-accepted id. Returns a STATIC string. */
const char *model_normalize_for_deepseek(const char *model_name);

/* Drop a leading vendor/. Caller frees. */
char *model_normalize_strip_vendor(const char *model_name);
/* Replace '.' with '-'. Caller frees. */
char *model_normalize_dots_to_hyphens(const char *model_name);
/* Resolve provider alias to canonical id. Caller frees. */
char *model_normalize_provider_alias(const char *provider_name);
/* Strip provider/ only when prefix == target. Caller frees. */
char *model_normalize_strip_match_prefix(const char *model_name, const char *target_provider);
/* Detect vendor slug from a bare name. Caller frees (or NULL). */
char *model_normalize_detect_vendor(const char *model_name);
/* Prepend vendor/ when missing. Caller frees. */
char *model_normalize_prepend_vendor(const char *model_name);
/* Copilot-specific id resolution (pure static-alias part; live catalog lookup
 * is a REAL_GAP). Caller frees. */
char *model_normalize_copilot_model_id(const char *model_id);
/* Primary entry point. Caller frees. */
char *model_normalize_for_provider(const char *model_input, const char *target_provider);

#ifdef __cplusplus
}
#endif
#endif /* HERMES_MODEL_NORMALIZE_H */
