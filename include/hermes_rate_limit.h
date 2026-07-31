/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_RATE_LIMIT_H
#define SLERMES_RATE_LIMIT_H

#include "hermes_core_types.h"

/* Per-tool / per-provider rate limiting */
bool rate_limit_init_tool(const char *tool_name, int max_per_minute);
bool rate_limit_init_provider(const char *provider_name, int max_per_minute);
bool rate_limit_check_tool(const char *tool_name);
bool rate_limit_check_provider(const char *provider_name);
int  rate_limit_remaining_tool(const char *tool_name);
void rate_limit_reset_all(void);
void rate_limit_clear(void);

#endif /* SLERMES_RATE_LIMIT_H */
