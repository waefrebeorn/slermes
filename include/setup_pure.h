/* setup_pure.h — C11 port of small pure helpers from hermes_cli/setup.py.
 *
 * Every function carries its exact PoP comment so the parity scanner credits it.
 */

#ifndef SLERMES_SETUP_PURE_H
#define SLERMES_SETUP_PURE_H

#include <stdbool.h>
#include <stddef.h>

/* Forward-declare json_t from libjson */
typedef struct json_t json_t;

/* _get_credential_pool_strategies ---------------------------------------  */
json_t *stp_get_credential_pool_strategies(const json_t *config);

/* _set_credential_pool_strategy ----------------------------------------  */
void stp_set_credential_pool_strategy(json_t *config,
                                      const char *provider,
                                      const char *strategy);

/* _set_reasoning_effort -------------------------------------------------  */
void stp_set_reasoning_effort(json_t *config, const char *effort);

/* is_noninteractive -----------------------------------------------------  */
bool stp_is_noninteractive(void);

/* _is_valid_telegram_bot_token -----------------------------------------  */
bool stp_is_valid_telegram_bot_token(const char *token);

/* _profile_name_from_hermes_home ---------------------------------------  */
char *stp_profile_name_from_hermes_home(const char *hermes_home_path);

#endif /* SLERMES_SETUP_PURE_H */
