#ifndef HERMES_AZURE_DETECT_H
#define HERMES_AZURE_DETECT_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void azure_strip_trailing_v1(const char *url, char *out, size_t out_cap);
int azure_looks_like_anthropic_path(const char *url);
json_t *azure_extract_model_ids(const json_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_AZURE_DETECT_H */
