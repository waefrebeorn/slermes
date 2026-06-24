#ifndef HERMES_COPILOT_OAUTH_H
#define HERMES_COPILOT_OAUTH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate a Copilot token.
 * Returns 0 if valid, 1 if invalid (reason in err_msg).
 */
int copilot_validate_token(const char *token, char *err_msg, size_t err_sz);

/**
 * Resolve Copilot token from env vars.
 * Checks: COPILOT_GITHUB_TOKEN -> GH_TOKEN -> GITHUB_TOKEN -> gh auth token
 * Returns 0 on success.
 */
int copilot_resolve_token(char *out, size_t out_sz, char *source, size_t src_sz);

/**
 * Run the full Copilot OAuth device code flow.
 * Prints user instructions, polls for token.
 * Returns 0 on success with token in out_token.
 */
int copilot_device_code_flow(char *out_token, size_t token_sz);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_COPILOT_OAUTH_H */
