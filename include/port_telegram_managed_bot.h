/*
 * port_telegram_managed_bot.h -- pure helpers ported from
 * hermes_cli/telegram_managed_bot.py.
 *
 * The httpx onboarding calls stay in Python.
 */
#ifndef PORT_TELEGRAM_MANAGED_BOT_H
#define PORT_TELEGRAM_MANAGED_BOT_H

#include <stddef.h>

/* telegram_managed_bot.py:_api_url -- explicit override, then
 * $TELEGRAM_ONBOARDING_URL, then the production default; trailing slashes
 * stripped. env_value stands in for os.environ.get. Caller frees. */
char *tg_api_url(const char *api_url, const char *env_value);

/* telegram_managed_bot.py:is_valid_telegram_bot_token --
 * ^\d+:[A-Za-z0-9_-]{30,}$ */
int tg_is_valid_bot_token(const char *token);

/* telegram_managed_bot.py:_parse_owner_user_id -- kind is the Python type of
 * the incoming value: 0 = int, 1 = decimal string, 2 = bool, 3 = other.
 * Returns the parsed positive id, or 0 for None. */
long long tg_parse_owner_user_id(int kind, long long int_val, const char *str_val);

#endif /* PORT_TELEGRAM_MANAGED_BOT_H */
