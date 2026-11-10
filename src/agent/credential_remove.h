/*
 * credential_remove.h - credential-source removal helpers.
 *
 * Module-internal header (credential_sources.c <-> credential_remove.c).
 */
#ifndef CREDENTIAL_REMOVE_H
#define CREDENTIAL_REMOVE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "credential_sources.h"   /* removal_result_t */

bool match_config_source(const char *source);
bool match_copilot_source(const char *source);
bool match_device_code(const char *source);
bool match_env_source(const char *source);
void removal_result_add_cleaned(removal_result_t *r, const char *msg);
void removal_result_add_hint(removal_result_t *r, const char *msg);
void removal_result_init(removal_result_t *r);
removal_result_t remove_claude_code(const char *provider, const char *source);
removal_result_t remove_custom_config(const char *provider, const char *source);
removal_result_t remove_hermes_pkce(const char *provider, const char *source);
removal_result_t remove_qwen_cli(const char *provider, const char *source);

#endif /* CREDENTIAL_REMOVE_H */
