/*
 * port_gemini_native_adapter.h -- pure helpers ported from
 * agent/gemini_native_adapter.py.
 *
 * The httpx / asyncio half stays in Python.
 */
#ifndef PORT_GEMINI_NATIVE_ADAPTER_H
#define PORT_GEMINI_NATIVE_ADAPTER_H

#include <stddef.h>

/* bare_gemini_model_id -- strip google/ or gemini/ from an aggregator id.
 * Caller frees. */
char *gna_bare_model_id(const char *model);

/* _gemini_version -- (major, minor); (0, 0) for non-Gemini / unversioned. */
void gna_version(const char *model, int *major, int *minor);

int gna_requires_tool_call_ids(const char *model);
int gna_uses_thinking_level(const char *model);
int gna_sampling_deprecated(const char *model);

int gna_is_vertex_express(const char *base_url);
char *gna_normalize_base_url(const char *base_url);
int gna_is_native_base_url(const char *base_url);
int gna_accepts_parameters_schema(const char *base_url);

int gna_is_free_tier_quota_error(const char *error_message);
int gna_is_standard_key_error(int status, const char *error_message,
                              const char *reason, const char *api_key);

/* _coerce_content_to_text -- `items` is a NULL-terminated array where a NULL
 * entry is a non-text item. Caller frees. */
char *gna_coerce_content_to_text(const char *const *items);

char *gna_lower(const char *s);

#endif /* PORT_GEMINI_NATIVE_ADAPTER_H */
