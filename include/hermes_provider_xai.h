/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_PROVIDER_XAI_H
#define SLERMES_PROVIDER_XAI_H

#include "hermes_core_types.h"

/* xAI model retirement detection */
bool xai_is_model_retired(const char *model_name,
                          char *replacement_out, size_t replacement_sz,
                          char *reasoning_out, size_t reasoning_sz);

#endif /* SLERMES_PROVIDER_XAI_H */
