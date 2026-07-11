#ifndef SLERMES_PROMPT_BUILDER_GUIDANCE_H
#define SLERMES_PROMPT_BUILDER_GUIDANCE_H

#include <stddef.h>

/*
 * prompt_builder_guidance.h — platform-aware computer-use guidance string
 * builder for the system prompt.
 *
 * Self-contained module: no god headers, no cross-module struct access.
 * Port of agent/prompt_builder.py:computer_use_guidance. The single public
 * function returns a malloc'd, platform-aware guidance string (caller frees)
 * or NULL on allocation failure. This mirrors the Python helper which returns
 * a freshly-built str.
 */

/* Opaque guidance builder context. Currently stateless, but kept opaque so
 * the module stays self-contained and can grow per-platform config without
 * touching callers. */
typedef struct prompt_builder_guidance prompt_builder_guidance_t;

/* Create a guidance builder context. Returns NULL on allocation failure. */
prompt_builder_guidance_t *prompt_builder_guidance_init(void);

/* Free a guidance builder context (safe with NULL). */
void prompt_builder_guidance_free(prompt_builder_guidance_t *ctx);

/* Return a platform-aware computer-use guidance string for the system prompt.
 * platform_name is an sys.platform-style string ("darwin"/"win32"/"linux");
 * NULL means "use the running host" which we approximate as "linux" (the agent
 * runs on Linux here). Caller frees the result. Returns NULL on alloc failure. */
char *prompt_builder_computer_use_guidance(const prompt_builder_guidance_t *ctx,
                                           const char *platform_name);

#endif /* SLERMES_PROMPT_BUILDER_GUIDANCE_H */
