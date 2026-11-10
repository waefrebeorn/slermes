/*
 * discord_http.h - Discord REST helpers (auth header, API call, errors).
 *
 * Module-internal header (discord.c <-> discord_http.c).
 */
#ifndef DISCORD_HTTP_H
#define DISCORD_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_tool_helpers.h"   /* tool_api_result_t */

tool_api_result_t * discord_api_call(const char *url, const char *method, const char *body, int timeout, int retries);
const char * get_token(void);
char * json_error(const char *msg);
char * json_errorf(const char *fmt, ...);

#endif /* DISCORD_HTTP_H */
