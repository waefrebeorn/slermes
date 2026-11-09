/*
 * port_telegram_managed_bot.c
 *
 * Pure helpers ported from hermes_cli/telegram_managed_bot.py - Telegram bot
 * onboarding.
 *
 * The module's real work is httpx calls to the onboarding API. What is
 * portable is the URL resolution and the token / owner-id shape checks:
 *
 *   tg_api_url             <- telegram_managed_bot.py:_api_url
 *   tg_is_valid_bot_token  <- telegram_managed_bot.py:is_valid_telegram_bot_token
 *   tg_parse_owner_user_id <- telegram_managed_bot.py:_parse_owner_user_id
 *
 * The env lookup is a parameter because a C body should not own os.environ.
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* telegram_managed_bot.py:TELEGRAM_ONBOARDING_URL_ENV */
#define TG_ONBOARDING_URL_ENV "TELEGRAM_ONBOARDING_URL"

/* telegram_managed_bot.py:DEFAULT_API_URL */
#define TG_DEFAULT_API_URL "https://setup.hermes-agent.nousresearch.com"

/* ===========================================================================
 *  _api_url  (telegram_managed_bot.py:_api_url)
 *
 *      (api_url or os.environ.get(TELEGRAM_ONBOARDING_URL_ENV)
 *       or DEFAULT_API_URL).rstrip("/")
 *
 *  Python truthiness again: an empty explicit value falls through to the
 *  environment, and an empty environment value falls through to the default.
 *  `env_value` stands in for os.environ.get. Caller frees.
 * =========================================================================== */

/* PoP: _api_url @ hermes_cli/telegram_managed_bot.py:_api_url */
char *tg_api_url(const char *api_url, const char *env_value)
{
    const char *chosen = (api_url && api_url[0]) ? api_url
                      : (env_value && env_value[0]) ? env_value
                      : TG_DEFAULT_API_URL;
    size_t len = strlen(chosen);
    while (len > 0 && chosen[len - 1] == '/') len--;

    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, chosen, len);
    out[len] = '\0';
    return out;
}

/* ===========================================================================
 *  is_valid_telegram_bot_token  (telegram_managed_bot.py:is_valid_telegram_bot_token)
 *
 *      isinstance(token, str) and bool(_TELEGRAM_BOT_TOKEN_RE.match(token))
 *
 *  _TELEGRAM_BOT_TOKEN_RE = ^\d+:[A-Za-z0-9_-]{30,}$
 *
 *  A non-string is simply False (Python's isinstance guard). Returns 1 when
 *  the shape matches.
 * =========================================================================== */

/* PoP: is_valid_telegram_bot_token @ hermes_cli/telegram_managed_bot.py:is_valid_telegram_bot_token */
int tg_is_valid_bot_token(const char *token)
{
    if (!token) return 0;

    const char *p = token;
    if (!isdigit((unsigned char)*p)) return 0;
    while (isdigit((unsigned char)*p)) p++;
    if (*p != ':') return 0;
    p++;

    size_t secret = 0;
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
        secret++;
    }
    return secret >= 30;
}

/* ===========================================================================
 *  _parse_owner_user_id  (telegram_managed_bot.py:_parse_owner_user_id)
 *
 *      if isinstance(value, bool):      return None
 *      if isinstance(value, str) and value.isdecimal(): value = int(value)
 *      return value if isinstance(value, int) and value > 0 else None
 *
 *  `kind` selects the Python type of the incoming value:
 *    0 = int (as_json=true), 1 = decimal string, 2 = bool, 3 = anything else.
 *  A bool is rejected BEFORE the int check because in Python bool is a
 *  subclass of int - True would otherwise parse as 1.
 *  Returns the parsed id, or 0 for None.
 * =========================================================================== */

/* PoP: _parse_owner_user_id @ hermes_cli/telegram_managed_bot.py:_parse_owner_user_id */
long long tg_parse_owner_user_id(int kind, long long int_val, const char *str_val)
{
    switch (kind) {
    case 0:   /* int */
        return int_val > 0 ? int_val : 0;
    case 1: { /* str: isdecimal() then int() */
        if (!str_val) return 0;
        const char *p = str_val;
        if (!*p) return 0;
        for (; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
        /* strtoll would accept leading whitespace/sign, which isdecimal() does not */
        long long v = 0;
        for (p = str_val; *p; p++) {
            if (v > 922337203685477580LL) return 0;   /* overflow guard */
            v = v * 10 + (*p - '0');
        }
        return v > 0 ? v : 0;
    }
    case 2:   /* bool: rejected outright */
    case 3:   /* anything else */
    default:
        return 0;
    }
}
