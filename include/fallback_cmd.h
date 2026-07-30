#ifndef HERMES_FALLBACK_CMD_H
#define HERMES_FALLBACK_CMD_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void fallback_format_entry(const json_t *entry, char *out, size_t out_cap);
json_t *fallback_extract_from_model_cfg(const json_t *model_cfg);
void fallback_describe_primary(const json_t *config, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FALLBACK_CMD_H */
