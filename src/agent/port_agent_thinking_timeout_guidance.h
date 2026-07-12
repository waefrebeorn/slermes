#ifndef AGENT_THINKING_TIMEOUT_GUIDANCE_H
#define AGENT_THINKING_TIMEOUT_GUIDANCE_H
#include <stdbool.h>
bool agent_thinking_timeout_is_thinking_timeout(const char *reason_value, const char *model, const char *err);
#endif
