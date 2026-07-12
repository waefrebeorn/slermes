#ifndef PORT_AGENT_THINKING_TIMEOUT_GUIDANCE_H
#define PORT_AGENT_THINKING_TIMEOUT_GUIDANCE_H

#include <stdbool.h>

/* C port of agent/thinking_timeout_guidance.py. */
bool thinking_timeout_is(const char *reason_value, const char *model,
                         const char *error_msg);
/* malloc'd guidance string (caller frees) or NULL. */
char *thinking_timeout_guidance(const char *provider, const char *model,
                                const char *model_label);

#endif /* PORT_AGENT_THINKING_TIMEOUT_GUIDANCE_H */
