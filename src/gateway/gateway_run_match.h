/*
 * gateway_run_match.h — provider-error pattern matchers + secret redaction.
 *
 * Module-internal header (port_gateway_run.c <-> gateway_run_match.c).
 */
#ifndef GATEWAY_RUN_MATCH_H
#define GATEWAY_RUN_MATCH_H

#include <stdbool.h>
#include <stddef.h>

bool match_provider_error_re(const char *text);
bool match_auth_error_re(const char *text);
bool match_policy_re(const char *text);
bool match_rate_limit_re(const char *text);
void apply_secret_patterns(const char *text, char *out, size_t out_size);

#endif /* GATEWAY_RUN_MATCH_H */
