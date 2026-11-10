/*
 * provider_model_caps.h — Anthropic model-capability predicates.
 *
 * Module-internal header (provider_anthropic.c <-> provider_model_caps.c).
 * Model version/feature detection shared by the request builder.
 */
#ifndef PROVIDER_MODEL_CAPS_H
#define PROVIDER_MODEL_CAPS_H

#include <stdbool.h>
#include <stddef.h>

void normalize_model_key(const char *model, char *out, size_t out_size);
bool supports_adaptive_thinking(const char *model);
bool supports_xhigh_effort(const char *model);
bool forbids_sampling_params(const char *model);
bool supports_fast_mode(const char *model);

#endif /* PROVIDER_MODEL_CAPS_H */
